// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager_mock.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/channel/channel_mock.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"

DECLARE_string(base_bcm_chassis_map_file);
DECLARE_string(bcm_sdk_config_file);
DECLARE_string(bcm_sdk_config_flush_file);
DECLARE_string(bcm_sdk_shell_log_file);
DECLARE_string(bcm_sdk_checkpoint_dir);
DECLARE_string(test_tmpdir);

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace bcm {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

// TODO: Investigate moving the test protos into a testdata folder.
class BcmChassisManagerTest : public ::testing::TestWithParam<OperationMode> {
 protected:
  BcmChassisManagerTest() {
    FLAGS_base_bcm_chassis_map_file =
        FLAGS_test_tmpdir + "/base_bcm_chassis_map.pb.txt";
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
  }

  bool Initialized() {
    return bcm_chassis_manager_->initialized_;
  }

  ::util::Status InitializeBcmChips(BcmChassisMap base_bcm_chassis_map,
                                    BcmChassisMap target_bcm_chassis_map) {
    return bcm_chassis_manager_->InitializeBcmChips(base_bcm_chassis_map,
                                                    target_bcm_chassis_map);
  }

  void TriggerLinkScanEvent(int unit, int logical_port, PortState state) {
    bcm_chassis_manager_->LinkscanEventHandler(unit, logical_port, state);
  }

  void CheckCleanInternalState() {
    ASSERT_TRUE(bcm_chassis_manager_->unit_to_bcm_chip_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->slot_port_channel_to_bcm_port_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->slot_port_to_flex_bcm_ports_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->slot_port_to_non_flex_bcm_ports_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->slot_port_to_transceiver_state_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->unit_to_logical_ports_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->node_id_to_unit_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->node_id_to_port_ids_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->port_id_to_slot_port_channel_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->unit_logical_port_to_port_id_.empty());
    ASSERT_TRUE(bcm_chassis_manager_->slot_port_channel_to_port_state_.empty());
    ASSERT_EQ(nullptr, bcm_chassis_manager_->base_bcm_chassis_map_);
    ASSERT_EQ(nullptr, bcm_chassis_manager_->applied_bcm_chassis_map_);
    ASSERT_EQ(nullptr, bcm_chassis_manager_->xcvr_event_channel_);
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

  ::util::StatusOr<BcmChip> GetBcmChip(int unit) {
    return bcm_chassis_manager_->GetBcmChip(unit);
  }

  ::util::StatusOr<BcmPort> GetBcmPort(int slot, int port, int channel) {
    return bcm_chassis_manager_->GetBcmPort(slot, port, channel);
  }

  ::util::StatusOr<std::map<uint64, int>> GetNodeIdToUnitMap() {
    return bcm_chassis_manager_->GetNodeIdToUnitMap();
  }

  ::util::StatusOr<std::map<uint64, std::pair<int, int>>>
  GetPortIdToUnitLogicalPortMap() {
    return bcm_chassis_manager_->GetPortIdToUnitLogicalPortMap();
  }

  ::util::StatusOr<std::map<uint64, std::pair<int, int>>>
  GetTrunkIdToUnitTrunkPortMap() {
    return bcm_chassis_manager_->GetTrunkIdToUnitTrunkPortMap();
  }

  ::util::StatusOr<PortState> GetPortState(uint64 port_id) {
    return bcm_chassis_manager_->GetPortState(port_id);
  }

  ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
    return bcm_chassis_manager_->RegisterEventNotifyWriter(writer);
  }

  void SendPortOperStateGnmiEvent(int node_id, int port_id, PortState state) {
    absl::ReaderMutexLock l(&chassis_lock);
    bcm_chassis_manager_->SendPortOperStateGnmiEvent(node_id, port_id, state);
  }

  bool IsInternalPort(std::pair<int, int> slot_port_pair) const {
    return bcm_chassis_manager_->IsInternalPort(slot_port_pair);
  }

  OperationMode mode_;
  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmSerdesDbManagerMock> bcm_serdes_db_manager_mock_;
  std::unique_ptr<BcmChassisManager> bcm_chassis_manager_;
  static constexpr int kTestLinkscanWriterId = 10;
  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_P(BcmChassisManagerTest, PreFirstConfigPushState) {
  CheckCleanInternalState();
  EXPECT_FALSE(Initialized());
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
    auto ret = GetNodeIdToUnitMap();
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }
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
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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
  }
  {
    // (slot: 1, port: 1) needs to be in the internal map.
    auto ret = GetBcmPort(1, 1, 0);
    ASSERT_TRUE(ret.ok());
    const auto& bcm_port = ret.ValueOrDie();
    EXPECT_EQ(kHundredGigBps, bcm_port.speed_bps());
    EXPECT_EQ(34, bcm_port.logical_port());

    // (slot: 1, port: 1, channel: 1) is not in the internal map.
    ret = GetBcmPort(1, 1, 1);
    ASSERT_FALSE(ret.ok());
  }
  {
    auto ret = GetNodeIdToUnitMap();
    ASSERT_TRUE(ret.ok());
    const auto& node_id_to_unit = ret.ValueOrDie();
    ASSERT_EQ(1U, node_id_to_unit.size());
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 34), pord_id_to_unit_logical_port.at(12345UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(33333UL);
    ASSERT_FALSE(ret.ok());
  }

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  CheckCleanInternalState();
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
    auto ret = GetPortState(12345UL);
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

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 34), pord_id_to_unit_logical_port.at(12345UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(12346UL);
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
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(2U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 34), pord_id_to_unit_logical_port.at(12345UL));
    EXPECT_EQ(std::make_pair(0, 36), pord_id_to_unit_logical_port.at(12346UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());
  }

  EXPECT_FALSE(IsInternalPort({1, 1}));

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  CheckCleanInternalState();
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
    auto ret = GetPortState(12345UL);
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
      }
  )";

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 1), pord_id_to_unit_logical_port.at(12345UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(33333UL);
    ASSERT_FALSE(ret.ok());
  }
  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.0=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.0=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.0=33:100"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.0=0"));

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
    auto ret = GetPortState(12345UL);
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

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 1), pord_id_to_unit_logical_port.at(12345UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(12346UL);
    ASSERT_FALSE(ret.ok());
  }
  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
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
    EXPECT_EQ(0, node_id_to_unit.at(7654321UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(2U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(0, 1), pord_id_to_unit_logical_port.at(12345UL));
    EXPECT_EQ(std::make_pair(0, 3), pord_id_to_unit_logical_port.at(12346UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());
  }
  bcm_sdk_config.clear();
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
    auto ret = GetPortState(12345UL);
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
        id: 8765432
        slot: 9
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
    ASSERT_EQ(2U, node_id_to_unit.size());
    EXPECT_EQ(2, node_id_to_unit.at(7654321UL));
    EXPECT_EQ(-1, node_id_to_unit.at(8765432UL));
  }
  {
    auto ret = GetPortIdToUnitLogicalPortMap();
    ASSERT_TRUE(ret.ok());
    const auto& pord_id_to_unit_logical_port = ret.ValueOrDie();
    ASSERT_EQ(1U, pord_id_to_unit_logical_port.size());
    EXPECT_EQ(std::make_pair(2, 1), pord_id_to_unit_logical_port.at(12345UL));
  }
  {
    // State for a known port right after config is unknown.
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UNKNOWN, ret.ValueOrDie());

    // State for a unknown port returns an error.
    ret = GetPortState(33333UL);
    ASSERT_FALSE(ret.ok());
  }

  // Verify config.bcm in this case. Logical ports start from 1 and go and up.
  std::string bcm_sdk_config;
  ASSERT_OK(ReadFileToString(FLAGS_bcm_sdk_config_file, &bcm_sdk_config));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_xport_xe.2=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("pbmp_oversubscribe.2=0x2"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("portmap_1.2=61:40"));
  EXPECT_THAT(bcm_sdk_config, HasSubstr("dport_map_port_1.2=64"));

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
    auto ret = GetPortState(12345UL);
    ASSERT_FALSE(ret.ok());
    EXPECT_THAT(ret.status().error_message(), HasSubstr("Not initialized"));
  }

  EXPECT_FALSE(IsInternalPort({9, 80}));
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
          serdes_lane: 2
          num_serdes_lanes: 2
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
          serdes_lane: 2
          num_serdes_lanes: 2
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
          serdes_lane: 2
          num_serdes_lanes: 2
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
          serdes_lane: 2
          num_serdes_lanes: 2
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
          serdes_lane: 2
          num_serdes_lanes: 2
          internal: true
        }
     }
  )";
  const std::string kChassisMapError11 =
      "auto_add_logical_ports is False and logical_port is not positive";

  // Valid chassis config.
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

  // Chassis config with more than one slot for nodes and ports.
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
        slot: 10
        port: 80
        speed_bps: 40000000000
        node: 7654321
      }
  )";
  const std::string kConfigError2 =
      "auto_add_slot is true and we have more than one slot";

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
        id: 0xFFFFFFFD
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
      "was already recorded for another SingletonPort in the config";

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
      "was already recorded for another SingletonPort in the config";

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
      "Could not find any BcmPort in base_bcm_chassis_map  whose (slot, port, "
      "channel, speed_bps) tuple matches non-flex SingletonPort ";

  const std::vector<std::pair<std::string, std::string>>
      bad_bcm_chassis_map_texts_to_errors = {
          {kBcmChassisMapListText2, kChassisMapError2},
          {kBcmChassisMapListText3, kChassisMapError3},
          {kBcmChassisMapListText4, kChassisMapError4},
          {kBcmChassisMapListText5, kChassisMapError5},
          {kBcmChassisMapListText6, kChassisMapError6},
          {kBcmChassisMapListText7, kChassisMapError7},
          {kBcmChassisMapListText8, kChassisMapError8},
          {kBcmChassisMapListText9, kChassisMapError9},
          {kBcmChassisMapListText10, kChassisMapError10},
          {kBcmChassisMapListText11, kChassisMapError11}};

  const std::vector<std::pair<std::string, std::string>>
      bad_config_texts_to_errors = {
          {kConfigText2, kConfigError2}, {kConfigText3, kConfigError3},
          {kConfigText4, kConfigError4}, {kConfigText5, kConfigError5},
          {kConfigText6, kConfigError6}, {kConfigText7, kConfigError7},
          {kConfigText8, kConfigError8}, {kConfigText9, kConfigError9}};

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillRepeatedly(Return(::util::OkStatus()));

  // Call PushChassisConfig() for all the bad configs and verify the results.
  for (const auto& e : bad_bcm_chassis_map_texts_to_errors) {
    SCOPED_TRACE(
        absl::StrCat("Failed for the following BcmChassisMap: ", e.first));

    // Valid ChassisConfig but invalid BcmChassisMapList
    ASSERT_OK(WriteStringToFile(e.first, FLAGS_base_bcm_chassis_map_file));
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(kConfigText1, &config));
    ASSERT_FALSE(Initialized());
    ::util::Status status = PushChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr(e.second));
  }
  for (const auto& e : bad_config_texts_to_errors) {
    SCOPED_TRACE(
        absl::StrCat("Failed for the following ChassisConfig: ", e.first));

    // Invalid ChassisConfig but valid BcmChassisMapList
    ASSERT_OK(WriteStringToFile(kBcmChassisMapListText1,
                                FLAGS_base_bcm_chassis_map_file));
    ChassisConfig config;
    ASSERT_OK(ParseProtoFromString(e.first, &config));
    ASSERT_FALSE(Initialized());
    ::util::Status status = PushChassisConfig(config);
    ASSERT_FALSE(status.ok());
    EXPECT_THAT(status.error_message(), HasSubstr(e.second));
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
      "Could not find any BcmPort in base_bcm_chassis_map  whose (slot, port, "
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
      "Could not find any BcmPort in base_bcm_chassis_map  whose (slot, port, "
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

  const std::vector<std::pair<std::string, std::string>>
      bad_config_texts_to_errors = {{kConfigText1, kConfigError1},
                                    {kConfigText2, kConfigError2},
                                    {kConfigText3, kConfigError3},
                                    {kConfigText4, kConfigError4}};

  // Write the kBcmChassisMapListText to FLAGS_base_bcm_chassis_map_file.
  ASSERT_OK(WriteStringToFile(kBcmChassisMapListText,
                              FLAGS_base_bcm_chassis_map_file));

  for (const auto& e : bad_config_texts_to_errors) {
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

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load());
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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

  // Emulate a few link scan events linkscan event.
  TriggerLinkScanEvent(0, 34, PORT_STATE_UP);
  TriggerLinkScanEvent(0, 35, PORT_STATE_UP);  // unknown port
  {
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UP, ret.ValueOrDie());
  }

  // Push config again. The state of the port will not change.
  ASSERT_OK(PushChassisConfig(config));
  ASSERT_TRUE(Initialized());
  {
    auto ret = GetPortState(12345UL);
    ASSERT_TRUE(ret.ok());
    EXPECT_EQ(PORT_STATE_UP, ret.ValueOrDie());
  }

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  ASSERT_FALSE(Initialized());
  {
    auto ret = GetPortState(12345UL);
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
  EXPECT_EQ(error,
            InitializeBcmChips(base_bcm_chassis_map, target_bcm_chassis_map));
  EXPECT_FALSE(Initialized());
}

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

TEST_P(BcmChassisManagerTest, TestSendTransceiverGnmiEvent) {
  // Push config from test:
  // PushChassisConfigSuccessWithoutAutoAddLogicalPortsWithoutFlexPorts.
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
      }
  )";

  // Expectations for the mock objects on initialization.
  EXPECT_CALL(*bcm_serdes_db_manager_mock_, Load())
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, InitializeSdk(FLAGS_bcm_sdk_config_file,
                                            FLAGS_bcm_sdk_config_flush_file,
                                            FLAGS_bcm_sdk_shell_log_file))
      .WillOnce(Return(::util::OkStatus()));
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

  // Setup a test config and pass it to PushChassisConfig.
  ChassisConfig config;
  ASSERT_OK(ParseProtoFromString(kConfigText, &config));

  // Call PushChassisConfig() verify the results.
  ASSERT_FALSE(Initialized());
  ASSERT_OK(PushChassisConfig(config));
  ASSERT_TRUE(Initialized());

  // Create and register writer for sending events.
  auto writer = std::make_shared<WriterMock<GnmiEventPtr>>();
  ASSERT_OK(bcm_chassis_manager_->RegisterEventNotifyWriter(writer));

  // Test successful Write() with new state to writer.
  GnmiEventPtr event(
      new PortOperStateChangedEvent(7654321, 1234, PORT_STATE_UP));
  EXPECT_CALL(*writer, Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(event))))
      .WillOnce(Return(true));
  SendPortOperStateGnmiEvent(7654321, 1234, PORT_STATE_UP);
  Mock::VerifyAndClear(writer.get());

  // Test failed Write() results in unregistering of writer.
  ::util::Status closed_status = MAKE_ERROR(ERR_CANCELLED) << "test_error";
  EXPECT_CALL(*writer, Write(Matcher<const GnmiEventPtr&>(GnmiEventEq(event))))
      .WillOnce(Return(false));
  SendPortOperStateGnmiEvent(7654321, 1234, PORT_STATE_UP);
  SendPortOperStateGnmiEvent(7654321, 1234, PORT_STATE_UP);

  // Now shutdown and verify things are all reset after shutdown.
  ASSERT_OK(Shutdown());
  CheckCleanInternalState();
}

INSTANTIATE_TEST_CASE_P(BcmChassisManagerTestWithMode, BcmChassisManagerTest,
                        ::testing::Values(OPERATION_MODE_STANDALONE));

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
