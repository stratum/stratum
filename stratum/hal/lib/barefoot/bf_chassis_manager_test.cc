// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_pal_mock.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/test_utils/matchers.h"

using ::stratum::test_utils::EqualsProto;
using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace stratum {
namespace hal {
namespace barefoot {

namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr uint64 kNodeId = 7654321ULL;
// For Tofino, unit is the 0-based index of the node in the ChassisConfig.
constexpr int kUnit = 0;
constexpr int kSlot = 1;
constexpr int kPort = 1;
constexpr uint32 kPortId = 12345;
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
    chassis->set_platform(PLT_BAREFOOT_TOFINO);
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
    bf_pal_mock_ = absl::make_unique<BFPalMock>();
    bf_chassis_manager_ =
        BFChassisManager::CreateInstance(phal_mock_.get(), bf_pal_mock_.get());
    ON_CALL(*bf_pal_mock_, PortIsValid(_, _)).WillByDefault(Return(true));
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
        bf_chassis_manager_->xcvr_port_key_to_xcvr_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->port_status_change_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_reader_ == nullptr);
    return ::util::OkStatus();
  }

  bool Initialized() { return bf_chassis_manager_->initialized_; }

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
    builder->AddPort(kPortId, kPort, ADMIN_STATE_ENABLED);

    // PortStatusChangeRegisterEventWriter called because this is the first call
    // to PushChassisConfig
    EXPECT_CALL(*bf_pal_mock_, PortStatusChangeRegisterEventWriter(_));
    EXPECT_CALL(*bf_pal_mock_,
                PortAdd(kUnit, kPortId, kDefaultSpeedBps, kDefaultFecMode));
    EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId));

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
    EXPECT_CALL(*bf_pal_mock_, PortStatusChangeUnregisterEventWriter())
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
  std::unique_ptr<BFPalMock> bf_pal_mock_;
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
  EXPECT_CALL(*bf_pal_mock_, PortDelete(kUnit, kPortId));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, AddPortFec) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  auto portId = kPortId + 1;
  auto port = kPort + 1;

  builder.AddPort(portId, port, ADMIN_STATE_ENABLED, kHundredGigBps,
                  FEC_MODE_ON);
  EXPECT_CALL(*bf_pal_mock_,
              PortAdd(kUnit, portId, kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, portId));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, SetPortLoopback) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  SingletonPort* sport = builder.GetPort(kPortId);
  sport->mutable_config_params()->set_loopback_mode(LOOPBACK_STATE_MAC);

  EXPECT_CALL(*bf_pal_mock_,
              PortLoopbackModeSet(kUnit, kPortId, LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, ReplayPorts) {
  ASSERT_OK(PushBaseChassisConfig());

  EXPECT_CALL(*bf_pal_mock_,
              PortAdd(kUnit, kPortId, kDefaultSpeedBps, kDefaultFecMode));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId));

  // For now, when replaying the port configuration, we set the mtu and autoneg
  // even if the values where already the defaults. This seems like a good idea
  // to ensure configuration consistency.
  EXPECT_CALL(*bf_pal_mock_, PortMtuSet(kUnit, kPortId, 0)).Times(AtMost(1));
  EXPECT_CALL(*bf_pal_mock_,
              PortAutonegPolicySet(kUnit, kPortId, TRI_STATE_UNKNOWN))
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

  auto portId = kPortId + 1;
  auto port = kPort + 1;

  builder.AddPort(portId, port, ADMIN_STATE_ENABLED, kHundredGigBps,
                  FEC_MODE_ON, TRI_STATE_TRUE, LOOPBACK_STATE_MAC);
  EXPECT_CALL(*bf_pal_mock_,
              PortAdd(kUnit, portId, kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_pal_mock_,
              PortLoopbackModeSet(kUnit, portId, LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, portId));
  EXPECT_CALL(*bf_pal_mock_, PortOperStateGet(kUnit, portId))
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

  EXPECT_CALL(*bf_pal_mock_, PortAllStatsGet(kUnit, portId, _))
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

  ON_CALL(*bf_pal_mock_, PortAutonegPolicySet(_, _, _))
      .WillByDefault(Return(::util::OkStatus()));
  ON_CALL(*bf_pal_mock_, PortIsValid(_, _)).WillByDefault(Return(true));
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

  // Unsupprorted
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
  SingletonPort* new_port =
      builder.AddPort(kPortId + 1, kPort + 1, ADMIN_STATE_ENABLED);
  EXPECT_CALL(*bf_pal_mock_,
              PortAdd(kUnit, kPortId + 1, kDefaultSpeedBps, FEC_MODE_UNKNOWN))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId + 1))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK(PushChassisConfig(builder));

  EXPECT_CALL(*bf_pal_mock_, PortIsValid(kUnit, kPortId + 1))
      .WillOnce(Return(false));

  // Update port, but port is invalid.
  new_port->set_speed_bps(10000000000ULL);
  auto status = PushChassisConfig(builder);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), ERR_INTERNAL);
  std::stringstream err_msg;
  err_msg << "Port " << kPortId + 1 << " in node " << kNodeId
          << " is not valid.";
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
