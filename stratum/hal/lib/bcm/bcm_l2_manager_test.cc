// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_l2_manager.h"

#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_mock.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace bcm {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

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
    bcm_chassis_ro_mock_ = absl::make_unique<BcmChassisRoMock>();
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_l2_manager_ = BcmL2Manager::CreateInstance(bcm_chassis_ro_mock_.get(),
                                                   bcm_sdk_mock_.get(), kUnit);
  }

  void VerifyL3PromoteMyStationEntry(int station_id, bool must_exist) {
    BcmL2Manager::MyStationEntry entry(
        BcmL2Manager::kL3PromoteMyStationEntryPriority, kDefaultVlan, 0xfff,
        0ULL, kNonMulticastDstMacMask);
    auto it = bcm_l2_manager_->my_station_entry_to_station_id_.find(entry);
    if (!must_exist) {
      EXPECT_EQ(bcm_l2_manager_->my_station_entry_to_station_id_.end(), it);
    } else {
      ASSERT_NE(bcm_l2_manager_->my_station_entry_to_station_id_.end(), it);
      EXPECT_EQ(station_id, it->second);
    }
  }

  void VerifyRegularMyStationEntry(int vlan, int vlan_mask, uint64 dst_mac,
                                   uint64 dst_mac_mask, int station_id,
                                   bool must_exist) {
    BcmL2Manager::MyStationEntry entry(
        BcmL2Manager::kRegularMyStationEntryPriority, vlan, vlan_mask, dst_mac,
        dst_mac_mask);
    auto it = bcm_l2_manager_->my_station_entry_to_station_id_.find(entry);
    if (!must_exist) {
      EXPECT_EQ(bcm_l2_manager_->my_station_entry_to_station_id_.end(), it);
    } else {
      ASSERT_NE(bcm_l2_manager_->my_station_entry_to_station_id_.end(), it);
      EXPECT_EQ(station_id, it->second);
    }
  }

  ::util::Status DefaultError() {
    return ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, "Some error");
  }

  bool l2_learning_disabled_for_default_vlan() {
    return bcm_l2_manager_->l2_learning_disabled_for_default_vlan_;
  }

  bool MyStationEntryEqual(int priority1, int vlan1, int vlan_mask1,
                           uint64 dst_mac1, uint64 dst_mac_mask1, int priority2,
                           int vlan2, int vlan_mask2, uint64 dst_mac2,
                           uint64 dst_mac_mask2) {
    BcmL2Manager::MyStationEntry entry1(priority1, vlan1, vlan_mask1, dst_mac1,
                                        dst_mac_mask1);
    BcmL2Manager::MyStationEntry entry2(priority2, vlan2, vlan_mask2, dst_mac2,
                                        dst_mac_mask2);
    return entry1 == entry2;
  }

  bool MyStationEntryLess(int priority1, int vlan1, int vlan_mask1,
                          uint64 dst_mac1, uint64 dst_mac_mask1, int priority2,
                          int vlan2, int vlan_mask2, uint64 dst_mac2,
                          uint64 dst_mac_mask2) {
    BcmL2Manager::MyStationEntry entry1(priority1, vlan1, vlan_mask1, dst_mac1,
                                        dst_mac_mask1);
    BcmL2Manager::MyStationEntry entry2(priority2, vlan2, vlan_mask2, dst_mac2,
                                        dst_mac_mask2);
    return entry1 < entry2;
  }

  static constexpr uint64 kNodeId = 123123123;
  static constexpr int kUnit = 0;
  static constexpr int kStationId = 10;
  static constexpr uint64 kDstMac = 0x123456789012;
  static constexpr uint64 kDstMacMask = 0xfffffffffffa;
  static constexpr int kL2McastGroupId = 20;
  static constexpr int kRegularMyStationEntryPriority =
      BcmL2Manager::kRegularMyStationEntryPriority;
  static constexpr int kL3PromoteMyStationEntryPriority =
      BcmL2Manager::kL3PromoteMyStationEntryPriority;
  static constexpr int kSoftwareMulticastMyStationEntryPriority =
      BcmL2Manager::kSoftwareMulticastMyStationEntryPriority;

  std::unique_ptr<BcmChassisRoMock> bcm_chassis_ro_mock_;
  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmL2Manager> bcm_l2_manager_;
};

constexpr uint64 BcmL2ManagerTest::kNodeId;
constexpr int BcmL2ManagerTest::kUnit;
constexpr int BcmL2ManagerTest::kStationId;
constexpr uint64 BcmL2ManagerTest::kDstMac;
constexpr uint64 BcmL2ManagerTest::kDstMacMask;
constexpr int BcmL2ManagerTest::kRegularMyStationEntryPriority;
constexpr int BcmL2ManagerTest::kL3PromoteMyStationEntryPriority;

TEST_F(BcmL2ManagerTest, MyStationEntryEqualLess) {
  EXPECT_TRUE(MyStationEntryEqual(1, 0, 0, kDstMac, kDstMacMask, 1, 0, 0,
                                  kDstMac, kDstMacMask));
  EXPECT_TRUE(MyStationEntryLess(1, 0, 0, kDstMac, kDstMacMask, 10, 0, 0,
                                 kDstMac, kDstMacMask));
  EXPECT_FALSE(MyStationEntryEqual(1, 0, 0, kDstMac, kDstMacMask, 10, 0, 0,
                                   kDstMac, kDstMacMask));
  EXPECT_TRUE(MyStationEntryLess(1, 0, 0, kDstMac, kDstMacMask, 1, 0, 0,
                                 kDstMac, kDstMacMask + 1));
  EXPECT_FALSE(MyStationEntryEqual(1, 0, 0, kDstMac, kDstMacMask, 1, 0, 0,
                                   kDstMac, kDstMacMask + 1));
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForNoNodeConfigParams) {
  // Setup a test empty config with no node config param.
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForEmptyNodeConfigParams) {
  // Setup a test config with an empty node config param.
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  node->mutable_config_params();

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigSuccessForEmptyVlanConfig) {
  // Setup a test config with an empty/default VLAN config.
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  node->mutable_config_params()->add_vlan_configs();

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));

  // The first config push will add the station entry and the second one does
  // not.
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
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
  //    vlan config. This should not add my station entry.
  // 4- We push the config one more time and enable L2. This time we remove the
  //    my station entry as well.
  // 5- We push the same config one more time and this time make sure we do not
  //    try to remove the my station entry again.

  // 1st config push
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // 2nd config push
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kL3PromoteMyStationEntryPriority, kDefaultVlan,
                        0xfff, 0ULL, kNonMulticastDstMacMask))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_TRUE(l2_learning_disabled_for_default_vlan());

  // 3rd config push
  auto* l2_config = node->mutable_config_params()->mutable_l2_config();
  l2_config->set_l2_age_duration_sec(300);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, SetL2AgeTimer(kUnit, 300))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_TRUE(l2_learning_disabled_for_default_vlan());

  // 4th config push
  node->mutable_config_params()->clear_vlan_configs();
  node->mutable_config_params()->add_vlan_configs();
  node->mutable_config_params()->clear_l2_config();

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // 5th config push
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());
}

TEST_F(BcmL2ManagerTest, PushChassisConfigFailure) {
  ChassisConfig config;
  Node* node = config.add_nodes();
  node->set_id(kNodeId);
  auto vlan_config = node->mutable_config_params()->add_vlan_configs();

  // Failue when L2 is enabled -- scenario 1
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 2
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 3
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is enabled -- scenario 4
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 1
  vlan_config->set_block_broadcast(false);
  vlan_config->set_block_known_multicast(false);
  vlan_config->set_block_unknown_multicast(true);
  vlan_config->set_block_unknown_unicast(true);
  vlan_config->set_disable_l2_learning(true);
  auto* l2_config = node->mutable_config_params()->mutable_l2_config();
  l2_config->set_l2_age_duration_sec(300);

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 2
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 3
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 4
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
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
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 5
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kL3PromoteMyStationEntryPriority, kDefaultVlan,
                        0xfff, 0ULL, kNonMulticastDstMacMask))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(-1, false);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 6
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kL3PromoteMyStationEntryPriority, kDefaultVlan,
                        0xfff, 0ULL, kNonMulticastDstMacMask))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 7
  // AddMyStationEntry will not be called from now on
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 8
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->PushChassisConfig(config, kNodeId),
              DerivedFromStatus(DefaultError()));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_FALSE(l2_learning_disabled_for_default_vlan());

  // Failue when L2 is disabled -- scenario 9
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
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
  VerifyL3PromoteMyStationEntry(kStationId, true);
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

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kL3PromoteMyStationEntryPriority, kDefaultVlan,
                        0xfff, 0ULL, kNonMulticastDstMacMask))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
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

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureVlanBlock(kUnit, kDefaultVlan, false,
                                                 false, false, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, false))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteVlanIfFound(kUnit, kArpVlan))
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
  VerifyL3PromoteMyStationEntry(kStationId, false);

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

  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kDefaultVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kDefaultVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2EntriesByVlan(kUnit, kDefaultVlan))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kL3PromoteMyStationEntryPriority, kDefaultVlan,
                        0xfff, 0ULL, kNonMulticastDstMacMask))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, AddVlanIfNotFound(kUnit, kArpVlan, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_,
              ConfigureVlanBlock(kUnit, kArpVlan, false, false, true, true))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, ConfigureL2Learning(kUnit, kArpVlan, false))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->PushChassisConfig(config, kNodeId));
  VerifyL3PromoteMyStationEntry(kStationId, true);
  EXPECT_OK(bcm_l2_manager_->Shutdown());
  VerifyL3PromoteMyStationEntry(-1, false);
}

TEST_F(BcmL2ManagerTest, InsertAndDeleteMyStationEntrySuccess) {
  BcmFlowEntry bcm_flow_entry;

  // An entry which only has dst_mac (no dst_mac_mask, no vlan, no vlan_mask).
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
  }

  // First add the entry.
  EXPECT_CALL(*bcm_sdk_mock_,
              AddMyStationEntry(kUnit, kRegularMyStationEntryPriority, 0, 0,
                                kDstMac, 0))
      .WillOnce(Return(kStationId));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, 0, kStationId, true);

  // Then remove the same entry.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, 0, -1, false);

  // Now add a new entry for a specific dst_mac and a specific VLAN.
  bcm_flow_entry.clear_fields();
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field->mutable_mask()->set_u64(0xffffffffffff);
  }
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::VLAN_VID);
    field->mutable_value()->set_u32(kDefaultVlan + 1);  // != kDefaultVlan
    field->mutable_mask()->set_u32(0xffa);
  }

  EXPECT_CALL(
      *bcm_sdk_mock_,
      AddMyStationEntry(kUnit, kRegularMyStationEntryPriority, kDefaultVlan + 1,
                        0xffa, kDstMac, 0xffffffffffff))
      .WillOnce(Return(kStationId + 1));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(kDefaultVlan + 1, 0xffa, kDstMac, 0xffffffffffff,
                              kStationId + 1, true);

  // Program the entry again. This should not fail not it should try to add the
  // entry to hadrware.
  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyRegularMyStationEntry(kDefaultVlan + 1, 0xffa, kDstMac, 0xffffffffffff,
                              kStationId + 1, true);

  // Now add another entry for a specific dst_mac and dst_mac_mask (no vlan,
  // no vlan_mask).
  bcm_flow_entry.clear_fields();
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field->mutable_mask()->set_u64(kDstMacMask);
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              AddMyStationEntry(kUnit, kRegularMyStationEntryPriority, 0, 0,
                                kDstMac, kDstMacMask))
      .WillOnce(Return(kStationId + 2));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, kDstMacMask, kStationId + 2, true);

  // Then remove the same entry.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId + 2))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, kDstMacMask, -1, false);

  // Trying to remove the entry again. This should be OK and should not try
  // to remove the entry again.
  EXPECT_OK(bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry));
  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, kDstMacMask, -1, false);
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureWhenAddFailsOnHw) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field->mutable_mask()->set_u64(0xffffffffffff);
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              AddMyStationEntry(kUnit, kRegularMyStationEntryPriority, 0, 0,
                                kDstMac, 0xffffffffffff))
      .WillOnce(Return(DefaultError()));

  EXPECT_THAT(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry),
              DerivedFromStatus(DefaultError()));

  VerifyL3PromoteMyStationEntry(-1, false);
  VerifyRegularMyStationEntry(0, 0, kDstMac, 0xffffffffffff, -1, false);
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForInvalidTableId) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV4_LPM);
  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid table_id"));
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForInvalidUnit) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit + 1);
  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("BcmFlowEntry for wrong unit"));
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForInvalidAction) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  bcm_flow_entry.add_actions();
  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("entry with action"));
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForInvalidDstMac) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kBroadcastMac);
  }

  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("with ETH_DST set to broadcast MAC"));
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForUnknownField) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_SRC);
    field->mutable_value()->set_u64(kDstMac);
  }

  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("fields othere than ETH_DST and VLAN_VID"));
}

TEST_F(BcmL2ManagerTest, InsertMyStationEntryFailureForInvalidVlanMask) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
  }
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::VLAN_VID);
    field->mutable_value()->set_u32(kDefaultVlan);
  }

  ::util::Status status = bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("vlan > 0 while vlan_mask is either not given or is 0"));
}

TEST_F(BcmL2ManagerTest, DeleteMyStationEntryFailureWhenDeleteFailsOnHw) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field->mutable_mask()->set_u64(0xffffffffffff);
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              AddMyStationEntry(kUnit, kRegularMyStationEntryPriority, 0, 0,
                                kDstMac, 0xffffffffffff))
      .WillOnce(Return(kStationId));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteMyStationEntry(kUnit, kStationId))
      .WillOnce(Return(DefaultError()));

  EXPECT_OK(bcm_l2_manager_->InsertMyStationEntry(bcm_flow_entry));
  EXPECT_THAT(bcm_l2_manager_->DeleteMyStationEntry(bcm_flow_entry),
              DerivedFromStatus(DefaultError()));

  VerifyRegularMyStationEntry(0, 0, kDstMac, 0xffffffffffff, kStationId, true);
}

TEST_F(BcmL2ManagerTest, InsertAndDeleteL2EntrySuccess) {
  BcmFlowEntry bcm_flow_entry;

  // Simple entry without actions
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_L2_UNICAST);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::VLAN_VID);
    field->mutable_value()->set_u32(kDefaultVlan);
  }

  EXPECT_CALL(*bcm_sdk_mock_, AddL2Entry(kUnit, kDefaultVlan, kDstMac, 0, 0, 0,
                                         0, false, false))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->InsertL2Entry(bcm_flow_entry));

  // Delete entry
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2Entry(kUnit, kDefaultVlan, kDstMac))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(bcm_l2_manager_->DeleteL2Entry(bcm_flow_entry));
}

TEST_F(BcmL2ManagerTest, InsertThenDeleteMulticastGroupSuccess) {
  BcmFlowEntry bcm_flow_entry;
  bcm_flow_entry.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_L2_MULTICAST);
  bcm_flow_entry.set_unit(kUnit);
  {
    auto* field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::ETH_DST);
    field->mutable_value()->set_u64(kDstMac);
    field->mutable_mask()->set_u64(kBroadcastMac);
    field = bcm_flow_entry.add_fields();
    field->set_type(BcmField::VLAN_VID);
    field->mutable_value()->set_u32(kDefaultVlan);
    auto* action = bcm_flow_entry.add_actions();
    action->set_type(BcmAction::SET_L2_MCAST_GROUP);
    auto* param = action->add_params();
    param->set_type(BcmAction::Param::L2_MCAST_GROUP_ID);
    param->mutable_value()->set_u32(kL2McastGroupId);
  }

  EXPECT_CALL(*bcm_sdk_mock_,
              AddL2MulticastEntry(
                  kUnit, kSoftwareMulticastMyStationEntryPriority, kDefaultVlan,
                  0, kDstMac, kBroadcastMac, false, false, kL2McastGroupId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(bcm_l2_manager_->InsertMulticastGroup(bcm_flow_entry));

  EXPECT_CALL(*bcm_sdk_mock_, DeleteL2MulticastEntry(kUnit, kDefaultVlan, 0,
                                                     kDstMac, kBroadcastMac))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(bcm_l2_manager_->DeleteMulticastGroup(bcm_flow_entry));
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
