// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using test_utils::EqualsProto;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::WithArg;

namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr uint64 kNodeId = 7654321ULL;
// For Tofino, unit is the 0-based index of the node in the ChassisConfig.
constexpr int kUnit = 0;
constexpr int kSlot = 1;
constexpr int kPort = 1;
constexpr uint32 kPortId = 12345;
constexpr uint32 kSdkPortOffset = 900000;
constexpr uint64 kDefaultSpeedBps = kHundredGigBps;
constexpr FecMode kDefaultFecMode = FEC_MODE_UNKNOWN;
constexpr TriState kDefaultAutoneg = TRI_STATE_UNKNOWN;
constexpr LoopbackState kDefaultLoopbackMode = LOOPBACK_STATE_UNKNOWN;

// A helper class to build a single-node ChassisConfig message.
class ChassisConfigBuilder {
 public:
  explicit ChassisConfigBuilder(uint64 node_id = kNodeId) : node_id(node_id) {
    config_.set_description("Test config for BFChassisManager");
    auto* chassis = config_.mutable_chassis();
    chassis->set_platform(PLT_GENERIC_BAREFOOT_TOFINO);
    chassis->set_name("Tofino");

    auto* node = config_.add_nodes();
    node->set_id(node_id);
    node->set_slot(kSlot);
  }

  SingletonPort* AddPort(uint32 port_id, int32 port, AdminState admin_state,
                         uint64 speed_bps = kDefaultSpeedBps,
                         FecMode fec_mode = kDefaultFecMode,
                         TriState autoneg = kDefaultAutoneg,
                         LoopbackState loopback_mode = kDefaultLoopbackMode) {
    auto* sport = config_.add_singleton_ports();
    sport->set_id(port_id);
    sport->set_node(node_id);
    sport->set_port(port);
    sport->set_slot(kSlot);
    sport->set_channel(0);
    sport->set_speed_bps(speed_bps);
    sport->mutable_config_params()->set_admin_state(admin_state);
    sport->mutable_config_params()->set_fec_mode(fec_mode);
    sport->mutable_config_params()->set_autoneg(autoneg);
    sport->mutable_config_params()->set_loopback_mode(loopback_mode);
    return sport;
  }

  SingletonPort* GetPort(uint32 port_id) {
    for (auto& sport : *config_.mutable_singleton_ports()) {
      if (sport.id() == port_id) return &sport;
    }
    return nullptr;
  }

  void SetVendorConfig(const VendorConfig& vendor_config) {
    *config_.mutable_vendor_config() = vendor_config;
  }

  void RemoveLastPort() { config_.mutable_singleton_ports()->RemoveLast(); }

  const ChassisConfig& Get() const { return config_; }

 private:
  uint64 node_id;
  ChassisConfig config_;
};

}  // namespace

class BFChassisManagerTest : public ::testing::Test {
 protected:
  BFChassisManagerTest() {}

  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    // TODO(max): create parametrized test suite over mode.
    bf_chassis_manager_ = BFChassisManager::CreateInstance(
        OPERATION_MODE_STANDALONE, phal_mock_.get(), bf_sde_mock_.get());
    ON_CALL(*bf_sde_mock_, IsValidPort(_, _))
        .WillByDefault(
            WithArg<1>(Invoke([](uint32 id) { return id > kSdkPortOffset; })));
  }

  void RegisterSdkPortId(uint32 port_id, int slot, int port, int channel,
                         int device) {
    PortKey port_key(slot, port, channel);
    EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(device, port_key))
        .WillRepeatedly(Return(port_id + kSdkPortOffset));
  }

  void RegisterSdkPortId(const SingletonPort* singleton_port) {
    RegisterSdkPortId(singleton_port->id(), singleton_port->slot(),
                      singleton_port->port(), singleton_port->channel(),
                      kUnit);  // TODO(bocon): look up unit from node
  }

  ::util::Status CheckCleanInternalState() {
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->unit_to_node_id_.empty());
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->node_id_to_unit_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_port_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_port_config_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_singleton_port_key_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_sdk_port_id_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_sdk_port_id_to_port_id_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->xcvr_port_key_to_xcvr_state_.empty());
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->port_status_event_channel_ ==
                          nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_reader_ == nullptr);
    return ::util::OkStatus();
  }

  bool Initialized() { return bf_chassis_manager_->initialized_; }

  ::util::Status VerifyChassisConfig(const ChassisConfig& config) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bf_chassis_manager_->VerifyChassisConfig(config);
  }

  ::util::Status PushChassisConfig(const ChassisConfig& config) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->PushChassisConfig(config);
  }

  ::util::Status PushChassisConfig(const ChassisConfigBuilder& builder) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->PushChassisConfig(builder.Get());
  }

  ::util::Status PushBaseChassisConfig(ChassisConfigBuilder* builder) {
    CHECK_RETURN_IF_FALSE(!Initialized())
        << "Can only call PushBaseChassisConfig() for first ChassisConfig!";
    RegisterSdkPortId(builder->AddPort(kPortId, kPort, ADMIN_STATE_ENABLED));

    // RegisterPortStatusEventWriter called because this is the first call
    // to PushChassisConfig
    EXPECT_CALL(*bf_sde_mock_, RegisterPortStatusEventWriter(_));
    EXPECT_CALL(*bf_sde_mock_, AddPort(kUnit, kPortId + kSdkPortOffset,
                                       kDefaultSpeedBps, kDefaultFecMode));
    EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, kPortId + kSdkPortOffset));

    EXPECT_CALL(*phal_mock_,
                RegisterTransceiverEventWriter(
                    _, PhalInterface::kTransceiverEventWriterPriorityHigh))
        .WillOnce(Return(kTestTransceiverWriterId));
    EXPECT_CALL(*phal_mock_,
                UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
        .WillOnce(Return(::util::OkStatus()));

    RETURN_IF_ERROR(PushChassisConfig(builder->Get()));
    auto unit = GetUnitFromNodeId(kNodeId);
    CHECK_RETURN_IF_FALSE(unit.ok());
    CHECK_RETURN_IF_FALSE(unit.ValueOrDie() == kUnit) << "Invalid unit number!";
    CHECK_RETURN_IF_FALSE(Initialized())
        << "Class is not initialized after push!";
    return ::util::OkStatus();
  }

  ::util::Status ReplayPortsConfig(uint64 node_id) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->ReplayPortsConfig(node_id);
  }

  ::util::Status PushBaseChassisConfig() {
    ChassisConfigBuilder builder;
    return PushBaseChassisConfig(&builder);
  }

  ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bf_chassis_manager_->GetUnitFromNodeId(node_id);
  }

  ::util::Status Shutdown() { return bf_chassis_manager_->Shutdown(); }

  ::util::Status ShutdownAndTestCleanState() {
    EXPECT_CALL(*bf_sde_mock_, UnregisterPortStatusEventWriter())
        .WillOnce(Return(::util::OkStatus()));
    RETURN_IF_ERROR(Shutdown());
    RETURN_IF_ERROR(CheckCleanInternalState());
    return ::util::OkStatus();
  }

  std::unique_ptr<ChannelWriter<TransceiverEvent>> GetTransceiverEventWriter() {
    absl::WriterMutexLock l(&chassis_lock);
    CHECK(bf_chassis_manager_->xcvr_event_channel_ != nullptr)
        << "xcvr channel is null!";
    return ChannelWriter<PhalInterface::TransceiverEvent>::Create(
        bf_chassis_manager_->xcvr_event_channel_);
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<BFChassisManager> bf_chassis_manager_;

  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_F(BFChassisManagerTest, PreFirstConfigPushState) {
  ASSERT_OK(CheckCleanInternalState());
  EXPECT_FALSE(Initialized());
  // TODO(antonin): add more checks (to verify that method calls fail as
  // expected)
}

TEST_F(BFChassisManagerTest, FirstConfigPush) {
  ASSERT_OK(PushBaseChassisConfig());
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, RemovePort) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  builder.RemoveLastPort();
  EXPECT_CALL(*bf_sde_mock_, DeletePort(kUnit, kPortId + kSdkPortOffset));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, AddPortFec) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  const uint32 portId = kPortId + 1;
  const int port = kPort + 1;

  RegisterSdkPortId(builder.AddPort(portId, port, ADMIN_STATE_ENABLED,
                                    kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_, AddPort(kUnit, portId + kSdkPortOffset,
                                     kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, portId + kSdkPortOffset));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, SetPortLoopback) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  SingletonPort* sport = builder.GetPort(kPortId);
  sport->mutable_config_params()->set_loopback_mode(LOOPBACK_STATE_MAC);

  EXPECT_CALL(
      *bf_sde_mock_,
      SetPortLoopbackMode(kUnit, kPortId + kSdkPortOffset, LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, kPortId + kSdkPortOffset));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, ApplyPortShaping) {
  const std::string kVendorConfigText = R"PROTO(
    tofino_config {
      node_id_to_port_shaping_config {
        key: 7654321
        value {
          per_port_shaping_configs {
            key: 12345
            value {
              byte_shaping {
                max_rate_bps: 10000000000 # 10G
                max_burst_bytes: 16384 # 2x jumbo frame
              }
            }
          }
        }
      }
    }
  )PROTO";

  VendorConfig vendor_config;
  ASSERT_OK(ParseProtoFromString(kVendorConfigText, &vendor_config));

  ChassisConfigBuilder builder;
  builder.SetVendorConfig(vendor_config);
  ASSERT_OK(PushBaseChassisConfig(&builder));

  EXPECT_CALL(*bf_sde_mock_, SetPortShapingRate(kUnit, kPortId + kSdkPortOffset,
                                                false, 16384, kTenGigBps))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, EnablePortShaping(kUnit, kPortId + kSdkPortOffset,
                                               TRI_STATE_TRUE))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, kPortId + kSdkPortOffset))
      .Times(AtLeast(1));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, ReplayPorts) {
  ASSERT_OK(PushBaseChassisConfig());

  const uint32 sdkPortId = kPortId + kSdkPortOffset;
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kUnit, sdkPortId, kDefaultSpeedBps, kDefaultFecMode));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, sdkPortId));

  // For now, when replaying the port configuration, we set the mtu and autoneg
  // even if the values where already the defaults. This seems like a good idea
  // to ensure configuration consistency.
  EXPECT_CALL(*bf_sde_mock_, SetPortMtu(kUnit, sdkPortId, 0)).Times(AtMost(1));
  EXPECT_CALL(*bf_sde_mock_,
              SetPortAutonegPolicy(kUnit, sdkPortId, TRI_STATE_UNKNOWN))
      .Times(AtMost(1));

  EXPECT_OK(ReplayPortsConfig(kNodeId));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, TransceiverEvent) {
  ASSERT_OK(PushBaseChassisConfig());
  auto xcvr_event_writer = GetTransceiverEventWriter();

  EXPECT_CALL(*phal_mock_, GetFrontPanelPortInfo(kSlot, kPort, _));

  EXPECT_OK(
      xcvr_event_writer->Write(TransceiverEvent{kSlot, kPort, HW_STATE_PRESENT},
                               absl::InfiniteDuration()));
  // Make sure the event reader reads the event and make expected calls to
  // phal mock interface.
  absl::SleepFor(absl::Milliseconds(1000));

  ASSERT_OK(ShutdownAndTestCleanState());
}

template <typename T>
T GetPortData(BFChassisManager* bf_chassis_manager_, uint64 node_id,
              int port_id,
              DataRequest::Request::Port* (
                  DataRequest::Request::*get_mutable_message_func)(),
              const T& (DataResponse::*data_response_get_message_func)() const,
              bool (DataResponse::*data_response_has_message_func)() const) {
  DataRequest::Request req;
  (req.*get_mutable_message_func)()->set_node_id(node_id);
  (req.*get_mutable_message_func)()->set_port_id(port_id);
  auto resp = bf_chassis_manager_->GetPortData(req);
  EXPECT_OK(resp);

  DataResponse data_resp = resp.ValueOrDie();
  EXPECT_TRUE((data_resp.*data_response_has_message_func)());
  return (data_resp.*data_response_get_message_func)();
}

template <typename T, typename U>
void GetPortDataTest(BFChassisManager* bf_chassis_manager_, uint64 node_id,
                     int port_id,
                     DataRequest::Request::Port* (
                         DataRequest::Request::*get_mutable_message_func)(),
                     const T& (DataResponse::*data_response_get_message_func)()
                         const,
                     bool (DataResponse::*data_response_has_message_func)()
                         const,
                     U (T::*get_inner_message_func)() const, U expected_value) {
  const T& val = GetPortData(
      bf_chassis_manager_, node_id, port_id, get_mutable_message_func,
      data_response_get_message_func, data_response_has_message_func);
  EXPECT_EQ((val.*get_inner_message_func)(), expected_value);
}

template <typename T>
void GetPortDataTest(BFChassisManager* bf_chassis_manager_, uint64 node_id,
                     int port_id,
                     DataRequest::Request::Port* (
                         DataRequest::Request::*get_mutable_message_func)(),
                     const T& (DataResponse::*data_response_get_message_func)()
                         const,
                     bool (DataResponse::*data_response_has_message_func)()
                         const,
                     T expected_msg) {
  T val = GetPortData(bf_chassis_manager_, node_id, port_id,
                      get_mutable_message_func, data_response_get_message_func,
                      data_response_has_message_func);
  EXPECT_THAT(val, EqualsProto(expected_msg));
}

TEST_F(BFChassisManagerTest, GetPortData) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  const uint32 portId = kPortId + 1;
  const uint32 sdkPortId = portId + kSdkPortOffset;
  const int port = kPort + 1;

  RegisterSdkPortId(builder.AddPort(portId, port, ADMIN_STATE_ENABLED,
                                    kHundredGigBps, FEC_MODE_ON, TRI_STATE_TRUE,
                                    LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kUnit, sdkPortId, kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_,
              SetPortLoopbackMode(kUnit, sdkPortId, LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, sdkPortId));
  EXPECT_CALL(*bf_sde_mock_, GetPortState(kUnit, sdkPortId))
      .WillRepeatedly(Return(PORT_STATE_UP));

  PortCounters counters;
  counters.set_in_octets(1);
  counters.set_out_octets(2);
  counters.set_in_unicast_pkts(3);
  counters.set_out_unicast_pkts(4);
  counters.set_in_broadcast_pkts(5);
  counters.set_out_broadcast_pkts(6);
  counters.set_in_multicast_pkts(7);
  counters.set_out_multicast_pkts(8);
  counters.set_in_discards(9);
  counters.set_out_discards(10);
  counters.set_in_unknown_protos(11);
  counters.set_in_errors(12);
  counters.set_out_errors(13);
  counters.set_in_fcs_errors(14);

  EXPECT_CALL(*bf_sde_mock_, GetPortCounters(kUnit, sdkPortId, _))
      .WillOnce(DoAll(SetArgPointee<2>(counters), Return(::util::OkStatus())));

  FrontPanelPortInfo front_panel_port_info;
  front_panel_port_info.set_physical_port_type(PHYSICAL_PORT_TYPE_QSFP_CAGE);
  front_panel_port_info.set_media_type(MEDIA_TYPE_QSFP_COPPER);
  front_panel_port_info.set_vendor_name("dummy");
  front_panel_port_info.set_part_number("000");
  front_panel_port_info.set_serial_number("000");
  front_panel_port_info.set_hw_state(HW_STATE_PRESENT);
  EXPECT_CALL(*phal_mock_, GetFrontPanelPortInfo(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(front_panel_port_info),
                      Return(::util::OkStatus())));

  ON_CALL(*bf_sde_mock_, SetPortAutonegPolicy(_, _, _))
      .WillByDefault(Return(::util::OkStatus()));
  ON_CALL(*bf_sde_mock_, IsValidPort(_, _))
      .WillByDefault(
          WithArg<1>(Invoke([](uint32 id) { return id > kSdkPortOffset; })));
  ASSERT_OK(PushChassisConfig(builder));

  // Operation status
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_oper_status,
                  &DataResponse::oper_status, &DataResponse::has_oper_status,
                  &OperStatus::state, PORT_STATE_UP);

  // Admin status
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_admin_status,
                  &DataResponse::admin_status, &DataResponse::has_admin_status,
                  &AdminStatus::state, ADMIN_STATE_ENABLED);

  // Port speed
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_port_speed,
                  &DataResponse::port_speed, &DataResponse::has_port_speed,
                  &PortSpeed::speed_bps, kHundredGigBps);

  // Negotiated port speed
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_negotiated_port_speed,
                  &DataResponse::negotiated_port_speed,
                  &DataResponse::has_negotiated_port_speed,
                  &PortSpeed::speed_bps, kHundredGigBps);

  // Port counters
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_port_counters,
                  &DataResponse::port_counters,
                  &DataResponse::has_port_counters, counters);

  // Autoneg status
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_autoneg_status,
                  &DataResponse::autoneg_status,
                  &DataResponse::has_autoneg_status,
                  &AutonegotiationStatus::state, TRI_STATE_TRUE);

  // Front panel info
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_front_panel_port_info,
                  &DataResponse::front_panel_port_info,
                  &DataResponse::has_front_panel_port_info,
                  front_panel_port_info);

  // FEC status
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_fec_status,
                  &DataResponse::fec_status, &DataResponse::has_fec_status,
                  &FecStatus::mode, FEC_MODE_ON);

  // Loopback mode
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_loopback_status,
                  &DataResponse::loopback_status,
                  &DataResponse::has_loopback_status, &LoopbackStatus::state,
                  LOOPBACK_STATE_MAC);

  // SDK port number
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_sdn_port_id,
                  &DataResponse::sdn_port_id, &DataResponse::has_sdn_port_id,
                  &SdnPortId::port_id, sdkPortId);

  // Forwarding Viability
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_forwarding_viability,
                  &DataResponse::forwarding_viability,
                  &DataResponse::has_forwarding_viability,
                  &ForwardingViability::state,
                  TRUNK_MEMBER_BLOCK_STATE_UNKNOWN);

  // Health Indicator
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_health_indicator,
                  &DataResponse::health_indicator,
                  &DataResponse::has_health_indicator, &HealthIndicator::state,
                  HEALTH_STATE_UNKNOWN);

  // Unsupported
  DataRequest::Request req;
  req.mutable_lacp_router_mac()->set_node_id(kNodeId);
  req.mutable_lacp_router_mac()->set_port_id(portId);
  auto status = bf_chassis_manager_->GetPortData(req);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.status().error_code(), ERR_INTERNAL);
  EXPECT_THAT(status.status().error_message(), HasSubstr("Not supported yet"));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, UpdateInvalidPort) {
  ASSERT_OK(PushBaseChassisConfig());
  ChassisConfigBuilder builder;
  const uint32 portId = kPortId + 1;
  const uint32 sdkPortId = portId + kSdkPortOffset;
  SingletonPort* new_port =
      builder.AddPort(portId, kPort + 1, ADMIN_STATE_ENABLED);
  RegisterSdkPortId(new_port);
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kUnit, sdkPortId, kDefaultSpeedBps, FEC_MODE_UNKNOWN))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kUnit, sdkPortId))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK(PushChassisConfig(builder));

  EXPECT_CALL(*bf_sde_mock_, IsValidPort(kUnit, sdkPortId))
      .WillOnce(Return(false));

  // Update port, but port is invalid.
  new_port->set_speed_bps(10000000000ULL);
  auto status = PushChassisConfig(builder);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), ERR_INTERNAL);
  std::stringstream err_msg;
  err_msg << "Port " << portId << " in node " << kNodeId
          << " is not valid (SDK Port " << sdkPortId << ").";
  EXPECT_THAT(status.error_message(), HasSubstr(err_msg.str()));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, ResetPortsConfig) {
  ASSERT_OK(PushBaseChassisConfig());
  EXPECT_OK(bf_chassis_manager_->ResetPortsConfig(kNodeId));

  // Invalid node ID
  auto status = bf_chassis_manager_->ResetPortsConfig(kNodeId + 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), ERR_INVALID_PARAM);
  std::stringstream err_msg;
  err_msg << "Node " << kNodeId + 1 << " is not configured or not known.";
  EXPECT_THAT(status.error_message(), HasSubstr(err_msg.str()));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, GetSdkPortId) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  SingletonPort* sport = builder.GetPort(kPortId);
  auto resp = bf_chassis_manager_->GetSdkPortId(sport->node(), kPortId);
  EXPECT_OK(resp);
  EXPECT_EQ(resp.ValueOrDie(), kPortId + kSdkPortOffset);

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, VerifyChassisConfigSuccess) {
  const std::string kConfigText1 = R"(
      description: "Sample Generic Tofino config 2x25G ports."
      chassis {
        platform: PLT_GENERIC_BAREFOOT_TOFINO
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 1
        channel: 1
        speed_bps: 25000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
      singleton_ports {
        id: 2
        slot: 1
        port: 1
        channel: 2
        speed_bps: 25000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  ChassisConfig config1;
  ASSERT_OK(ParseProtoFromString(kConfigText1, &config1));

  EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kUnit, PortKey(1, 1, 1)))
      .WillRepeatedly(Return(1 + kSdkPortOffset));
  EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kUnit, PortKey(1, 1, 2)))
      .WillRepeatedly(Return(2 + kSdkPortOffset));

  ASSERT_OK(VerifyChassisConfig(config1));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
