// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

#include <memory>
#include <string>
#include <tuple>
#include <typeinfo>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_node_mock.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager_mock.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/channel/channel_mock.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DECLARE_string(base_bcm_chassis_map_file);
DECLARE_string(applied_bcm_chassis_map_file);
DECLARE_string(bcm_sdk_config_file);
DECLARE_string(bcm_sdk_config_flush_file);
DECLARE_string(bcm_sdk_shell_log_file);
DECLARE_string(bcm_sdk_checkpoint_dir);
DECLARE_string(test_tmpdir);

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace stratum {
namespace hal {
namespace bcm {

static constexpr uint64 kNodeId = 7654321ULL;
static constexpr uint32 kPortId = 12345;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

MATCHER_P(GnmiEventEq, event, "") {
  if (absl::StrContains(typeid(*event).name(), "PortOperStateChangedEvent")) {
    const auto& cast_event =
        static_cast<const PortOperStateChangedEvent&>(*event);
    const auto& cast_arg = static_cast<const PortOperStateChangedEvent&>(*arg);
    return cast_event.GetPortId() == cast_arg.GetPortId() &&
           cast_event.GetNodeId() == cast_arg.GetNodeId() &&
           cast_event.GetNewState() == cast_arg.GetNewState();
  }
  return false;
}

// TODO(unknown): Investigate moving the test protos into a testdata folder.
// TODO(unknown): Use constants for the config args used in tests.
class BcmChassisManagerTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  BcmChassisManagerTest() {
    FLAGS_base_bcm_chassis_map_file =
        FLAGS_test_tmpdir + "/base_bcm_chassis_map.pb.txt";
    FLAGS_applied_bcm_chassis_map_file =
        FLAGS_test_tmpdir + "/applied_bcm_chassis_map_file.pb.txt";
    FLAGS_bcm_sdk_config_file = FLAGS_test_tmpdir + "/config.bcm";
    FLAGS_bcm_sdk_config_flush_file = FLAGS_test_tmpdir + "/config.bcm.tmp";
    FLAGS_bcm_sdk_shell_log_file = FLAGS_test_tmpdir + "/bcm.log";
    FLAGS_bcm_sdk_checkpoint_dir = FLAGS_test_tmpdir + "/sdk_checkpoint/";
  }

  void SetUp() override {
    mode_ = GetParam();
    phal_mock_ = absl::make_unique<PhalMock>();
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_serdes_db_manager_mock_ = absl::make_unique<BcmSerdesDbManagerMock>();
    bcm_chassis_manager_ = BcmChassisManager::CreateInstance(
        mode_, phal_mock_.get(), bcm_sdk_mock_.get(),
        bcm_serdes_db_manager_mock_.get());
    std::map<int, BcmNode*> unit_to_node_map;
    for (int i = 0; i < 4; ++i) {
      bcm_node_mocks_[i] = absl::make_unique<BcmNodeMock>();
      unit_to_node_map[i] = bcm_node_mocks_[i].get();
    }
    bcm_chassis_manager_->SetUnitToBcmNodeMap(unit_to_node_map);
  }

  bool Initialized() { return bcm_chassis_manager_->initialized_; }

  ::util::Status InitializeBcmChips(BcmChassisMap base_bcm_chassis_map,
                                    BcmChassisMap target_bcm_chassis_map) {
    return bcm_chassis_manager_->InitializeBcmChips(base_bcm_chassis_map,
                                                    target_bcm_chassis_map);
  }

  void TriggerLinkscanEvent(int unit, int logical_port, PortState state) {
    bcm_chassis_manager_->LinkscanEventHandler(unit, logical_port, state);
  }

  ::util::Status CheckCleanInternalState() {
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->unit_to_bcm_chip_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->singleton_port_key_to_bcm_port_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->port_group_key_to_flex_bcm_ports_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->port_group_key_to_non_flex_bcm_ports_.empty());
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->node_id_to_unit_.empty());
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->unit_to_node_id_.empty());
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->node_id_to_port_ids_.empty());
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->node_id_to_trunk_ids_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_singleton_port_key_
            .empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_sdk_port_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_trunk_id_to_sdk_trunk_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_sdk_port_to_port_id_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_sdk_trunk_to_trunk_id_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->xcvr_port_key_to_xcvr_state_.empty());

    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_port_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_trunk_id_to_trunk_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_trunk_id_to_members_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_trunk_membership_info_
            .empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_admin_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_health_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bcm_chassis_manager_->node_id_to_port_id_to_loopback_state_.empty());
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->base_bcm_chassis_map_ ==
                          nullptr);
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->applied_bcm_chassis_map_ ==
                          nullptr);
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->xcvr_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bcm_chassis_manager_->linkscan_event_channel_ ==
                          nullptr);

    return ::util::OkStatus();
  }

  ::util::Status PushChassisConfig(const ChassisConfig& config) {
    absl::WriterMutexLock l(&chassis_lock);
    shutdown = false;
    return bcm_chassis_manager_->PushChassisConfig(config);
  }

  ::util::Status VerifyChassisConfig(const ChassisConfig& config) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->VerifyChassisConfig(config);
  }

  ::util::Status Shutdown() {
    {
      absl::WriterMutexLock l(&chassis_lock);
      shutdown = true;
    }
    return bcm_chassis_manager_->Shutdown();
  }

  ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
    return bcm_chassis_manager_->RegisterEventNotifyWriter(writer);
  }

  ::util::Status UnregisterEventNotifyWriter() {
    return bcm_chassis_manager_->UnregisterEventNotifyWriter();
  }

  ::util::StatusOr<BcmChip> GetBcmChip(int unit) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetBcmChip(unit);
  }

  ::util::StatusOr<BcmPort> GetBcmPort(int slot, int port, int channel) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetBcmPort(slot, port, channel);
  }

  ::util::StatusOr<BcmPort> GetBcmPort(uint64 node_id, uint32 port_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetBcmPort(node_id, port_id);
  }

  ::util::StatusOr<std::map<uint64, int>> GetNodeIdToUnitMap() const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetNodeIdToUnitMap();
  }

  ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetUnitFromNodeId(node_id);
  }

  ::util::StatusOr<std::map<uint32, SdkPort>> GetPortIdToSdkPortMap(
      uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetPortIdToSdkPortMap(node_id);
  }

  ::util::StatusOr<std::map<uint32, SdkTrunk>> GetTrunkIdToSdkTrunkMap(
      uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetTrunkIdToSdkTrunkMap(node_id);
  }

  ::util::StatusOr<PortState> GetPortState(SdkPort sdk_port) {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetPortState(sdk_port);
  }

  ::util::StatusOr<PortState> GetPortState(uint64 node_id,
                                           uint32 port_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetPortState(node_id, port_id);
  }

  ::util::StatusOr<TrunkState> GetTrunkState(uint64 node_id,
                                             uint32 trunk_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetTrunkState(node_id, trunk_id);
  }

  ::util::StatusOr<std::set<uint32>> GetTrunkMembers(uint64 node_id,
                                                     uint32 trunk_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetTrunkMembers(node_id, trunk_id);
  }

  ::util::StatusOr<uint32> GetParentTrunkId(uint64 node_id,
                                            uint32 port_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetParentTrunkId(node_id, port_id);
  }

  ::util::StatusOr<AdminState> GetPortAdminState(uint64 node_id,
                                                 uint32 port_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetPortAdminState(node_id, port_id);
  }

  ::util::StatusOr<LoopbackState> GetPortLoopbackState(uint64 node_id,
                                                       uint32 port_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->GetPortLoopbackState(node_id, port_id);
  }

  ::util::Status SetTrunkMemberBlockState(uint64 node_id, uint32 trunk_id,
                                          uint32 port_id,
                                          TrunkMemberBlockState state) {
    absl::WriterMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->SetTrunkMemberBlockState(node_id, trunk_id,
                                                          port_id, state);
  }

  ::util::Status SetPortAdminState(uint64 node_id, uint32 port_id,
                                   AdminState state) {
    absl::WriterMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->SetPortAdminState(node_id, port_id, state);
  }

  ::util::Status SetPortHealthState(uint64 node_id, uint32 port_id,
                                    HealthState state) {
    absl::WriterMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->SetPortHealthState(node_id, port_id, state);
  }

  ::util::Status SetPortLoopbackState(uint64 node_id, uint32 port_id,
                                      LoopbackState state) {
    absl::WriterMutexLock l(&chassis_lock);
    return bcm_chassis_manager_->SetPortLoopbackState(node_id, port_id, state);
  }

  void SendPortOperStateGnmiEvent(int node_id, int port_id, PortState state) {
    absl::ReaderMutexLock l(&chassis_lock);
    bcm_chassis_manager_->SendPortOperStateGnmiEvent(node_id, port_id, state);
  }

  bool IsInternalPort(const PortKey& port_key) const {
    return bcm_chassis_manager_->IsInternalPort(port_key);
  }

  // A large number of tests in this file test pushing different configs and
  // there is no fixed config that works for all. However, there are other tests
  // where we just need some valid test config. For those tests, we provide two
  // helper methods to push config and initialized the class and later shutdown
  // the class: PushTestConfig() & ShutdownAndTestCleanState(). These two
  // methods both need to be called in the tests, one at the beginning and the
  // other at the end of the test.

  ::util::Status PushTestConfig() {
    ChassisConfig config;
    return PushTestConfig(&config);
  }

  ::util::Status PushTestConfig(ChassisConfig* config) {
    const std::string kBcmChassisMapListText = R"(
        bcm_chassis_maps {
          bcm_chips {
            type: TOMAHAWK
            slot: 1
            unit: 0
            module: 0
            pci_bus: 7
            pci_slot: 1
            is_oversubscribed: true
          }
          bcm_ports {
            type: CE
            slot: 1
            port: 1
            unit: 0
            speed_bps: 100000000000
            logical_port: 34
            physical_port: 33
            diag_port: 0
            serdes_lane: 0
            num_serdes_lanes: 4
          }
          bcm_ports {
            type: CE
            slot: 1
            port: 2
            unit: 0
            speed_bps: 100000000000
            logical_port: 38
            physical_port: 37
            diag_port: 4
            serdes_lane: 0
            num_serdes_lanes: 4
          }
          bcm_ports {
            type: XE
            slot: 1
            port: 1
            channel: 1
            unit: 0
            speed_bps: 50000000000
            logical_port: 34
            physical_port: 33
            diag_port: 0
            serdes_lane: 0
            num_serdes_lanes: 2
          }
          bcm_ports {
            type: XE
            slot: 1
            port: 1
            channel: 2
            unit: 0
            speed_bps: 50000000000
            logical_port: 36
            physical_port: 35
            diag_port: 2
            serdes_lane: 2
            num_serdes_lanes: 2
          }
        }
    )";

    const std::string kConfigText = R"(
        description: "Sample Generic Tomahawk config 32x100G ports."
        chassis {
          platform: PLT_GENERIC_TOMAHAWK
          name: "standalone"
        }
        nodes {
          id: 7654321
          slot: 1
        }
        singleton_ports {
          id: 12345
          slot: 1
          port: 1
          speed_bps: 100000000000
          node: 7654321
          config_params {
            admin_state: ADMIN_STATE_ENABLED
          }
        }
        trunk_ports {
          id: 222
          node: 7654321
          type: STATIC_TRUNK
          members: 12345
        }
        trunk_ports {
          id: 333
          node: 7654321
          type: LACP_TRUNK
        }
    )";

    // Expectations for the mock objects on initialization.
    EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                              FLAGS_bcm_sdk_config_flush_file,
                                              FLAGS_bcm_sdk_shell_log_file))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
        .WillRepeatedly(Return(std::string("")));
    EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
        .Times(2)
        .WillRepeatedly(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_,
                RegisterLinkscanEventWriter(
                    _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
        .WillOnce(Return(kTestLinkscanWriterId));
    EXPECT_CALL(*phal_mock_,
                RegisterTransceiverEventWriter(
                    _, PhalInterface::kTransceiverEventWriterPriorityHigh))
        .WillOnce(Return(kTestTransceiverWriterId));
    EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
        .WillOnce(Return(::util::OkStatus()));

    // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
    RETURN_IF_ERROR(WriteStringToFile(kBcmChassisMapListText,
                                      FLAGS_base_bcm_chassis_map_file));

    // Setup a test config and pass it to PushChassisConfig.
    RETURN_IF_ERROR(ParseProtoFromString(kConfigText, config));

    // Call PushChassisConfig() verify the results.
    CHECK_RETURN_IF_FALSE(!Initialized())
        << "Class is initialized before push!";
    RETURN_IF_ERROR(PushChassisConfig(*config));
    CHECK_RETURN_IF_FALSE(Initialized())
        << "Class is not initialized after push!";

    return ::util::OkStatus();
  }

  ::util::Status ShutdownAndTestCleanState() {
    EXPECT_CALL(*bcm_sdk_mock_,
                UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*phal_mock_,
                UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
        .WillOnce(Return(::util::OkStatus()));
    EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
        .WillOnce(Return(::util::OkStatus()));

    RETURN_IF_ERROR(Shutdown());
    RETURN_IF_ERROR(CheckCleanInternalState());

    return ::util::OkStatus();
  }

  OperationMode mode_;
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmSerdesDbManagerMock> bcm_serdes_db_manager_mock_;
  std::unique_ptr<BcmNodeMock> bcm_node_mocks_[4];
  std::unique_ptr<BcmChassisManager> bcm_chassis_manager_;
  static constexpr int kTestLinkscanWriterId = 10;
  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_P(BcmChassisManagerTest, PreFirstConfigPushState) {
  ASSERT_OK(CheckCleanInternalState());
  EXPECT_FALSE(Initialized());
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));

    ret = GetBcmPort(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_EQ(ERR_NOT_INITIALIZED, ret.status().error_code());
  }
  {
    auto ret = GetPortAdminState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_EQ(ERR_NOT_INITIALIZED, ret.status().error_code());
  }
  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest, NonGracefulShutdownAfterConfigPush) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          serdes_lane: 0
          num_serdes_lanes: 4
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  ASSERT_OK(PushChassisConfig(config));
  ASSERT_TRUE(Initialized());

  // This will destruct the class without unregistering the event handlers.
}

TEST_P(BcmChassisManagerTest,
       PushChassisConfigSuccessWithoutAutoAddLogicalPortsWithoutFlexPorts) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .Times(4)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after config push
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());

    // Unit 10 must not be in the internal map.
    ret = GetBcmChip(10);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown unit 10"));
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    auto bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(34, bcm_port.logical_port());

    // same port as (node_id, port_id) should be in map.
    ret = GetBcmPort(kNodeId, kPortId);
    bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(34, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) is not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Unknown singleton port"));

    // should not be able to get port on nonexistent node.
    ret = GetBcmPort(kNodeId - 1, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown node"));

    // should not be able to get nonexistent port on node.
    ret = GetBcmPort(kNodeId, kPortId - 1);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown port"));
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    // Get unit from a known node.
    auto ret = GetUnitFromNodeId(kNodeId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(0, ret.ValueOrDie());

    // Get unit from an unknown node.
    ret = GetUnitFromNodeId(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Get logical port map from a known node.
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 34), pord_id_to_sdk_port.at(kPortId));

    // Get logical port map from an unknown node.
    ret = GetPortIdToSdkPortMap(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Get trunk port map from a known node.
    auto ret = GetTrunkIdToSdkTrunkMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& trunk_id_to_sdk_trunk = ret.ValueOrDie();
    EXPECT_TRUE(trunk_id_to_sdk_trunk.empty());

    // Get trunk port map from an unknown node.
    ret = GetTrunkIdToSdkTrunkMap(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Port state for a known (node, port) right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // Get known port state via (unit, logical_port).
    ret = GetPortState(SdkPort(0, 34));
    ASSERT_TRUE(ret.ok()) << ret.status();
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // Port state for an unknown node.
    ret = GetPortState(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Port state for a unknown port .
    ret = GetPortState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Port 33333 is not known"));

    // Port state for unknown unit.
    ret = GetPortState(SdkPort(777777, 34));
    ASSERT_FALSE(ret.ok());

    // Port state for unknown SDK port on known unit.
    ret = GetPortState(SdkPort(0, 33333));
    ASSERT_FALSE(ret.ok());
  }
  {
    // Get port admin state for known port.
    auto ret = GetPortAdminState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(ADMIN_STATE_ENABLED, ret.ValueOrDie());

    // Fail to get port admin state on unknown node.
    ret = GetPortAdminState(kNodeId - 1, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown node"));

    // Fail to get port admin state for unknown port on node.
    ret = GetPortAdminState(kNodeId, kPortId - 1);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown port"));
  }
  {
    // Trunk state for an unknown node.
    auto ret = GetTrunkState(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk state for an unknown trunk.
    ret = GetTrunkState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Trunk 33333 is not known"));
  }
  {
    // Trunk members for an unknown node.
    auto ret = GetTrunkMembers(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk members for a unknown trunk .
    ret = GetTrunkMembers(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Trunk 33333 is not known"));
  }
  {
    // Parent trunk members for known (node, port) when the port is not part of
    // any trunk.
    auto ret = GetParentTrunkId(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(
        ret.status().error_message(),
        HasSubstr("Port 12345 is not known or does not belong to any trunk"));

    // Parent trunk members for an unknown node.
    ret = GetParentTrunkId(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk members for a unknown trunk .
    ret = GetParentTrunkId(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(
        ret.status().error_message(),
        HasSubstr("Port 33333 is not known or does not belong to any trunk"));
  }

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_OK(CheckCleanInternalState());
  ASSERT_FALSE(Initialized());

  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetUnitFromNodeId(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkIdToSdkTrunkMap(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkState(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkMembers(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetParentTrunkId(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest,
       PushChassisConfigSuccessWithoutAutoAddLogicalPortsWithFlexPorts) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          flex_port: true
          serdes_lane: 2
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 25000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 25000000000
          logical_port: 35
          physical_port: 34
          diag_port: 1
          flex_port: true
          serdes_lane: 1
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 3
          unit: 0
          speed_bps: 25000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          flex_port: true
          serdes_lane: 2
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 4
          unit: 0
          speed_bps: 25000000000
          logical_port: 37
          physical_port: 36
          diag_port: 3
          flex_port: true
          serdes_lane: 3
          num_serdes_lanes: 1
        }
      }
  )";

  const std::string kConfigText1 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
      }
  )";

  const std::string kConfigText2 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        channel: 1
        speed_bps: 50000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
      singleton_ports {
        id: 12346
        slot: 1
        port: 1
        channel: 2
        speed_bps: 50000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 35))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 36))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 37))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GetPortOptions(0, 34, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 35, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 36, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 37, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup test configs and pass them to PushChassisConfig. The first config
  // sets the port to 100G and the second one sets it to 2x50G.
  ChassisConfig config1, config2;
  ASSERT_OK(ParseProtoFromString(kConfigText1, &config1));
  ASSERT_OK(ParseProtoFromString(kConfigText2, &config2));

  // Call PushChassisConfig() and push config1 multiple times.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config1));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after pushing config1.
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(34, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) and (slot: 1, port: 1, channel: 2)
    // are not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
    ret = GetBcmPort(1, 1, 2);
    ASSERT_FALSE(ret.ok());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 34), pord_id_to_sdk_port.at(kPortId));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(kNodeId, 12346UL);
    ASSERT_FALSE(ret.ok());
  }

  // Now call PushChassisConfig() and push config2 multiple times.
  ASSERT_TRUE(Initialized());  // already initialized
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config2));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after pushing config2.
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());
  }
  {
    // (slot: 1, port: 1) is not in the internal map any more.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());

    // (slot: 1, port: 1, channel: 1) and (slot: 1, port: 1, channel: 2)
    // are in the internal map now.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(kFiftyGigBps, ret.ValueOrDie().speed_bps());
    EXPECT_EQ(34, ret.ValueOrDie().logical_port());

    ret = GetBcmPort(1, 1, 2);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(kFiftyGigBps, ret.ValueOrDie().speed_bps());
    EXPECT_EQ(36, ret.ValueOrDie().logical_port());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(2U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 34), pord_id_to_sdk_port.at(kPortId));
    EXPECT_EQ(SdkPort(0, 36), pord_id_to_sdk_port.at(12346UL));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());
  }

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_OK(CheckCleanInternalState());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest,
       PushChassisConfigSuccessWithAutoAddLogicalPortsWithoutFlexPorts) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 1, _))
      .Times(4)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after config push
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(1, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) is not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 1), pord_id_to_sdk_port.at(kPortId));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
  }
  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
  /* BCM SDK Only
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.0=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.0=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.0=33:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.0=0"));
  */

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest,
       PushChassisConfigSuccessWithAutoAddLogicalPortsWithFlexPorts) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          physical_port: 35
          diag_port: 2
          flex_port: true
          serdes_lane: 2
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 25000000000
          physical_port: 33
          diag_port: 0
          flex_port: true
          serdes_lane: 0
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 25000000000
          physical_port: 34
          diag_port: 1
          flex_port: true
          serdes_lane: 1
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 3
          unit: 0
          speed_bps: 25000000000
          physical_port: 35
          diag_port: 2
          flex_port: true
          serdes_lane: 2
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 4
          unit: 0
          speed_bps: 25000000000
          physical_port: 36
          diag_port: 3
          flex_port: true
          serdes_lane: 3
          num_serdes_lanes: 1
        }
      }
  )";

  const std::string kConfigText1 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  const std::string kConfigText2 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        channel: 1
        speed_bps: 50000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
      singleton_ports {
        id: 12346
        slot: 1
        port: 1
        channel: 2
        speed_bps: 50000000000
        node: 7654321
        config_params {
          admin_state: ADMIN_STATE_ENABLED
        }
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 2))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 3))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 4))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GetPortOptions(0, 1, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 1, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 2, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 3, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 4, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup test configs and pass them to PushChassisConfig. The first config
  // sets the port to 100G and the second one sets it to 2x50G.
  ChassisConfig config1, config2;
  ASSERT_OK(ParseProtoFromString(kConfigText1, &config1));
  ASSERT_OK(ParseProtoFromString(kConfigText2, &config2));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config1));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after pushing config1.
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(1, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) and (slot: 1, port: 1, channel: 2)
    // are not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
    ret = GetBcmPort(1, 1, 2);
    ASSERT_FALSE(ret.ok());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 1), pord_id_to_sdk_port.at(kPortId));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(kNodeId, 12346UL);
    ASSERT_FALSE(ret.ok());
  }
  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
  /* BCM SDK only
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.0=0x1E"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.0=0x1E"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.0=33:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.0=0"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_2.0=34:25"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_2.0=1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_3.0=35:50"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_3.0=2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_4.0=36:25"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_4.0=3"));
  */

  // Now call PushChassisConfig() and push config2 multiple times.
  ASSERT_TRUE(Initialized());  // already initialized
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config2));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after pushing config2.
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());
  }
  {
    // (slot: 1, port: 1) is not in the internal map any more.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());

    // (slot: 1, port: 1, channel: 1) and (slot: 1, port: 1, channel: 2)
    // are in the internal map now.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(kFiftyGigBps, ret.ValueOrDie().speed_bps());
    EXPECT_EQ(1, ret.ValueOrDie().logical_port());

    ret = GetBcmPort(1, 1, 2);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(kFiftyGigBps, ret.ValueOrDie().speed_bps());
    EXPECT_EQ(3, ret.ValueOrDie().logical_port());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(2U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 1), pord_id_to_sdk_port.at(kPortId));
    EXPECT_EQ(SdkPort(0, 3), pord_id_to_sdk_port.at(12346UL));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());
  }
  bcm_sdk_config.clear();
  /* BCM SDK only
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  // This is the same as the previous case.
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.0=0x1E"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.0=0x1E"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.0=33:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.0=0"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_2.0=34:25"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_2.0=1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_3.0=35:50"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_3.0=2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_4.0=36:25"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_4.0=3"));
  */

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest, PushChassisConfigSuccessWithAutoAddSlot) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        auto_add_slot: True
        bcm_chips {
          type: TRIDENT2
          pci_bus: 1
          is_oversubscribed: true
        }
        bcm_chips {
          type: TRIDENT2
          unit: 1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
        bcm_chips {
          type: TRIDENT2
          unit: 2
          module: 2
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          port: 80
          unit: 2
          speed_bps: 40000000000
          physical_port: 61
          diag_port: 64
          module: 2
          serdes_core: 15
          num_serdes_lanes: 4
          rx_lane_map: 4131
          internal: true
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 1
          unit: 2
          speed_bps: 20000000000
          physical_port: 61
          diag_port: 64
          module: 2
          serdes_core: 15
          num_serdes_lanes: 2
          rx_lane_map: 4131
          internal: true
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 2
          unit: 2
          speed_bps: 20000000000
          physical_port: 63
          diag_port: 66
          module: 2
          serdes_core: 15
          serdes_lane: 2
          num_serdes_lanes: 2
          internal: true
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(2, 5, 0, BcmChip::TRIDENT2))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(2, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(2, 2))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(2, 1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_, GetFrontPanelPortInfo(9, 80, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, LookupSerdesConfigForPort(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigSerdesForPort(2, 1, kFortyGigBps, 15, 0, 4, _, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(2, 1, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(2))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after config push
  {
    // Unit 2 must be in the internal map.
    auto ret = GetBcmChip(2);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TRIDENT2, bcm_chip.type());
    EXPECT_EQ(9, bcm_chip.slot());
  }
  {
    // (slot: 9, port: 80) needs to be in the internal map.
    auto ret = GetBcmPort(9, 80, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kFortyGigBps, bcm_port.speed_bps());
    EXPECT_EQ(1, bcm_port.logical_port());

    // (slot: 9, port: 80, channel: 1) is not in the internal map.
    ret = GetBcmPort(9, 80, 1);
    ASSERT_FALSE(ret.ok());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(2, node_id_to_unit.at(kNodeId));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(2, 1), pord_id_to_sdk_port.at(kPortId));
  }
  {
    // State for a known port right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
  }

  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
  /* BCM SDK only
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.2=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.2=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.2=61:40"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.2=64"));
  */

  EXPECT_TRUE(IsInternalPort({9, 80}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetBcmPort(9, 80, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmChip(2);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }

  EXPECT_FALSE(IsInternalPort({9, 80}));
}

TEST_P(BcmChassisManagerTest, PushChassisConfigSuccessWithTrunks) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        node: 7654321
        type: STATIC_TRUNK
        members: 12345
      }
      trunk_ports {
        id: 333
        node: 7654321
        type: LACP_TRUNK
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 1))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 1, _))
      .Times(3)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() multiple times and verify the results.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after config push
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TOMAHAWK, bcm_chip.type());
    EXPECT_EQ(1, bcm_chip.slot());

    // Unit 10 must not be in the internal map.
    ret = GetBcmChip(10);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Unknown unit 10"));
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(1, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) is not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Unknown singleton port"));
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(kNodeId));
  }
  {
    // Get unit from a know node.
    auto ret = GetUnitFromNodeId(kNodeId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(0, ret.ValueOrDie());

    // Get unit from an unknown node.
    ret = GetUnitFromNodeId(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Get logical port map from a known node.
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_sdk_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_sdk_port.size());
    EXPECT_EQ(SdkPort(0, 1), pord_id_to_sdk_port.at(kPortId));

    // Get logical port map from an unknown node.
    ret = GetPortIdToSdkPortMap(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Get trunk port map from a known node.
    auto ret = GetTrunkIdToSdkTrunkMap(kNodeId);
    ASSERT_TRUE(ret.ok());
    const auto& trunk_id_to_sdk_trunk = ret.ValueOrDie();
    ASSERT_EQ(2U, trunk_id_to_sdk_trunk.size());
    EXPECT_EQ(SdkTrunk(0, /*invalid*/ -1), trunk_id_to_sdk_trunk.at(222));
    EXPECT_EQ(SdkTrunk(0, /*invalid*/ -1), trunk_id_to_sdk_trunk.at(333));

    // Get trunk port map from an unknown node.
    ret = GetTrunkIdToSdkTrunkMap(777777UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));
  }
  {
    // Port state for a known (node, port) right after config is pushed.
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // Port state for an unknown node.
    ret = GetPortState(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Port state for a unknown port .
    ret = GetPortState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Port 33333 is not known"));
  }
  {
    // Trunk state for a known (node, port) right after config is pushed.
    auto ret = GetTrunkState(kNodeId, 222);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(TRUNK_STATE_UNKNOWN, ret.ValueOrDie());
    ret = GetTrunkState(kNodeId, 333);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(TRUNK_STATE_UNKNOWN, ret.ValueOrDie());

    // Trunk state for an unknown node.
    ret = GetTrunkState(777777UL, 222);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk state for an unknown trunk.
    ret = GetTrunkState(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Trunk 33333 is not known"));
  }
  {
    auto ret = GetTrunkMembers(kNodeId, 222);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(std::set<uint32>(), ret.ValueOrDie());
    ret = GetTrunkMembers(kNodeId, 333);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(std::set<uint32>(), ret.ValueOrDie());

    // Trunk members for an unknown node.
    ret = GetTrunkMembers(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk members for a unknown trunk .
    ret = GetTrunkMembers(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Trunk 33333 is not known"));
  }
  {
    // Parent trunk members for known (node, port) when the port is not part of
    // any trunk.
    auto ret = GetParentTrunkId(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(
        ret.status().error_message(),
        HasSubstr("Port 12345 is not known or does not belong to any trunk"));

    // Parent trunk members for an unknown node.
    ret = GetParentTrunkId(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Node 777777 is not configured"));

    // Trunk members for a unknown trunk .
    ret = GetParentTrunkId(kNodeId, 33333UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(
        ret.status().error_message(),
        HasSubstr("Port 33333 is not known or does not belong to any trunk"));
  }

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_OK(CheckCleanInternalState());
  ASSERT_FALSE(Initialized());

  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetUnitFromNodeId(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortIdToSdkPortMap(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkIdToSdkTrunkMap(kNodeId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkState(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetTrunkMembers(777777UL, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetParentTrunkId(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest, PushChassisConfigSuccessWithGePortOnTridentPlus) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT_PLUS
          slot: 3
          pci_bus: 21
        }
        bcm_ports {
          type: XE
          slot: 3
          port: 10
          channel: 1
          speed_bps: 10000000000
          logical_port: 37
          physical_port: 41
          diag_port: 36
          serdes_core: 10
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 3
          port: 10
          channel: 2
          speed_bps: 10000000000
          logical_port: 38
          physical_port: 42
          diag_port: 37
          serdes_core: 10
          serdes_lane: 1
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 3
          port: 10
          channel: 3
          speed_bps: 10000000000
          logical_port: 39
          physical_port: 43
          diag_port: 38
          serdes_core: 10
          serdes_lane: 2
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: XE
          slot: 3
          port: 10
          channel: 4
          speed_bps: 10000000000
          logical_port: 40
          physical_port: 44
          diag_port: 39
          serdes_core: 10
          serdes_lane: 3
          num_serdes_lanes: 1
        }
        bcm_ports {
          type: GE
          slot: 3
          port: 33
          speed_bps: 1000000000
          physical_port: 1
          diag_port: 64
          num_serdes_lanes: 1
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample generic_trident_plus config 64x4x10G ports."
      chassis {
        platform: PLT_GENERIC_TRIDENT_PLUS
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 3
      }
      singleton_ports {
        id: 12345
        slot: 3
        port: 10
        channel: 1
        speed_bps: 10000000000
        node: 7654321
      }
      singleton_ports {
        id: 12346
        slot: 3
        port: 10
        channel: 4
        speed_bps: 10000000000
        node: 7654321
      }
      singleton_ports {
        id: 12347
        slot: 3
        port: 33
        speed_bps: 1000000000
        node: 7654321
      }
  )";

  // Expectations for the mock objects.
  // Note that the logical port used for GE port is the largest logical port
  // in X pipeline, which is 32.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 21, 0, BcmChip::TRIDENT_PLUS))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 32))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 37))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 40))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GetPortOptions(0, 37, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 32, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 37, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 40, _))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup test configs and pass them to PushChassisConfig. The first config
  // sets the port to 100G and the second one sets it to 2x50G.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() and push config multiple times.
  ASSERT_FALSE(Initialized());
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(PushChassisConfig(config));
    ASSERT_TRUE(Initialized());
  }

  // Verify the state after pushing config.
  {
    // Unit 0 must be in the internal map.
    auto ret = GetBcmChip(0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_chip = ret.ValueOrDie();
    EXPECT_EQ(BcmChip::TRIDENT_PLUS, bcm_chip.type());
    EXPECT_EQ(3, bcm_chip.slot());
  }

  {
    // (slot: 3, port: 10, channel: 1) needs to be in the internal map.
    auto ret = GetBcmPort(3, 10, 1);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kTenGigBps, bcm_port.speed_bps());
    EXPECT_EQ(37, bcm_port.logical_port());

    // (slot: 3, port: 10, channel: 2) is not in the internal map.
    ret = GetBcmPort(3, 10, 2);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(),
                HasSubstr("Unknown singleton port"));
  }

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_OK(CheckCleanInternalState());
  ASSERT_FALSE(Initialized());

  {
    auto ret = GetBcmChip(0);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetBcmPort(3, 10, 1);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest, PushChassisConfigFailure) {
  // Valid BcmChassisMapList.
  const std::string kBcmChassisMapListText1 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: true
        auto_add_slot: true
        bcm_chips {
          type: TRIDENT2
          unit: 2
          module: 2
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_chips {
          type: TRIDENT2
          unit: 3
          module: 3
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
        bcm_ports {
          type: XE
          port: 84
          unit: 3
          module: 3
          speed_bps: 40000000000
          physical_port: 67
          diag_port: 70
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
      }
  )";

  // auto_add_slot = true but slot is specified for node.
  const std::string kBcmChassisMapListText2 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        auto_add_slot: true
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
      }
  )";
  const std::string kChassisMapError2 =
      "auto_add_slot is True and slot is non-zero for chip";

  // auto_add_slot = false but slot is not specified for node.
  const std::string kBcmChassisMapListText3 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          unit: 1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
      }
  )";
  const std::string kChassisMapError3 = "Invalid slot";

  // auto_add_slot = true but slot is specified for port.
  const std::string kBcmChassisMapListText4 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: True
        auto_add_slot: true
        bcm_chips {
          type: TRIDENT2
          unit: 2
          module: 2
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
     }
  )";
  const std::string kChassisMapError4 =
      "auto_add_slot is True and slot is non-zero for port";

  // Invalid unit given for node.
  const std::string kBcmChassisMapListText5 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: -1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
      }
  )";
  const std::string kChassisMapError5 = "Invalid unit";

  // Same unit number given to multiple nodes.
  const std::string kBcmChassisMapListText6 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 1
          module: 1
          pci_bus: 3
          is_oversubscribed: true
        }
      }
  )";
  const std::string kChassisMapError6 = "Invalid unit";

  // Same module number given to multiple nodes.
  const std::string kBcmChassisMapListText7 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 1
          module: 2
          pci_bus: 3
          is_oversubscribed: true
        }
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 2
          module: 2
          pci_bus: 3
          is_oversubscribed: true
        }
      }
  )";
  const std::string kChassisMapError7 = "Invalid module";

  // Unit for a port is not known.
  const std::string kBcmChassisMapListText8 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 2
          module: 2
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 80
          unit: 3
          module: 2
          speed_bps: 40000000000
        }
     }
  )";
  const std::string kChassisMapError8 = "Invalid unit";

  // Module for a port is not known.
  const std::string kBcmChassisMapListText9 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          slot: 1
          unit: 2
          module: 2
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 80
          unit: 2
          module: 3
          speed_bps: 40000000000
        }
     }
  )";
  const std::string kChassisMapError9 = "Invalid module";

  // auto_add_logical_ports = true and logical port is given
  const std::string kBcmChassisMapListText10 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: true
        bcm_chips {
          type: TRIDENT2
          unit: 2
          slot: 1
          module: 2
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          logical_port: 88
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
     }
  )";
  const std::string kChassisMapError10 =
      "auto_add_logical_ports is True and logical_port is non-zero";

  // auto_add_logical_ports = false and logical port is not given
  const std::string kBcmChassisMapListText11 = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TRIDENT2
          unit: 2
          slot: 1
          module: 2
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
     }
  )";
  const std::string kChassisMapError11 =
      "auto_add_logical_ports is False and port is not a GE port, yet "
      "logical_port is not positive";

  // For the same (slot, port) we have both flex and non-flex ports.
  const std::string kBcmChassisMapListText12 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: true
        auto_add_slot: true
        bcm_chips {
          type: TRIDENT2
          unit: 2
          module: 2
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          flex_port: true
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 1
          unit: 2
          module: 2
          speed_bps: 20000000000
          flex_port: true
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 2
          unit: 2
          module: 2
          speed_bps: 20000000000
          physical_port: 65
          diag_port: 68
          serdes_core: 15
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";
  const std::string kChassisMapError12 = "is in flex_port_group_keys";

  // Unsupported chip type. Generic Trident2 supports TRIDENT2 while BcmChip
  // here says TOMAHAWK.
  const std::string kBcmChassisMapListText13 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: true
        auto_add_slot: true
        bcm_chips {
          type: TOMAHAWK
          unit: 2
          module: 2
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          flex_port: true
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
      }
  )";
  const std::string kChassisMapError13 = "Chip type TOMAHAWK is not supported";

  // For the same (slot, port), we have both internal and external ports.
  const std::string kBcmChassisMapListText14 = R"(
      bcm_chassis_maps {
        auto_add_logical_ports: true
        auto_add_slot: true
        bcm_chips {
          type: TRIDENT2
          unit: 2
          module: 2
          pci_bus: 5
          is_oversubscribed: true
        }
        bcm_ports {
          type: XE
          port: 80
          unit: 2
          module: 2
          speed_bps: 40000000000
          flex_port: true
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 4
          internal: true
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 1
          unit: 2
          module: 2
          speed_bps: 20000000000
          flex_port: true
          physical_port: 63
          diag_port: 66
          serdes_core: 15
          serdes_lane: 0
          num_serdes_lanes: 2
          internal: true
        }
        bcm_ports {
          type: XE
          port: 80
          channel: 2
          unit: 2
          module: 2
          speed_bps: 20000000000
          flex_port: true
          physical_port: 65
          diag_port: 68
          serdes_core: 15
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";
  const std::string kChassisMapError14 =
      "found both internal and external BCM ports";

  // Valid chassis config for non flex ports.
  const std::string kConfigText1 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";

  // Valid chassis config for flex ports.
  const std::string kConfigText2 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        channel: 1
        speed_bps: 20000000000
        node: 7654321
      }
      singleton_ports {
        id: 12346
        slot: 9
        port: 80
        channel: 2
        speed_bps: 20000000000
        node: 7654321
      }
  )";

  // Chassis config with a port poiting to unknown node.
  const std::string kConfigText3 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 33333
      }
  )";
  const std::string kConfigError3 =
      "has not been given to any Node in the config";

  // Chassis config with multiple nodes with the same ID.
  const std::string kConfigText4 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      nodes {
        id: 7654321
        slot: 9
      }
  )";
  const std::string kConfigError4 =
      "was already recorded for another Node in the config";

  // Chassis config which uses the reserved CPU port
  const std::string kConfigText5 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 0xFD
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError5 = "has the reserved CPU port ID";

  // Chassis config with multiple ports with the same ID.
  const std::string kConfigText6 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 81
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError6 =
      "was already recorded for another SingletonPort";

  // Same slot/port/channel given to two ports.
  const std::string kConfigText7 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      singleton_ports {
        id: 12346
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError7 =
      "was already recorded for another SingletonPort";

  // Inconsistent config. The node Id that a port is pointing to was previously
  // assigned to a unit but chassis map suggests a different unit for the port.
  const std::string kConfigText8 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      singleton_ports {
        id: 12346
        slot: 9
        port: 84
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError8 = "But BcmChassisMap now suggests unit";

  // Speed of the port does not match anything in the BcmChassisMap.
  const std::string kConfigText9 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 50000000000
        node: 7654321
      }
  )";
  const std::string kConfigError9 =
      "Could not find any BcmPort in base_bcm_chassis_map whose (slot, port, "
      "channel, speed_bps) tuple matches non-flex SingletonPort ";

  // No ID for singleton port.
  const std::string kConfigText10 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError10 = "No positive ID ";

  // No valid slot.
  const std::string kConfigText11 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError11 =
      "Cannot support a case where auto_add_slot";

  // No valid port.
  const std::string kConfigText12 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError12 = "No valid port";

  // No valid speed.
  const std::string kConfigText13 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        node: 7654321
      }
  )";
  const std::string kConfigError13 = "No valid speed_bps";

  // No valid node.
  const std::string kConfigText14 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
      }
  )";
  const std::string kConfigError14 = "No valid node ID";

  // Node with no port.
  const std::string kConfigText15 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
  )";
  const std::string kConfigError15 = "No port found for Node with ID";

  // No trunk ID.
  const std::string kConfigText16 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        node: 7654321
        type: STATIC_TRUNK
        members: 12345
      }
  )";
  const std::string kConfigError16 = "No positive ID";

  // No type for trunk.
  const std::string kConfigText17 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        node: 7654321
        members: 12345
      }
  )";
  const std::string kConfigError17 = "No type";

  // No node ID for trunk.
  const std::string kConfigText18 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        type: STATIC_TRUNK
        members: 12345
      }
  )";
  const std::string kConfigError18 = "No valid node ID";

  // Trunk using reserved CPU port id.
  const std::string kConfigText19 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 0xFD
        node: 7654321
        type: STATIC_TRUNK
        members: 12345
      }
  )";
  const std::string kConfigError19 = "has the reserved CPU port ID";

  // Trunk ID is colliding with port IDs.
  const std::string kConfigText20 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 12345
        node: 7654321
        type: STATIC_TRUNK
        members: 12345
      }
  )";
  const std::string kConfigError20 =
      "was already recorded for another SingletonPort";

  // Trunk ID already used before.
  const std::string kConfigText21 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        node: 7654321
        type: STATIC_TRUNK
        members: 12345
      }
      trunk_ports {
        id: 222
        node: 7654321
        type: STATIC_TRUNK
      }
  )";
  const std::string kConfigError21 =
      "was already recorded for another TrunkPort";

  // Unknown node ID for trunk.
  const std::string kConfigText22 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        node: 33333333
        type: STATIC_TRUNK
        members: 12345
      }
  )";
  const std::string kConfigError22 =
      "has not been given to any Node in the config";

  // Trunk member is unknown.
  const std::string kConfigText23 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 9
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
      trunk_ports {
        id: 222
        node: 7654321
        type: STATIC_TRUNK
        members: 234234
      }
  )";
  const std::string kConfigError23 = "Unknown member SingletonPort";

  // Chassis config with more than one slot for nodes and ports.
  const std::string kConfigText24 = R"(
      description: "Sample Trazpezium config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
      }
      nodes {
        id: 7654321
        slot: 9
      }
      singleton_ports {
        id: 12345
        slot: 10
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError24 =
      "auto_add_slot is true and we have more than one slot";

  // Each tuple contains the ChassisConfig text, BcmChassisMap text, and the
  // resulting config push error.
  const std::vector<std::tuple<std::string, std::string, std::string>>
      kBadBcmChassisMaps = {
          std::make_tuple(kConfigText1, kBcmChassisMapListText2,
                          kChassisMapError2),
          std::make_tuple(kConfigText1, kBcmChassisMapListText3,
                          kChassisMapError3),
          std::make_tuple(kConfigText1, kBcmChassisMapListText4,
                          kChassisMapError4),
          std::make_tuple(kConfigText1, kBcmChassisMapListText5,
                          kChassisMapError5),
          std::make_tuple(kConfigText1, kBcmChassisMapListText6,
                          kChassisMapError6),
          std::make_tuple(kConfigText1, kBcmChassisMapListText7,
                          kChassisMapError7),
          std::make_tuple(kConfigText1, kBcmChassisMapListText8,
                          kChassisMapError8),
          std::make_tuple(kConfigText1, kBcmChassisMapListText9,
                          kChassisMapError9),
          std::make_tuple(kConfigText1, kBcmChassisMapListText10,
                          kChassisMapError10),
          std::make_tuple(kConfigText1, kBcmChassisMapListText11,
                          kChassisMapError11),
          std::make_tuple(kConfigText2, kBcmChassisMapListText12,
                          kChassisMapError12),
          std::make_tuple(kConfigText1, kBcmChassisMapListText13,
                          kChassisMapError13),
          std::make_tuple(kConfigText2, kBcmChassisMapListText14,
                          kChassisMapError14)};

  const std::vector<std::tuple<std::string, std::string, std::string>>
      kBadChassisConfigs = {
          std::make_tuple(kConfigText3, kBcmChassisMapListText1, kConfigError3),
          std::make_tuple(kConfigText4, kBcmChassisMapListText1, kConfigError4),
          std::make_tuple(kConfigText5, kBcmChassisMapListText1, kConfigError5),
          std::make_tuple(kConfigText6, kBcmChassisMapListText1, kConfigError6),
          std::make_tuple(kConfigText7, kBcmChassisMapListText1, kConfigError7),
          std::make_tuple(kConfigText8, kBcmChassisMapListText1, kConfigError8),
          std::make_tuple(kConfigText9, kBcmChassisMapListText1, kConfigError9),
          std::make_tuple(kConfigText10, kBcmChassisMapListText1,
                          kConfigError10),
          std::make_tuple(kConfigText11, kBcmChassisMapListText1,
                          kConfigError11),
          std::make_tuple(kConfigText12, kBcmChassisMapListText1,
                          kConfigError12),
          std::make_tuple(kConfigText13, kBcmChassisMapListText1,
                          kConfigError13),
          std::make_tuple(kConfigText14, kBcmChassisMapListText1,
                          kConfigError14),
          std::make_tuple(kConfigText15, kBcmChassisMapListText1,
                          kConfigError15),
          std::make_tuple(kConfigText16, kBcmChassisMapListText1,
                          kConfigError16),
          std::make_tuple(kConfigText17, kBcmChassisMapListText1,
                          kConfigError17),
          std::make_tuple(kConfigText18, kBcmChassisMapListText1,
                          kConfigError18),
          std::make_tuple(kConfigText19, kBcmChassisMapListText1,
                          kConfigError19),
          std::make_tuple(kConfigText20, kBcmChassisMapListText1,
                          kConfigError20),
          std::make_tuple(kConfigText21, kBcmChassisMapListText1,
                          kConfigError21),
          std::make_tuple(kConfigText22, kBcmChassisMapListText1,
                          kConfigError22),
          std::make_tuple(kConfigText23, kBcmChassisMapListText1,
                          kConfigError23),
          std::make_tuple(kConfigText24, kBcmChassisMapListText1,
                          kConfigError24)};

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillRepeatedly(Return(::util::OkStatus()));

  // Call PushChassisConfig() for all the bad configs and verify the results.
  for (const auto& e : kBadBcmChassisMaps) {
    SCOPED_TRACE(absl::StrCat("Failed for the following BcmChassisMap: ",
                              std::get<1>(e)));

    // Valid ChassisConfig but invalid BcmChassisMapList
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(std::get<0>(e), &config));
    ASSERT_OK(
        WriteStringToFile(std::get<1>(e), FLAGS_base_bcm_chassis_map_file));
    ASSERT_FALSE(Initialized());
    ::util::Status status = PushChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr(std::get<2>(e)));
  }
  for (const auto& e : kBadChassisConfigs) {
    SCOPED_TRACE(absl::StrCat("Failed for the following ChassisConfig: ",
                              std::get<0>(e)));

    // Invalid ChassisConfig but valid BcmChassisMapList.
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(std::get<0>(e), &config));
    ASSERT_OK(
        WriteStringToFile(std::get<1>(e), FLAGS_base_bcm_chassis_map_file));
    ASSERT_FALSE(Initialized());
    ::util::Status status = PushChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr(std::get<2>(e)));
  }
  {
    // A special case where we run out of empty slots for logical ports for
    // GE ports on a T+.
    BcmChassisMapList bcm_chassis_map_list;
    auto* bcm_chassis_map = bcm_chassis_map_list.add_bcm_chassis_maps();
    auto* bcm_chip = bcm_chassis_map->add_bcm_chips();
    bcm_chip->set_type(BcmChip::TRIDENT_PLUS);
    bcm_chip->set_slot(3);
    for (int i = 1; i <= 16; ++i) {
      for (int j = 1; j <= 4; ++j) {
        auto* bcm_port = bcm_chassis_map->add_bcm_ports();
        bcm_port->set_type(BcmPort::XE);
        bcm_port->set_slot(3);
        bcm_port->set_port(i);
        bcm_port->set_channel(j);
        bcm_port->set_speed_bps(kTenGigBps);
        bcm_port->set_logical_port((i - 1) * 4 + j);
        bcm_port->set_physical_port((i - 1) * 4 + j);
        bcm_port->set_diag_port((i - 1) * 4 + j - 1);
        // bcm_port->set_serdes_core(10);
        bcm_port->set_num_serdes_lanes(1);
      }
    }
    {
      auto* bcm_port = bcm_chassis_map->add_bcm_ports();
      bcm_port->set_type(BcmPort::GE);
      bcm_port->set_slot(3);
      bcm_port->set_port(33);
      bcm_port->set_speed_bps(kOneGigBps);
      bcm_port->set_physical_port(1);
      bcm_port->set_diag_port(64);
      bcm_port->set_num_serdes_lanes(1);
    }

    ChassisConfig config;
    config.mutable_chassis()->set_platform(PLT_GENERIC_TRIDENT_PLUS);
    auto* node = config.add_nodes();
    node->set_id(7654321);
    node->set_slot(3);
    for (int i = 1; i <= 16; ++i) {
      for (int j = 1; j <= 4; ++j) {
        auto* singleton_port = config.add_singleton_ports();
        singleton_port->set_id(12345 + (i - 1) * 4 + j);
        singleton_port->set_slot(3);
        singleton_port->set_port(i);
        singleton_port->set_channel(j);
        singleton_port->set_speed_bps(kTenGigBps);
        singleton_port->set_node(7654321);
      }
    }
    {
      auto* singleton_port = config.add_singleton_ports();
      singleton_port->set_id(12345 + 65);
      singleton_port->set_slot(3);
      singleton_port->set_port(33);
      singleton_port->set_speed_bps(kOneGigBps);
      singleton_port->set_node(7654321);
    }

    ASSERT_OK(WriteProtoToTextFile(bcm_chassis_map_list,
                                   FLAGS_base_bcm_chassis_map_file));
    ::util::Status status = PushChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(),
                HasSubstr("There is no empty logical_port in X pipeline of the "
                          "T+ chip to assign to GE port"));
  }
}

TEST_P(BcmChassisManagerTest, VerifyChassisConfigSuccess) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chassis {
          sdk_properties: "property1=1234"
          sdk_properties: "property2=5678"
        }
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  // Valid configs.
  const std::string kConfigText1 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 1
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 1
      }
  )";

  const std::vector<std::string> good_config_texts = {kConfigText1};

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Call VerifyChassisConfig() for good and bad configs and verify the results.
  for (const std::string& e : good_config_texts) {
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(e, &config));
    EXPECT_OK(VerifyChassisConfig(config));
  }
}

TEST_P(BcmChassisManagerTest, VerifyChassisConfigReportsRebootRequired) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  // Valid config.
  const std::string kConfigText1 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
      }
  )";

  // Reboot required config due to change in node ID.
  const std::string kConfigText2 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 77777
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 77777
      }
  )";
  const std::string kConfigError2 = "requires a change in node_id_to_unit";

  // Reboot required config due to change in applied bcm chassis map.
  const std::string kConfigText3 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        channel: 1
        speed_bps: 50000000000
        node: 7654321
      }
      singleton_ports {
        id: 12346
        slot: 1
        port: 1
        channel: 2
        speed_bps: 50000000000
        node: 7654321
      }
  )";
  const std::string kConfigError3 =
      "requires a change in applied_bcm_chassis_map_";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Push the initial config first.
  {
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(kConfigText1, &config));
    ASSERT_OK(VerifyChassisConfig(config));
    ASSERT_OK(PushChassisConfig(config));
  }

  // Now verify the reboot required behavior.
  const std::vector<std::pair<std::string, std::string>>
      reboot_req_config_texts_to_errors = {{kConfigText2, kConfigError2},
                                           {kConfigText3, kConfigError3}};

  for (const auto& e : reboot_req_config_texts_to_errors) {
    SCOPED_TRACE(
        absl::StrCat("Failed for the following ChassisConfig: ", e.first));

    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(e.first, &config));
    ::util::Status status = VerifyChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), ERR_REBOOT_REQUIRED);
    EXPECT_THAT(status.error_message(), HasSubstr(e.second));
    ASSERT_TRUE(Initialized());
  }

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
}

TEST_P(BcmChassisManagerTest, VerifyChassisConfigFailure) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chassis {
          sdk_properties: "property1=1234"
          sdk_properties: "property2=5678"
        }
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 2
          unit: 0
          speed_bps: 100000000000
          logical_port: 38
          physical_port: 37
          diag_port: 4
          serdes_lane: 0
          num_serdes_lanes: 4
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 1
          unit: 0
          speed_bps: 50000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 2
        }
        bcm_ports {
          type: XE
          slot: 1
          port: 1
          channel: 2
          unit: 0
          speed_bps: 50000000000
          logical_port: 36
          physical_port: 35
          diag_port: 2
          serdes_lane: 2
          num_serdes_lanes: 2
        }
      }
  )";

  // Note that the following failure cases may have been already tested in
  // PushChassisConfigFailure. We repeat some of them for a different platform
  // that has fixed slot.

  // ChassisConfig with invalid bcm_chassis_map_id.
  const std::string kConfigText1 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 1
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 1
      }
      vendor_config {
        google_config {
          bcm_chassis_map_id: "TEST"
        }
      }
  )";
  const std::string kConfigError1 = "Did not find a BcmChassisMap with id TEST";

  // ChassisConfig with unknown slot.
  const std::string kConfigText2 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 1
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 10
        speed_bps: 100000000000
        node: 1
      }
  )";
  const std::string kConfigError2 =
      "Could not find any BcmPort in base_bcm_chassis_map whose (slot, port, "
      "channel, speed_bps) tuple matches non-flex SingletonPort";

  // ChassisConfig with non-existing port.
  const std::string kConfigText3 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 1
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 10
        speed_bps: 100000000000
        node: 1
      }
  )";
  const std::string kConfigError3 =
      "Could not find any BcmPort in base_bcm_chassis_map whose (slot, port, "
      "channel, speed_bps) tuple matches non-flex SingletonPort";

  // Two different speeds for a single (slot, port).
  const std::string kConfigText4 = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 1
        slot: 1
      }
      singleton_ports {
        id: 1
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 1
      }
      singleton_ports {
        id: 2
        slot: 1
        port: 1
        channel: 1
        speed_bps: 50000000000
        node: 1
      }
  )";
  const std::string kConfigError4 =
      "found 2 different speed_bps. This is invalid.";

  const std::vector<std::pair<std::string, std::string>> kBadChassisConfigs = {
      {kConfigText1, kConfigError1},
      {kConfigText2, kConfigError2},
      {kConfigText3, kConfigError3},
      {kConfigText4, kConfigError4}};

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  for (const auto& e : kBadChassisConfigs) {
    SCOPED_TRACE(
        absl::StrCat("Failed for the following ChassisConfig: ", e.first));

    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(e.first, &config));
    ASSERT_FALSE(Initialized());
    ::util::Status status = VerifyChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr(e.second));
  }
}

TEST_P(BcmChassisManagerTest, ShutdownBeforeFirstConfigPush) {
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
}

TEST_P(BcmChassisManagerTest, GetPortStateAfterConfigPushAndLinkEvent) {
  const std::string kBcmChassisMapListText = R"(
      bcm_chassis_maps {
        bcm_chips {
          type: TOMAHAWK
          slot: 1
          unit: 0
          module: 0
          pci_bus: 7
          pci_slot: 1
          is_oversubscribed: true
        }
        bcm_ports {
          type: CE
          slot: 1
          port: 1
          unit: 0
          speed_bps: 100000000000
          logical_port: 34
          physical_port: 33
          diag_port: 0
          serdes_lane: 0
          num_serdes_lanes: 4
        }
      }
  )";

  const std::string kConfigText = R"(
      description: "Sample Generic Tomahawk config 32x100G ports."
      chassis {
        platform: PLT_GENERIC_TOMAHAWK
        name: "standalone"
      }
      nodes {
        id: 7654321
        slot: 1
      }
      singleton_ports {
        id: 12345
        slot: 1
        port: 1
        speed_bps: 100000000000
        node: 7654321
      }
  )";

  // WriterInterface for reporting gNMI events.
  auto gnmi_event_writer = std::make_shared<WriterMock<GnmiEventPtr>>();
  GnmiEventPtr link_up(
      new PortOperStateChangedEvent(kNodeId, kPortId, PORT_STATE_UP));
  GnmiEventPtr link_down(
      new PortOperStateChangedEvent(kNodeId, kPortId, PORT_STATE_DOWN));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load());
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              RegisterLinkscanEventWriter(
                  _, BcmSdkInterface::kLinkscanEventWriterPriorityHigh))
      .WillOnce(Return(kTestLinkscanWriterId));
  EXPECT_CALL(*phal_mock_,
              RegisterTransceiverEventWriter(
                  _, PhalInterface::kTransceiverEventWriterPriorityHigh))
      .WillOnce(Return(kTestTransceiverWriterId));
  EXPECT_CALL(*bcm_sdk_mock_, StartLinkscan(0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_node_mocks_[0], UpdatePortState(kPortId))
      .WillOnce(Return(::util::OkStatus()))
      .WillOnce(Return(::util::UnknownErrorBuilder(GTL_LOC) << "error"));
  EXPECT_CALL(*gnmi_event_writer,
              Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(link_down))))
      .WillOnce(Return(true));
  EXPECT_CALL(*gnmi_event_writer,
              Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(link_up))))
      .WillOnce(Return(true));
  EXPECT_CALL(*bcm_sdk_mock_,
              UnregisterLinkscanEventWriter(kTestLinkscanWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*phal_mock_,
              UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ShutdownAllUnits())
      .WillOnce(Return(::util::OkStatus()));

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() and verify the results.
  ASSERT_FALSE(Initialized());
  ASSERT_OK(PushChassisConfig(config));
  ASSERT_TRUE(Initialized());

  // Register gNMI event writer.
  EXPECT_OK(RegisterEventNotifyWriter(gnmi_event_writer));

  // Emulate a few link scan events linkscan event.
  TriggerLinkscanEvent(0, 34, PORT_STATE_DOWN);
  TriggerLinkscanEvent(0, 35, PORT_STATE_UP);   // unknown port
  TriggerLinkscanEvent(10, 34, PORT_STATE_UP);  // unknown unit
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_DOWN, ret.ValueOrDie());
  }
  TriggerLinkscanEvent(0, 34, PORT_STATE_UP);
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UP, ret.ValueOrDie());
  }

  // Push config again. The state of the port will not change.
  ASSERT_OK(PushChassisConfig(config));
  ASSERT_TRUE(Initialized());
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UP, ret.ValueOrDie());
  }

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetPortState(kNodeId, kPortId);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
}

TEST_P(BcmChassisManagerTest, InitializeBcmChipsSuccess) {
  // This test config has a mix of flex and non-flex ports and mgmt ports.
  const std::string kBaseBcmChassisMapText = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
        sdk_properties: "property2=5678"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
        pci_bus: 7
        pci_slot: 1
        is_oversubscribed: true
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        physical_port: 33
        diag_port: 0
        serdes_core: 8
        serdes_lane: 0
        num_serdes_lanes: 4
        flex_port: true
        tx_lane_map: 8241
        rx_lane_map: 8961
        tx_polarity_flip: 10
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 2
        unit: 0
        speed_bps: 100000000000
        logical_port: 38
        physical_port: 37
        diag_port: 4
        serdes_core: 9
        serdes_lane: 0
        num_serdes_lanes: 4
        tx_lane_map: 531
        rx_lane_map: 8961
        tx_polarity_flip: 10
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 24
        unit: 0
        speed_bps: 100000000000
        logical_port: 130
        physical_port: 125
        diag_port: 92
        module: 0
        serdes_core: 31
        serdes_lane: 0
        num_serdes_lanes: 4
        tx_lane_map: 306
        rx_lane_map: 4146
        tx_polarity_flip: 3
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 1
        unit: 0
        speed_bps: 50000000000
        logical_port: 34
        physical_port: 33
        diag_port: 0
        serdes_core: 8
        serdes_lane: 0
        num_serdes_lanes: 2
        flex_port: true
        tx_lane_map: 8241
        rx_lane_map: 8961
        tx_polarity_flip: 10
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 2
        unit: 0
        speed_bps: 50000000000
        logical_port: 36
        physical_port: 35
        diag_port: 2
        serdes_core: 8
        serdes_lane: 2
        num_serdes_lanes: 2
        flex_port: true
        tx_polarity_flip: 2
        rx_polarity_flip: 3
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 1
        unit: 0
        speed_bps: 25000000000
        logical_port: 34
        physical_port: 33
        diag_port: 0
        serdes_core: 8
        serdes_lane: 0
        num_serdes_lanes: 1
        flex_port: true
        tx_lane_map: 8241
        rx_lane_map: 8961
        tx_polarity_flip: 10
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 2
        unit: 0
        speed_bps: 25000000000
        logical_port: 35
        physical_port: 34
        diag_port: 1
        serdes_core: 8
        serdes_lane: 1
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 1
        rx_polarity_flip: 1
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 3
        unit: 0
        speed_bps: 25000000000
        logical_port: 36
        physical_port: 35
        diag_port: 2
        serdes_core: 8
        serdes_lane: 2
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 2
        rx_polarity_flip: 3
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 4
        unit: 0
        speed_bps: 25000000000
        logical_port: 37
        physical_port: 36
        diag_port: 3
        serdes_core: 8
        serdes_lane: 3
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 1
        rx_polarity_flip: 1
      }
      bcm_ports {
        type: MGMT
        slot: 1
        port: 1
        unit: 0
        speed_bps: 10000000000
        logical_port: 66
        physical_port: 129
        diag_port: 128
      }
      bcm_ports {
        type: MGMT
        slot: 1
        port: 2
        unit: 0
        speed_bps: 10000000000
        logical_port: 100
        physical_port: 131
        diag_port: 129
      }
  )";

  // This is a pruned version of the kBaseBcmChassisMapText which has
  // still both flex and non-flex ports.
  const std::string kTargetBcmChassisMapText = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
        sdk_properties: "property2=5678"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
        pci_bus: 7
        pci_slot: 1
        is_oversubscribed: true
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 1
        unit: 0
        speed_bps: 25000000000
        logical_port: 34
        physical_port: 33
        diag_port: 0
        serdes_core: 8
        serdes_lane: 0
        num_serdes_lanes: 1
        flex_port: true
        tx_lane_map: 8241
        rx_lane_map: 8961
        tx_polarity_flip: 10
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 2
        unit: 0
        speed_bps: 25000000000
        logical_port: 35
        physical_port: 34
        diag_port: 1
        serdes_core: 8
        serdes_lane: 1
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 1
        rx_polarity_flip: 1
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 3
        unit: 0
        speed_bps: 25000000000
        logical_port: 36
        physical_port: 35
        diag_port: 2
        serdes_core: 8
        serdes_lane: 2
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 2
        rx_polarity_flip: 3
      }
      bcm_ports {
        type: XE
        slot: 1
        port: 1
        channel: 4
        unit: 0
        speed_bps: 25000000000
        logical_port: 37
        physical_port: 36
        diag_port: 3
        serdes_core: 8
        serdes_lane: 3
        num_serdes_lanes: 1
        flex_port: true
        tx_polarity_flip: 1
        rx_polarity_flip: 1
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 24
        unit: 0
        speed_bps: 100000000000
        logical_port: 130
        physical_port: 125
        diag_port: 92
        module: 0
        serdes_core: 31
        serdes_lane: 0
        num_serdes_lanes: 4
        tx_lane_map: 306
        rx_lane_map: 4146
        tx_polarity_flip: 3
        rx_polarity_flip: 15
      }
      bcm_ports {
        type: MGMT
        slot: 1
        port: 1
        unit: 0
        speed_bps: 10000000000
        logical_port: 66
        physical_port: 129
        diag_port: 128
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_CALL(*bcm_sdk_mock_, FindUnit(0, 7, 1, BcmChip::TOMAHAWK))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeUnit(0, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetModuleId(0, 0))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 34))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 35))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 36))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 37))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 130))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializePort(0, 66))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, StartDiagShellServer())
      .WillOnce(Return(::util::OkStatus()));

  // Setup test base_bcm_chassis_map and target_bcm_chassis_map.
  BcmChassisMap base_bcm_chassis_map, target_bcm_chassis_map;
  ASSERT_OK(
      ParseProtoFromString(kBaseBcmChassisMapText, &base_bcm_chassis_map));
  ASSERT_OK(
      ParseProtoFromString(kTargetBcmChassisMapText, &target_bcm_chassis_map));

  // Call InitializeBcmChips() and verify the results.
  ASSERT_FALSE(Initialized());
  ASSERT_OK(InitializeBcmChips(base_bcm_chassis_map, target_bcm_chassis_map));
  ASSERT_FALSE(Initialized());
  std::string bcm_sdk_config;
  /* BCM SDK only
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("property1=1234"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("property2=5678"));
  EXPECT_THAT(bcm_sdk_config,
              HasSubstr("pbmp_xport_xe.0=0x400000000000000000000003C00000000"));
  EXPECT_THAT(
      bcm_sdk_config,
      HasSubstr("pbmp_oversubscribe.0=0x400000000000000000000003C00000000"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_34.0=33:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_34.0=0"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("xgxs_tx_lane_map_xe0.0=0x2031"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("xgxs_rx_lane_map_xe0.0=0x2301"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_tx_polarity_flip_xe0.0=0xA"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_rx_polarity_flip_xe0.0=0xF"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_35.0=34:25:i"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_35.0=1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_tx_polarity_flip_xe1.0=0x1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_rx_polarity_flip_xe1.0=0x1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_36.0=35:50:i"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_36.0=2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_tx_polarity_flip_xe2.0=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_rx_polarity_flip_xe2.0=0x3"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_37.0=36:25:i"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_37.0=3"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_tx_polarity_flip_xe3.0=0x1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("phy_xaui_rx_polarity_flip_xe3.0=0x1"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_130.0=125:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_130.0=92"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("xgxs_tx_lane_map_xe92.0=0x132"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("xgxs_rx_lane_map_xe92.0=0x1032"));
  EXPECT_THAT(bcm_sdk_config,
              HasSubstr("phy_xaui_tx_polarity_flip_xe92.0=0x3"));
  EXPECT_THAT(bcm_sdk_config,
              HasSubstr("phy_xaui_rx_polarity_flip_xe92.0=0xF"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_66.0=129:10"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_66.0=128"));
  */
}

TEST_P(BcmChassisManagerTest, InitializeBcmChipsFailure) {
  // Simple base_bcm_chassis_map with one chip and one port.
  const std::string kBaseBcmChassisMapText = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 4
      }
  )";

  // target_bcm_chassis_map with an ID, while the base does not have one.
  const std::string kTargetBcmChassisMapText1 = R"(
      id: "TEST"
      bcm_chassis {
        sdk_properties: "property1=1234"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 4
      }
  )";

  // target_bcm_chassis_map with no bcm_chassis.
  const std::string kTargetBcmChassisMapText2 = R"(
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 4
      }
  )";

  // target_bcm_chassis_map with unknown unit.
  const std::string kTargetBcmChassisMapText3 = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 10
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 4
      }
  )";

  // target_bcm_chassis_map with unknown port.
  const std::string kTargetBcmChassisMapText4 = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 2
        unit: 0
        speed_bps: 100000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 4
      }
  )";

  // target_bcm_chassis_map with a channelized port, which the port in the
  // base is non-channelized.
  const std::string kTargetBcmChassisMapText5 = R"(
      bcm_chassis {
        sdk_properties: "property1=1234"
      }
      bcm_chips {
        type: TOMAHAWK
        slot: 1
        unit: 0
        module: 0
      }
      bcm_ports {
        type: CE
        slot: 1
        port: 1
        channel: 1
        unit: 0
        speed_bps: 25000000000
        logical_port: 34
        serdes_lane: 0
        num_serdes_lanes: 1
      }
  )";

  const std::vector<std::string> bad_target_bcm_chassis_map_texts = {
      kTargetBcmChassisMapText1, kTargetBcmChassisMapText2,
      kTargetBcmChassisMapText3, kTargetBcmChassisMapText4,
      kTargetBcmChassisMapText5};

  // Setup test base_bcm_chassis_map and target_bcm_chassis_map.
  BcmChassisMap base_bcm_chassis_map, target_bcm_chassis_map;
  ASSERT_OK(
      ParseProtoFromString(kBaseBcmChassisMapText, &base_bcm_chassis_map));

  // Call Initialize() and verify the results for bad target_bcm_chassis_maps.
  ASSERT_FALSE(Initialized());
  for (const std::string& text : bad_target_bcm_chassis_map_texts) {
    ASSERT_OK(ParseProtoFromString(text, &target_bcm_chassis_map));
    EXPECT_NE(::util::OkStatus(),
              InitializeBcmChips(base_bcm_chassis_map, target_bcm_chassis_map));
    EXPECT_FALSE(Initialized());
  }

  // Now give the base as target (which is valid). However assume one of the
  // SDK calls is failing.
  target_bcm_chassis_map = base_bcm_chassis_map;
  ::util::Status error(StratumErrorSpace(), ERR_UNKNOWN, "Test");
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(error));
  EXPECT_CALL(*bcm_sdk_mock_, GenerateBcmConfigFile(_, _, _))
      .WillRepeatedly(Return(std::string("")));
  EXPECT_EQ(error,
            InitializeBcmChips(base_bcm_chassis_map, target_bcm_chassis_map));
  EXPECT_FALSE(Initialized());
}

TEST_P(BcmChassisManagerTest, TestSendTransceiverGnmiEvent) {
  ASSERT_OK(PushTestConfig());

  // Create and register writer for sending events.
  auto writer = std::make_shared<WriterMock<GnmiEventPtr>>();
  ASSERT_OK(RegisterEventNotifyWriter(writer));

  // Test successful Write() with new state to writer.
  GnmiEventPtr event(
      new PortOperStateChangedEvent(kNodeId, 1234, PORT_STATE_UP));
  EXPECT_CALL(*writer, Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(event))))
      .WillOnce(Return(true));
  SendPortOperStateGnmiEvent(kNodeId, 1234, PORT_STATE_UP);
  Mock::VerifyAndClear(writer.get());

  // Test failed Write() results in unregistering of writer.
  ::util::Status closed_status = MAKE_ERROR(ERR_CANCELLED) << "test_error";
  EXPECT_CALL(*writer, Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(event))))
      .WillOnce(Return(false));
  SendPortOperStateGnmiEvent(kNodeId, 1234, PORT_STATE_UP);
  SendPortOperStateGnmiEvent(kNodeId, 1234, PORT_STATE_UP);

  ASSERT_OK(UnregisterEventNotifyWriter());

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetTrunkMemberBlockStateByController) {
  ASSERT_OK(PushTestConfig());

  // We have one static trunk and one LACP trunk:
  // static: (node_id: 7654321, trunk_id: 222, port_id: 12345)
  // LACP: (node_id: 7654321, trunk_id: 333, port_id: NONE)

  // TODO(unknown): Extend the tests when the function is implemneted.
  // EXPECT_OK(SetTrunkMemberBlockState(kNodeId, 222, kPortId,
  //                                    TRUNK_MEMBER_BLOCK_STATE_BLOCKED));
  // EXPECT_OK(SetTrunkMemberBlockState(kNodeId, 0, kPortId,
  //                                    TRUNK_MEMBER_BLOCK_STATE_FORWARDING));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetPortAdminStateViaConfigPush) {
  ChassisConfig config;

  // Push a config which sets the admin state to enabled.
  ASSERT_OK(PushTestConfig(&config));

  // Check that port admin state is up.
  auto admin_state = GetPortAdminState(kNodeId, kPortId);
  ASSERT_TRUE(admin_state.ok());
  EXPECT_EQ(ADMIN_STATE_ENABLED, admin_state.ValueOrDie());

  // Change the config and set the adming state to down. Then re-push.
  for (auto& singleton_port : *config.mutable_singleton_ports()) {
    singleton_port.mutable_config_params()->set_admin_state(
        ADMIN_STATE_DISABLED);
  }

  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));

  ASSERT_OK(VerifyChassisConfig(config));
  ASSERT_OK(PushChassisConfig(config));

  admin_state = GetPortAdminState(kNodeId, kPortId);
  ASSERT_TRUE(admin_state.ok());
  EXPECT_EQ(ADMIN_STATE_DISABLED, admin_state.ValueOrDie());

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetPortLoopbackStateViaConfigPush) {
  ChassisConfig config;

  // Push a config which does not set the loopback state.
  ASSERT_OK(PushTestConfig(&config));

  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  // Check that port loopback state is undefined/unknown.
  auto loopback_state = GetPortLoopbackState(kNodeId, kPortId);
  ASSERT_TRUE(loopback_state.ok());
  EXPECT_EQ(LOOPBACK_STATE_UNKNOWN, loopback_state.ValueOrDie());

  // Change the config and set the loopback state to MAC. Then re-push.
  for (auto& singleton_port : *config.mutable_singleton_ports()) {
    singleton_port.mutable_config_params()->set_loopback_mode(
        LOOPBACK_STATE_MAC);
  }

  ASSERT_OK(VerifyChassisConfig(config));
  ASSERT_OK(PushChassisConfig(config));

  loopback_state = GetPortLoopbackState(kNodeId, kPortId);
  ASSERT_TRUE(loopback_state.ok());
  EXPECT_EQ(LOOPBACK_STATE_MAC, loopback_state.ValueOrDie());

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetPortAdminStateByController) {
  ASSERT_OK(PushTestConfig());

  // TODO(unknown): Extend the tests when the function is implemented.
  // EXPECT_OK(SetPortAdminState(kNodeId, kPortId, ADMIN_STATE_DISABLED));
  // EXPECT_OK(SetPortAdminState(kNodeId, kPortId, ADMIN_STATE_ENABLED));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetPortLoopbackStateByController) {
  ASSERT_OK(PushTestConfig());

  EXPECT_CALL(*bcm_sdk_mock_, SetPortOptions(0, 34, _))
      .WillRepeatedly(Return(::util::OkStatus()));

  // TODO(unknown): Extend the tests.
  EXPECT_OK(SetPortLoopbackState(kNodeId, kPortId, LOOPBACK_STATE_NONE));
  auto loopback_state = GetPortLoopbackState(kNodeId, kPortId);
  ASSERT_TRUE(loopback_state.ok());
  EXPECT_EQ(LOOPBACK_STATE_NONE, loopback_state.ValueOrDie());

  EXPECT_OK(SetPortLoopbackState(kNodeId, kPortId, LOOPBACK_STATE_MAC));
  EXPECT_OK(SetPortLoopbackState(kNodeId, kPortId, LOOPBACK_STATE_UNKNOWN));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_P(BcmChassisManagerTest, TestSetPortHealthStateByController) {
  ASSERT_OK(PushTestConfig());

  // TODO(unknown): Extend the tests when the function is implemented.
  // EXPECT_OK(SetPortHealthState(kNodeId, kPortId, HEALTH_STATE_BAD));
  // EXPECT_OK(SetPortHealthState(kNodeId, kPortId, HEALTH_STATE_GOOD));

  ASSERT_OK(ShutdownAndTestCleanState());
}

INSTANTIATE_TEST_SUITE_P(BcmChassisManagerTestWithMode, BcmChassisManagerTest,
                         ::testing::Values(OPERATION_MODE_STANDALONE));

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
