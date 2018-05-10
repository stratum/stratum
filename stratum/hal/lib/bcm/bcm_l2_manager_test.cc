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


#include "stratum/hal/lib/bcm/bcm_l2_manager.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager_mock.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace bcm {

MATCHER_P(DerivedFromStatus, status, "") {
  if (arg.error_code() != status.error_code()) {
    return false;
  }
  if (arg.error_message().find(status.error_message()) == std::string::npos) {
    *result_listener << "\nOriginal error string: \"" << status.error_message()
                     << "\" is missing from the actual status.";
    return false;
  }
  return true;
}

class BcmL2ManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bcm_chassis_manager_mock_ = absl::make_unique<BcmChassisManagerMock>();
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_l2_manager_ = BcmL2Manager::CreateInstance(
        bcm_chassis_manager_mock_.get(), bcm_sdk_mock_.get(), kUnit);
  }

  void VerifyPromotedMyStationEntry(int vlan, int station_id, bool must_exist) {
    std::tuple<int, uint64, int> tuple =
        std::make_tuple(vlan, 0, BcmL2Manager::kPromotedMyStationEntryPriority);
    auto it = bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.find(tuple);
    if (!must_exist) {
      EXPECT_EQ(bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.end(),
                it);
    } else {
      ASSERT_NE(bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.end(),
                it);
      EXPECT_EQ(station_id, it->second);
    }
  }

  void VerifyRegularMyStationEntry(int vlan, uint64 dst_mac, int station_id,
                                   bool must_exist) {
    std::tuple<int, uint64, int> tuple = std::make_tuple(
        vlan, dst_mac, BcmL2Manager::kRegularMyStationEntryPriority);
    auto it = bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.find(tuple);
    if (!must_exist) {
      EXPECT_EQ(bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.end(),
                it);
    } else {
      ASSERT_NE(bcm_l2_manager_->vlan_dst_mac_priority_to_station_id_.end(),
                it);
      EXPECT_EQ(station_id, it->second);
    }
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, "Some error");
  }

  bool l2_learning_disabled_for_default_vlan() {
    return bcm_l2_manager_->l2_learning_disabled_for_default_vlan_;
  }

  static constexpr uint64 kNodeId = 123123123;
  static constexpr int kUnit = 0;
  static constexpr int kStationId = 10;
  static constexpr uint64 kDstMac = 0x123456789012;

  std::unique_ptr<BcmChassisManagerMock> bcm_chassis_manager_mock_;
  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmL2Manager> bcm_l2_manager_;
};

constexpr uint64 BcmL2ManagerTest::kNodeId;
constexpr int BcmL2ManagerTest::kUnit;
constexpr int BcmL2ManagerTest::kStationId;
constexpr uint64 BcmL2ManagerTest::kDstMac;

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForNoNodeConfigParams) {
  // Setup a test empty config with no node config param.
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForEmptyNodeConfigParams) {
  // Setup a test config with an empty node config param.
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  node->mutable_config_params();

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForEmptyVlanConfig) {
  // Setup a test config with an empty/default VLAN config.
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  node->mutable_config_params()->add_vlan_configs();

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlan(kUnit, kArpVlan))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));

  // The first config push will add the station entry and the second one does
  // not.
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForNonEmptyVlanConfig) {
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  auto* vlan_config = node->mutable_config_params()->add_vlan_configs();

  // We do the following:
  // 1- We push a config which enables L2 on node1. There is no my station entry
  //    so we do not delete anything.
  // 2- We push another config that then disables L2 on node1. This will add
  //    a my station entry
  // 3- We push the config one more time and add l2_age_duration_sec for the
  //    vlan config.
  // 4- We push the config one more time and enable L2. This time we remove the
  //    my station entry as well.

  // 1st config push
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlan(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // 2nd config push
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, kDefaultVlan, 0ULL, _))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_TRUE(l2_learning_disabled_for_default_vlan());

  // 3rd config push
  auto* l2_config = node->mutable_config_params()->mutable_l2_config();
  l2_config->set_l2_age_duration_sec(300);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetL2AgeTimer(kUnit, 300))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_TRUE(l2_learning_disabled_for_default_vlan());

  // 4th config push
  node->mutable_config_params()->clear_vlan_configs();
  node->mutable_config_params()->add_vlan_configs();
  node->mutable_config_params()->clear_l2_config();

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlan(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigFailure) {
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  auto vlan_config = node->mutable_config_params()->add_vlan_configs();

  // Failue when L2 is enabled -- scenario 1
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 2
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 3
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 4
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlan(kUnit, kArpVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 1
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);
  auto* l2_config = node->mutable_config_params()->mutable_l2_config();
  l2_config->set_l2_age_duration_sec(300);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 2
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 3
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 4
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 5
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, kDefaultVlan, 0ULL, _))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 6
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, kDefaultVlan, 0ULL, _))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 7
  // AddMyStationEntry will not be called from now on
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 8
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 9
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetL2AgeTimer(kUnit, 300))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_TRUE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  auto* vlan_config = node->mutable_config_params()->add_vlan_configs();
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, kDefaultVlan, 0ULL, _))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));

  // Verify before config push, push config, then verify again.
  EXPECT_OK(bcm_l2_manager_->VerifyChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l2_manager_->VerifyChassisConfig(config, kNodeId));

  // Now mutate the config and verify one more time.
  vlan_config = node->mutable_config_params()->add_vlan_configs();
  vlan_config->set_vlan_id(kArpVlan + 1);
  EXPECT_OK(bcm_l2_manager_->VerifyChassisConfig(config, kNodeId));
}

TEST_F(BcmL2ManagerTest, VerifyChassisConfigFailure) {
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  node->mutable_config_params()->add_vlan_configs();

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlan(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  // Do a config push to setup environment.
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));

  // Verify failure for invalid node
  ::util::Status status = bcm_l2_manager_->VerifyChassisConfig(config, 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid node ID"));

  // Change in the node_id after config push is reboot required.
  status = bcm_l2_manager_->VerifyChassisConfig(config, kNodeId + 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected a change in the node_id"));

  // vlan given more than once.
  auto* vlan_config = node->mutable_config_params()->add_vlan_configs();
  vlan_config->set_vlan_id(kDefaultVlan);
  status = bcm_l2_manager_->VerifyChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("vlan 1 has been given more than once"));

  // Special ARP vlan is given.
  vlan_config->set_vlan_id(kArpVlan);
  status = bcm_l2_manager_->VerifyChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("You specified config for the special ARP vlan"));
}

TEST_F(BcmL2ManagerTest, Shutdown) {
  // Shutdown before config push.
  EXPECT_OK(bcm_l2_manager_->Shutdown());
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, false);

  // Shutdown after config push.
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  auto* vlan_config = node->mutable_config_params()->add_vlan_configs();
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, kDefaultVlan, 0ULL, _))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyPromotedMyStationEntry(kDefaultVlan, kStationId, true);
  EXPECT_OK(bcm_l2_manager_->Shutdown());
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
}

TEST_F(BcmL2ManagerTest, InsertThenDeleteMyStationEntrySuccess) {
  BcmFlowEntry bcm_flow_entry;

  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
  }

  // First add the entry.
  EXPECT_CALL(*bcm_sdk_mock_, AddMyStationEntry(kUnit, 0, kDstMac, _))
      .WillOnce(Return(kStationId));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyPromotedMyStationEntry(0, 0, false);
  VerifyRegularMyStationEntry(0, kDstMac, kStationId, true);

  // Then remove the same entry.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry));
  VerifyPromotedMyStationEntry(kDefaultVlan, 0, false);
  VerifyRegularMyStationEntry(kDefaultVlan, kDstMac, 0, false);

  // Now add a new entry for a different VLAN.
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::VLAN_VID);
    field->mutable_value()->set_u32(kDefaultVlan + 1);  // != kDefaultVlan
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              AddMyStationEntry(kUnit, kDefaultVlan + 1, kDstMac, _))
      .WillOnce(Return(kStationId + 1));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyPromotedMyStationEntry(kDefaultVlan + 1, 0, false);
  VerifyRegularMyStationEntry(kDefaultVlan + 1, kDstMac, kStationId + 1, true);
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailure) {
  // TODO: Add this.
}

TEST_F(BcmL2ManagerTest, InsertThenDeleteMulticastGroupSuccess) {
  BcmFlowEntry bcm_flow_entry;

  // NOOP at the moment.
  EXPECT_OK(bcm_l2_manager_->InsertMulticastGroup(bcm_flow_entry));
  EXPECT_OK(bcm_l2_manager_->DeleteMulticastGroup(bcm_flow_entry));
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
