// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "absl/synchronization/notification.h"
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
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using PortStatusEvent = BfSdeInterface::PortStatusEvent;
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
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::WithArg;

namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr uint64 kNodeId = 7654321ULL;
constexpr int kDevice = 0;
constexpr int kSlot = 1;
constexpr int kPort = 1;
constexpr uint32 kPortId = 12345;
constexpr uint32 kSdkPortOffset = 900000;
constexpr uint64 kDefaultSpeedBps = kHundredGigBps;
constexpr FecMode kDefaultFecMode = FEC_MODE_UNKNOWN;
constexpr TriState kDefaultAutoneg = TRI_STATE_UNKNOWN;
constexpr LoopbackState kDefaultLoopbackMode = LOOPBACK_STATE_UNKNOWN;

MATCHER_P(GnmiEventEq, event, "") {
  if (absl::StrContains(typeid(*event).name(), "PortOperStateChangedEvent")) {
    const auto& cast_event =
        static_cast<const PortOperStateChangedEvent&>(*event);
    const auto& cast_arg = static_cast<const PortOperStateChangedEvent&>(*arg);
    return cast_event.GetPortId() == cast_arg.GetPortId() &&
           cast_event.GetNodeId() == cast_arg.GetNodeId() &&
           cast_event.GetNewState() == cast_arg.GetNewState() &&
           cast_event.GetTimeLastChanged() == cast_arg.GetTimeLastChanged();
  }
  return false;
}

// A helper class to build a single-node ChassisConfig message.
class ChassisConfigBuilder {
 public:
  explicit ChassisConfigBuilder(uint64 node_id = kNodeId) : node_id(node_id) {
    config_.set_description("Test config for BfChassisManager");
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

class BfChassisManagerTest : public ::testing::Test {
 protected:
  BfChassisManagerTest() {}

  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    bf_sde_mock_ = absl::make_unique<BfSdeMock>();
    // TODO(max): create parametrized test suite over mode.
    bf_chassis_manager_ = BfChassisManager::CreateInstance(
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
                      kDevice);  // TODO(bocon): look up device from node
  }

  ::util::Status CheckCleanInternalState() {
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->device_to_node_id_.empty());
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->node_id_to_device_.empty());
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

    // Save the SDE channel writer to trigger port events with it later.
    EXPECT_CALL(*bf_sde_mock_, RegisterPortStatusEventWriter(_))
        .WillOnce([this](std::unique_ptr<ChannelWriter<PortStatusEvent>> arg0) {
          sde_event_writer_ = std::move(arg0);
          return ::util::OkStatus();
        });

    EXPECT_CALL(*bf_sde_mock_, AddPort(kDevice, kPortId + kSdkPortOffset,
                                       kDefaultSpeedBps, kDefaultFecMode));
    EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, kPortId + kSdkPortOffset));

    EXPECT_CALL(*phal_mock_,
                RegisterTransceiverEventWriter(
                    _, PhalInterface::kTransceiverEventWriterPriorityHigh))
        .WillOnce(Return(kTestTransceiverWriterId));
    EXPECT_CALL(*phal_mock_,
                UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
        .WillOnce(Return(::util::OkStatus()));

    RETURN_IF_ERROR(PushChassisConfig(builder->Get()));
    auto device = GetDeviceFromNodeId(kNodeId);
    CHECK_RETURN_IF_FALSE(device.ok());
    CHECK_RETURN_IF_FALSE(device.ValueOrDie() == kDevice)
        << "Invalid device number!";
    CHECK_RETURN_IF_FALSE(Initialized())
        << "Class is not initialized after push!";
    return ::util::OkStatus();
  }

  ::util::Status ReplayChassisConfig(uint64 node_id) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->ReplayChassisConfig(node_id);
  }

  ::util::Status PushBaseChassisConfig() {
    ChassisConfigBuilder builder;
    return PushBaseChassisConfig(&builder);
  }

  ::util::StatusOr<int> GetDeviceFromNodeId(uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bf_chassis_manager_->GetDeviceFromNodeId(node_id);
  }

  ::util::Status Shutdown() { return bf_chassis_manager_->Shutdown(); }

  ::util::Status ShutdownAndTestCleanState() {
    EXPECT_CALL(*bf_sde_mock_, UnregisterPortStatusEventWriter())
        .WillOnce(Return(::util::OkStatus()));
    RETURN_IF_ERROR(Shutdown());
    RETURN_IF_ERROR(CheckCleanInternalState());
    return ::util::OkStatus();
  }

  ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
    return bf_chassis_manager_->RegisterEventNotifyWriter(writer);
  }

  ::util::Status UnregisterEventNotifyWriter() {
    return bf_chassis_manager_->UnregisterEventNotifyWriter();
  }

  std::unique_ptr<ChannelWriter<TransceiverEvent>> GetTransceiverEventWriter() {
    absl::WriterMutexLock l(&chassis_lock);
    CHECK(bf_chassis_manager_->xcvr_event_channel_ != nullptr)
        << "xcvr channel is null!";
    return ChannelWriter<PhalInterface::TransceiverEvent>::Create(
        bf_chassis_manager_->xcvr_event_channel_);
  }

  void TriggerPortStatusEvent(int device, int port, PortState state,
                              absl::Time time_last_changed) {
    PortStatusEvent event;
    event.device = device;
    event.port = port;
    event.state = state;
    event.time_last_changed = time_last_changed;
    ASSERT_OK(sde_event_writer_->Write(event, absl::Seconds(1)));
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BfSdeMock> bf_sde_mock_;
  std::unique_ptr<ChannelWriter<PortStatusEvent>> sde_event_writer_;
  std::unique_ptr<BfChassisManager> bf_chassis_manager_;

  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_F(BfChassisManagerTest, PreFirstConfigPushState) {
  ASSERT_OK(CheckCleanInternalState());
  EXPECT_FALSE(Initialized());
  // TODO(antonin): add more checks (to verify that method calls fail as
  // expected)
}

TEST_F(BfChassisManagerTest, FirstConfigPush) {
  ASSERT_OK(PushBaseChassisConfig());
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, RemovePort) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  builder.RemoveLastPort();
  EXPECT_CALL(*bf_sde_mock_, DeletePort(kDevice, kPortId + kSdkPortOffset));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, AddPortFec) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  const uint32 portId = kPortId + 1;
  const int port = kPort + 1;

  RegisterSdkPortId(builder.AddPort(portId, port, ADMIN_STATE_ENABLED,
                                    kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_, AddPort(kDevice, portId + kSdkPortOffset,
                                     kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, portId + kSdkPortOffset));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, SetPortLoopback) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  SingletonPort* sport = builder.GetPort(kPortId);
  sport->mutable_config_params()->set_loopback_mode(LOOPBACK_STATE_MAC);

  EXPECT_CALL(*bf_sde_mock_,
              SetPortLoopbackMode(kDevice, kPortId + kSdkPortOffset,
                                  LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, kPortId + kSdkPortOffset));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, ApplyPortShaping) {
  const std::string kVendorConfigText = R"PROTO(
    tofino_config {
      node_id_to_port_shaping_config {
        key: 7654321
        value {
          per_port_shaping_configs {
            key: 12345
            value {
              byte_shaping {
                rate_bps: 10000000000 # 10G
                burst_bytes: 16384 # 2x jumbo frame
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

  EXPECT_CALL(*bf_sde_mock_,
              SetPortShapingRate(kDevice, kPortId + kSdkPortOffset, false,
                                 16384, kTenGigBps))
      .Times(AtLeast(1));
  EXPECT_CALL(
      *bf_sde_mock_,
      EnablePortShaping(kDevice, kPortId + kSdkPortOffset, TRI_STATE_TRUE))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, kPortId + kSdkPortOffset))
      .Times(AtLeast(1));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, ApplyDeflectOnDrop) {
  const std::string kVendorConfigText = R"PROTO(
    tofino_config {
      node_id_to_deflect_on_drop_configs {
        key: 7654321
        value {
          drop_targets {
            port: 12345
            queue: 4
          }
          drop_targets {
            sdk_port: 56789
            queue: 1
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

  EXPECT_CALL(*bf_sde_mock_,
              SetDeflectOnDropDestination(kDevice, kPortId + kSdkPortOffset, 4))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, SetDeflectOnDropDestination(kDevice, 56789, 1))
      .Times(AtLeast(1));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, ApplyQoSConfig) {
  const std::string kVendorConfigText = R"PROTO(
    tofino_config {
      node_id_to_qos_config {
        key: 1
        value {
          pool_configs {
            pool: INGRESS_APP_POOL_0
            pool_size: 30000
            enable_color_drop: false
          }
          ppg_configs {
            sdk_port: 260
            is_default_ppg: true
            minimum_guaranteed_cells: 200
            pool: INGRESS_APP_POOL_0
            base_use_limit: 400
            baf: BAF_80_PERCENT
            hysteresis: 50
            ingress_drop_limit: 4000
            icos_bitmap: 0xfd
          }
          queue_configs {
            sdk_port: 260
            queue_mapping {
              queue_id: 0
              priority: PRIO_0
              weight: 1
              minimum_guaranteed_cells: 100
              pool: EGRESS_APP_POOL_0
              base_use_limit: 200
              baf: BAF_80_PERCENT
              hysteresis: 50
              max_rate_bytes {
                rate_bps: 100000000
                burst_bytes: 9000
              }
              min_rate_bytes {
                rate_bps: 1000000
                burst_bytes: 4500
              }
            }
          }
        }
      }
    }
  )PROTO";

  VendorConfig vendor_config;
  ASSERT_OK(ParseProtoFromString(kVendorConfigText, &vendor_config));
  const TofinoConfig::TofinoQosConfig& qos_config =
      vendor_config.tofino_config().node_id_to_qos_config().at(1);

  ChassisConfigBuilder builder;
  builder.SetVendorConfig(vendor_config);
  ASSERT_OK(PushBaseChassisConfig(&builder));

  EXPECT_CALL(*bf_sde_mock_, ConfigureQos(kDevice, EqualsProto(qos_config)))
      .Times(AtLeast(1));

  ASSERT_OK(PushChassisConfig(builder));
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, ReplayPorts) {
  const std::string kVendorConfigText = R"PROTO(
    tofino_config {
      node_id_to_deflect_on_drop_configs {
        key: 7654321
        value {
          drop_targets {
            port: 12345
            queue: 4
          }
          drop_targets {
            sdk_port: 56789
            queue: 1
          }
        }
      }
      node_id_to_port_shaping_config {
        key: 7654321
        value {
          per_port_shaping_configs {
            key: 12345
            value {
              byte_shaping {
                rate_bps: 10000000000
                burst_bytes: 16384
              }
            }
          }
        }
      }
    }
  )PROTO";

  constexpr int kCpuPort = 64;

  VendorConfig vendor_config;
  ASSERT_OK(ParseProtoFromString(kVendorConfigText, &vendor_config));

  ChassisConfigBuilder builder;
  builder.SetVendorConfig(vendor_config);
  ASSERT_OK(PushBaseChassisConfig(&builder));

  const uint32 sdkPortId = kPortId + kSdkPortOffset;
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kDevice, sdkPortId, kDefaultSpeedBps, kDefaultFecMode));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, sdkPortId));

  // For now, when replaying the port configuration, we set the mtu and autoneg
  // even if the values where already the defaults. This seems like a good idea
  // to ensure configuration consistency.
  EXPECT_CALL(*bf_sde_mock_, SetPortMtu(kDevice, sdkPortId, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_,
              SetPortAutonegPolicy(kDevice, sdkPortId, TRI_STATE_UNKNOWN))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, SetDeflectOnDropDestination(kDevice, sdkPortId, 4))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, SetDeflectOnDropDestination(kDevice, 56789, 1))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_,
              SetPortShapingRate(kDevice, sdkPortId, false, 16384, kTenGigBps))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_,
              EnablePortShaping(kDevice, sdkPortId, TRI_STATE_TRUE))
      .Times(AtLeast(1));
  EXPECT_CALL(*bf_sde_mock_, GetPcieCpuPort(kDevice))
      .WillRepeatedly(Return(kCpuPort));
  EXPECT_CALL(*bf_sde_mock_, SetTmCpuPort(kDevice, kCpuPort))
      .WillRepeatedly(Return(::util::OkStatus()));

  EXPECT_OK(ReplayChassisConfig(kNodeId));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, TransceiverEvent) {
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
T GetPortData(BfChassisManager* bf_chassis_manager_, uint64 node_id,
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
void GetPortDataTest(BfChassisManager* bf_chassis_manager_, uint64 node_id,
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
void GetPortDataTest(BfChassisManager* bf_chassis_manager_, uint64 node_id,
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

TEST_F(BfChassisManagerTest, GetPortData) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  const uint32 portId = kPortId + 1;
  const uint32 sdkPortId = portId + kSdkPortOffset;
  const int port = kPort + 1;
  constexpr absl::Time kPortTimeLastChanged1 = absl::FromUnixSeconds(1234);
  constexpr absl::Time kPortTimeLastChanged2 = absl::FromUnixSeconds(5678);
  constexpr absl::Time kPortTimeLastChanged3 = absl::FromUnixSeconds(9012);

  RegisterSdkPortId(builder.AddPort(portId, port, ADMIN_STATE_ENABLED,
                                    kHundredGigBps, FEC_MODE_ON, TRI_STATE_TRUE,
                                    LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kDevice, sdkPortId, kHundredGigBps, FEC_MODE_ON));
  EXPECT_CALL(*bf_sde_mock_,
              SetPortLoopbackMode(kDevice, sdkPortId, LOOPBACK_STATE_MAC));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, sdkPortId));
  EXPECT_CALL(*bf_sde_mock_, GetPortState(kDevice, sdkPortId))
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

  EXPECT_CALL(*bf_sde_mock_, GetPortCounters(kDevice, sdkPortId, _))
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

  // WriterInterface for reporting gNMI events.
  auto gnmi_event_writer = std::make_shared<WriterMock<GnmiEventPtr>>();
  GnmiEventPtr link_up(
      new PortOperStateChangedEvent(kNodeId, portId, PORT_STATE_UP,
                                    absl::ToUnixNanos(kPortTimeLastChanged1)));
  GnmiEventPtr link_down(
      new PortOperStateChangedEvent(kNodeId, portId, PORT_STATE_DOWN,
                                    absl::ToUnixNanos(kPortTimeLastChanged2)));
  GnmiEventPtr link_up_again(
      new PortOperStateChangedEvent(kNodeId, portId, PORT_STATE_UP,
                                    absl::ToUnixNanos(kPortTimeLastChanged3)));
  absl::Notification first_link_up;
  EXPECT_CALL(*gnmi_event_writer,
              Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(link_up))))
      .WillOnce(
          DoAll([&first_link_up] { first_link_up.Notify(); }, Return(true)));
  EXPECT_CALL(*gnmi_event_writer,
              Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(link_down))))
      .WillOnce(Return(true));
  absl::Notification port_flip_done;
  EXPECT_CALL(*gnmi_event_writer,
              Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(link_up_again))))
      .WillOnce(
          DoAll([&port_flip_done] { port_flip_done.Notify(); }, Return(true)));

  ASSERT_OK(PushChassisConfig(builder));

  // Register gNMI event writer.
  EXPECT_OK(RegisterEventNotifyWriter(gnmi_event_writer));

  // Operation status.
  // Emulate a few port status events.
  TriggerPortStatusEvent(kDevice, sdkPortId, PORT_STATE_UP,
                         kPortTimeLastChanged1);
  TriggerPortStatusEvent(kDevice, 12, PORT_STATE_UP,
                         kPortTimeLastChanged1);  // Unknown port
  TriggerPortStatusEvent(456, sdkPortId, PORT_STATE_UP,
                         kPortTimeLastChanged1);  // Unknown device
  ASSERT_TRUE(first_link_up.WaitForNotificationWithTimeout(absl::Seconds(5)));
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_oper_status,
                  &DataResponse::oper_status, &DataResponse::has_oper_status,
                  &OperStatus::state, PORT_STATE_UP);

  // Time last changed.
  // Check by simulating a port flip.
  TriggerPortStatusEvent(kDevice, sdkPortId, PORT_STATE_DOWN,
                         kPortTimeLastChanged2);
  TriggerPortStatusEvent(kDevice, sdkPortId, PORT_STATE_UP,
                         kPortTimeLastChanged3);
  ASSERT_TRUE(port_flip_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
  OperStatus oper_status =
      GetPortData(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_oper_status,
                  &DataResponse::oper_status, &DataResponse::has_oper_status);
  EXPECT_EQ(kPortTimeLastChanged3,
            absl::FromUnixNanos(oper_status.time_last_changed()));

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

  // LACP router MAC
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_lacp_router_mac,
                  &DataResponse::lacp_router_mac,
                  &DataResponse::has_lacp_router_mac, &MacAddress::mac_address,
                  0x112233445566ul);

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

  // SDN port ID
  GetPortDataTest(bf_chassis_manager_.get(), kNodeId, portId,
                  &DataRequest::Request::mutable_sdn_port_id,
                  &DataResponse::sdn_port_id, &DataResponse::has_sdn_port_id,
                  &SdnPortId::port_id, sdkPortId);

  ASSERT_OK(UnregisterEventNotifyWriter());
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BfChassisManagerTest, UpdateInvalidPort) {
  ASSERT_OK(PushBaseChassisConfig());
  ChassisConfigBuilder builder;
  const uint32 portId = kPortId + 1;
  const uint32 sdkPortId = portId + kSdkPortOffset;
  SingletonPort* new_port =
      builder.AddPort(portId, kPort + 1, ADMIN_STATE_ENABLED);
  RegisterSdkPortId(new_port);
  EXPECT_CALL(*bf_sde_mock_,
              AddPort(kDevice, sdkPortId, kDefaultSpeedBps, FEC_MODE_UNKNOWN))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_sde_mock_, EnablePort(kDevice, sdkPortId))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK(PushChassisConfig(builder));

  EXPECT_CALL(*bf_sde_mock_, IsValidPort(kDevice, sdkPortId))
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

TEST_F(BfChassisManagerTest, VerifyChassisConfigSuccess) {
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

  EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kDevice, PortKey(1, 1, 1)))
      .WillRepeatedly(Return(1 + kSdkPortOffset));
  EXPECT_CALL(*bf_sde_mock_, GetPortIdFromPortKey(kDevice, PortKey(1, 1, 2)))
      .WillRepeatedly(Return(2 + kSdkPortOffset));

  ASSERT_OK(VerifyChassisConfig(config1));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
