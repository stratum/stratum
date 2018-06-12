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


#include "stratum/hal/lib/bcm/bcm_l3_manager.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

using ::testing::HasSubstr;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace bcm {

class BcmL3ManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_l3_manager_ = BcmL3Manager::CreateInstance(bcm_sdk_mock_.get(), kUnit);

    // Setup test non-multipath nexthops
    cpu_l2_copy_nexthop_.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
    cpu_l2_copy_nexthop_.set_unit(kUnit);
    cpu_l2_copy_nexthop_.set_logical_port(kCpuPort);

    cpu_normal_l3_nexthop_.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
    cpu_normal_l3_nexthop_.set_unit(kUnit);
    cpu_normal_l3_nexthop_.set_logical_port(kCpuPort);
    cpu_normal_l3_nexthop_.set_vlan(kVlan);
    cpu_normal_l3_nexthop_.set_src_mac(kSrcMac);
    cpu_normal_l3_nexthop_.set_dst_mac(kDstMac);

    port_nexthop_.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
    port_nexthop_.set_unit(kUnit);
    port_nexthop_.set_logical_port(kLogicalPort);
    port_nexthop_.set_vlan(kVlan);
    port_nexthop_.set_src_mac(kSrcMac);
    port_nexthop_.set_dst_mac(kDstMac);

    trunk_nexthop_.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK);
    trunk_nexthop_.set_unit(kUnit);
    trunk_nexthop_.set_trunk_port(kTrunkPort);
    trunk_nexthop_.set_vlan(kVlan);
    trunk_nexthop_.set_src_mac(kSrcMac);
    trunk_nexthop_.set_dst_mac(kDstMac);

    drop_nexthop_.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP);
    drop_nexthop_.set_unit(kUnit);

    // Setup test multipath nexthops
    wcmp_nexthop_.set_unit(kUnit);
    {
      auto* member = wcmp_nexthop_.add_members();
      member->set_egress_intf_id(kMemberEgressIntfId1);
      member->set_weight(kMemberWeight1);
      for (int i = 0; i < kMemberWeight1; ++i) {
        wcmp_group_member_ids_.push_back(kMemberEgressIntfId1);
      }
    }
    {
      auto* member = wcmp_nexthop_.add_members();
      member->set_egress_intf_id(kMemberEgressIntfId2);
      member->set_weight(kMemberWeight2);
      for (int i = 0; i < kMemberWeight2; ++i) {
        wcmp_group_member_ids_.push_back(kMemberEgressIntfId2);
      }
    }
  }

  void IncrementRefCount(int router_intf_id) {
    ASSERT_OK(bcm_l3_manager_->IncrementRefCount(router_intf_id));
  }

  static constexpr int kUnit = 3;
  static constexpr uint64 kNodeId = 12345678;
  static constexpr int kEgressIntfId = 100002;
  static constexpr int kVlan = 1;
  static constexpr int kCpuPort = 0;
  static constexpr int kLogicalPort = 33;
  static constexpr int kTrunkPort = 22;
  static constexpr int kOldRouterIntfId = 2;
  static constexpr int kNewRouterIntfId = 3;
  static constexpr uint64 kSrcMac = 0x112233445566;
  static constexpr uint64 kDstMac = 0x223344556677;
  static constexpr int kMemberEgressIntfId1 = 100004;
  static constexpr int kMemberEgressIntfId2 = 100005;
  static constexpr uint32 kMemberWeight1 = 2;
  static constexpr uint32 kMemberWeight2 = 3;

  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmL3Manager> bcm_l3_manager_;
  BcmNonMultipathNexthop cpu_l2_copy_nexthop_;
  BcmNonMultipathNexthop cpu_normal_l3_nexthop_;
  BcmNonMultipathNexthop port_nexthop_;
  BcmNonMultipathNexthop trunk_nexthop_;
  BcmNonMultipathNexthop drop_nexthop_;
  BcmMultipathNexthop wcmp_nexthop_;
  std::vector<int> wcmp_group_member_ids_;
};

constexpr int BcmL3ManagerTest::kUnit;
constexpr uint64 BcmL3ManagerTest::kNodeId;
constexpr int BcmL3ManagerTest::kEgressIntfId;
constexpr int BcmL3ManagerTest::kVlan;
constexpr int BcmL3ManagerTest::kCpuPort;
constexpr int BcmL3ManagerTest::kLogicalPort;
constexpr int BcmL3ManagerTest::kTrunkPort;
constexpr int BcmL3ManagerTest::kOldRouterIntfId;
constexpr int BcmL3ManagerTest::kNewRouterIntfId;
constexpr uint64 BcmL3ManagerTest::kSrcMac;
constexpr uint64 BcmL3ManagerTest::kDstMac;
constexpr int BcmL3ManagerTest::kMemberEgressIntfId1;
constexpr int BcmL3ManagerTest::kMemberEgressIntfId2;
constexpr uint32 BcmL3ManagerTest::kMemberWeight1;
constexpr uint32 BcmL3ManagerTest::kMemberWeight2;

TEST_F(BcmL3ManagerTest, PushChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  ASSERT_OK(bcm_l3_manager_->PushChassisConfig(config, kNodeId));  // NOOP atm
}

TEST_F(BcmL3ManagerTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  // Verify before and after config push
  EXPECT_OK(bcm_l3_manager_->VerifyChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l3_manager_->PushChassisConfig(config, kNodeId));
  EXPECT_OK(bcm_l3_manager_->VerifyChassisConfig(config, kNodeId));
}

TEST_F(BcmL3ManagerTest, VerifyChassisConfigFailure) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);

  // Verify failure for invalid node
  ::util::Status status = bcm_l3_manager_->VerifyChassisConfig(config, 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid node ID"));

  // Change in the node_id after config push is reboot required.
  EXPECT_OK(bcm_l3_manager_->PushChassisConfig(config, kNodeId));
  status = bcm_l3_manager_->VerifyChassisConfig(config, kNodeId + 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected a change in the node_id"));
}

TEST_F(BcmL3ManagerTest, Shutdown) {
  ASSERT_OK(bcm_l3_manager_->Shutdown());  // NOOP at this point
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopSuccessForCpuPortL2Copy) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3CpuEgressIntf(kUnit))
      .WillOnce(Return(kEgressIntfId));

  auto ret =
      bcm_l3_manager_->FindOrCreateNonMultipathNexthop(cpu_l2_copy_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopSuccessForCpuPortNormalL3) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateL3PortEgressIntf(kUnit, kDstMac, kCpuPort, kVlan,
                                           kNewRouterIntfId))
      .WillOnce(Return(kEgressIntfId));

  auto ret =
      bcm_l3_manager_->FindOrCreateNonMultipathNexthop(cpu_normal_l3_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest, FindOrCreateNonMultipathNexthopSuccessForRegularPort) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateL3PortEgressIntf(kUnit, kDstMac, kLogicalPort, kVlan,
                                           kNewRouterIntfId))
      .WillOnce(Return(kEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(port_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest, FindOrCreateNonMultipathNexthopSuccessForTrunk) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateL3TrunkEgressIntf(kUnit, kDstMac, kTrunkPort, kVlan,
                                            kNewRouterIntfId))
      .WillOnce(Return(kEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(trunk_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest, FindOrCreateNonMultipathNexthopSuccessForDrop) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3DropIntf(kUnit))
      .WillOnce(Return(kEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(drop_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForRegularPortNoSrcMac) {
  port_nexthop_.clear_src_mac();
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(port_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_PORT"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForRegularPortNoDstMac) {
  port_nexthop_.clear_dst_mac();
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(port_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_PORT"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForTrunkNoSrcMac) {
  trunk_nexthop_.clear_src_mac();
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(trunk_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_TRUNK"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForTrunkNoDstMac) {
  trunk_nexthop_.clear_dst_mac();
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(trunk_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_TRUNK"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenCreateRouterIntfFailsForPort) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(port_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(
    BcmL3ManagerTest,
    FindOrCreateNonMultipathNexthopFailureWhenCreateRouterIntfFailsForTrunk) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(trunk_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenCreateEgressIntfFailsForCpu) {
  // Expectations for the mock objects when FindOrCreateL3PortEgressIntf errors
  // out on HW.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3CpuEgressIntf(kUnit))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret =
      bcm_l3_manager_->FindOrCreateNonMultipathNexthop(cpu_l2_copy_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenCreateEgressIntfFailsForPort) {
  // Expectations for the mock objects when FindOrCreateL3PortEgressIntf errors
  // out on HW.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateL3PortEgressIntf(kUnit, kDstMac, kLogicalPort, kVlan,
                                           kNewRouterIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(port_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(
    BcmL3ManagerTest,
    FindOrCreateNonMultipathNexthopFailureWhenCreateEgressIntfFailsForTrunk) {
  // Expectations for the mock objects when FindOrCreateL3PortEgressIntf errors
  // out on HW.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateL3TrunkEgressIntf(kUnit, kDstMac, kTrunkPort, kVlan,
                                            kNewRouterIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(trunk_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenCreateEgressIntfFailsForDrop) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3DropIntf(kUnit))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(drop_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenSrcMacIsGivenForDrop) {
  // Expectations for the mock objects.
  drop_nexthop_.set_src_mac(kSrcMac);
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(drop_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_DROP"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureWhenDstMacIsGivenForDrop) {
  // Expectations for the mock objects.
  drop_nexthop_.set_dst_mac(kDstMac);
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(drop_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_DROP"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForInvalidNexthopType) {
  // Expectations for the mock objects.
  BcmNonMultipathNexthop invalid_nexthop;
  invalid_nexthop.set_unit(kUnit);
  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(invalid_nexthop);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Invalid nexthop type"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateNonMultipathNexthopFailureForZeroEgressIntfId) {
  const int kInvalidEgressIntfId = 0;
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3DropIntf(kUnit))
      .WillOnce(Return(kInvalidEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateNonMultipathNexthop(drop_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid egress_intf_id"));
}

TEST_F(BcmL3ManagerTest, FindOrCreateMultipathNexthopSuccessForRegularGroups) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateEcmpEgressIntf(kUnit, wcmp_group_member_ids_))
      .WillOnce(Return(kEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(wcmp_nexthop_);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateMultipathNexthopSuccessForGroupsWithOneMember) {
  BcmMultipathNexthop nexthop;
  nexthop.set_unit(kUnit);
  auto* member = nexthop.add_members();
  member->set_egress_intf_id(kMemberEgressIntfId1);
  member->set_weight(1);

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateEcmpEgressIntf(
                  kUnit, std::vector<int>(
                             {kMemberEgressIntfId1, kMemberEgressIntfId1})))
      .WillOnce(Return(kEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(nexthop);
  ASSERT_TRUE(ret.ok());
  EXPECT_EQ(kEgressIntfId, ret.ValueOrDie());
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateMultipathNexthopFailureForZeroMemberWeight) {
  wcmp_nexthop_.mutable_members(0)->clear_weight();

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(wcmp_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Zero weight"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateMultipathNexthopFailureForInvalidMemberEgressIntf) {
  wcmp_nexthop_.mutable_members(0)->set_egress_intf_id(0);

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(wcmp_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("Invalid member egress_intf_id"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateMultipathNexthopFailureWhenEcmpGroupCreationFails) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateEcmpEgressIntf(kUnit, wcmp_group_member_ids_))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(wcmp_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       FindOrCreateMultipathNexthopFailureWhenInvalidEcmpGroupCreated) {
  // Expectations for the mock objects.
  const int kInvalidEgressIntfId = 0;
  EXPECT_CALL(*bcm_sdk_mock_,
              FindOrCreateEcmpEgressIntf(kUnit, wcmp_group_member_ids_))
      .WillOnce(Return(kInvalidEgressIntfId));

  auto ret = bcm_l3_manager_->FindOrCreateMultipathNexthop(wcmp_nexthop_);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.status().error_code());
  EXPECT_THAT(ret.status().error_message(),
              HasSubstr("No egress_intf_id found for"));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopSuccessForCpuPortL2Copy) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3CpuEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId,
                                                       cpu_l2_copy_nexthop_));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopSuccessForCpuPortNormalL3) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3PortEgressIntf(kUnit, kEgressIntfId, kDstMac, kCpuPort,
                                     kVlan, kNewRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId,
                                                       cpu_normal_l3_nexthop_));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopSuccessForRegularPort) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3PortEgressIntf(kUnit, kEgressIntfId, kDstMac,
                                     kLogicalPort, kVlan, kNewRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopSuccessForTrunk) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3TrunkEgressIntf(kUnit, kEgressIntfId, kDstMac, kTrunkPort,
                                      kVlan, kNewRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId,
                                                       trunk_nexthop_));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopSuccessForDrop) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3DropIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, drop_nexthop_));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureForInvalidEgressIntfId) {
  const int kInvalidEgressIntfId = 0;

  auto status = bcm_l3_manager_->ModifyNonMultipathNexthop(kInvalidEgressIntfId,
                                                           port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid egress_intf_id"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenFindRouterIntfFails) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureForRegularPortNoSrcMac) {
  // Expectations for the mock objects.
  port_nexthop_.clear_src_mac();
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_PORT"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureForRegularPortNoDstMac) {
  // Expectations for the mock objects.
  port_nexthop_.clear_dst_mac();
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_PORT"));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopFailureForTrunkNoSrcMac) {
  // Expectations for the mock objects.
  trunk_nexthop_.clear_src_mac();
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, trunk_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_TRUNK"));
}

TEST_F(BcmL3ManagerTest, ModifyNonMultipathNexthopFailureForTrunkNoDstMac) {
  // Expectations for the mock objects.
  trunk_nexthop_.clear_dst_mac();
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, trunk_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_TRUNK"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenCreateRouterIntfFailsForCpu) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3CpuEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status = bcm_l3_manager_->ModifyNonMultipathNexthop(
      kEgressIntfId, cpu_l2_copy_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenCreateRouterIntfFailsForPort) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenCreateRouterIntfFailsForTrunk) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, trunk_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenModifyEgressIntfFailsForPort) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3PortEgressIntf(kUnit, kEgressIntfId, kDstMac,
                                     kLogicalPort, kVlan, kNewRouterIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, port_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenModifyEgressIntfFailsForTrunk) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, FindOrCreateL3RouterIntf(kUnit, kSrcMac, kVlan))
      .WillOnce(Return(kNewRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3TrunkEgressIntf(kUnit, kEgressIntfId, kDstMac, kTrunkPort,
                                      kVlan, kNewRouterIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, trunk_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenModifyEgressIntfFailsForDrop) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3DropIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, drop_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenSrcMacIsGivenForDrop) {
  // Expectations for the mock objects.
  drop_nexthop_.set_src_mac(kSrcMac);
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, drop_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_DROP"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureWhenDstMacIsGivenForDrop) {
  // Expectations for the mock objects.
  drop_nexthop_.set_dst_mac(kDstMac);
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status =
      bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId, drop_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid nexthop of type NEXTHOP_TYPE_DROP"));
}

TEST_F(BcmL3ManagerTest,
       ModifyNonMultipathNexthopFailureForInvalidNexthopType) {
  // Expectations for the mock objects.
  BcmNonMultipathNexthop invalid_nexthop;
  invalid_nexthop.set_unit(kUnit);
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));

  auto status = bcm_l3_manager_->ModifyNonMultipathNexthop(kEgressIntfId,
                                                           invalid_nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid nexthop type"));
}

TEST_F(BcmL3ManagerTest, ModifyMultipathNexthopSuccess) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyEcmpEgressIntf(kUnit, kEgressIntfId,
                                                   wcmp_group_member_ids_))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(
      bcm_l3_manager_->ModifyMultipathNexthop(kEgressIntfId, wcmp_nexthop_));
}

TEST_F(BcmL3ManagerTest, ModifyMultipathNexthopFailureForInvalidEgressIntf) {
  const int kInvalidEgressIntfId = 0;
  auto status = bcm_l3_manager_->ModifyMultipathNexthop(kInvalidEgressIntfId,
                                                        wcmp_nexthop_);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid egress_intf_id"));
}

TEST_F(BcmL3ManagerTest, DeleteNonMultipathNexthopSuccess) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3EgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3RouterIntf(kUnit, kOldRouterIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteNonMultipathNexthop(kEgressIntfId));
}

TEST_F(BcmL3ManagerTest, DeleteNonMultipathNexthopFailure) {
  // Expectations for the mock objects.
  IncrementRefCount(kOldRouterIntfId);
  EXPECT_CALL(*bcm_sdk_mock_,
              FindRouterIntfFromEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(kOldRouterIntfId));
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3EgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status = bcm_l3_manager_->DeleteNonMultipathNexthop(kEgressIntfId);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

TEST_F(BcmL3ManagerTest, DeleteMultipathNexthopSuccess) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteEcmpEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteMultipathNexthop(kEgressIntfId));
}

TEST_F(BcmL3ManagerTest, DeleteMultipathNexthopFailure) {
  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteEcmpEgressIntf(kUnit, kEgressIntfId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));

  auto status = bcm_l3_manager_->DeleteMultipathNexthop(kEgressIntfId);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));
}

// TODO: Define static proto text and others constants in the test
// class, similar to nexthops.
TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowSuccessForIpv4LpmFlowAndMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      fields: {
        type: VRF
        value {
          u32: 80
        }
      }
      actions: {
        type: OUTPUT_L3
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 200256
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, AddL3RouteIpv4(kUnit, 80, 0xc0a00100, 0xffffff00,
                                             -1, 200256, true))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowSuccessForIpv4LpmFlowAndPortNonMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      fields: {
        type: VRF
        value {
          u32: 80
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, AddL3RouteIpv4(kUnit, 80, 0xc0a00100, 0xffffff00,
                                             -1, 100003, false))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowSuccessForIpv4LpmFlowAndTrunkNonMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      fields: {
        type: VRF
        value {
          u32: 80
        }
      }
      actions: {
        type: OUTPUT_TRUNK
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, AddL3RouteIpv4(kUnit, 80, 0xc0a00100, 0xffffff00,
                                             -1, 100003, false))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowSuccessForIpv4LpmFlowAndDropNonMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      fields: {
        type: VRF
        value {
          u32: 80
        }
      }
      actions: {
        type: DROP
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, AddL3RouteIpv4(kUnit, 80, 0xc0a00100, 0xffffff00,
                                             -1, 100003, false))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, InsertLpmOrHostFlowSuccessForIpv4HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_HOST
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, AddL3HostIpv4(kUnit, 0, 0xc0a00100, -1, 100003))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowSuccessForIpv6LpmFlowAndMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_LPM
      fields: {
        type: IPV6_DST
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
        mask {
          b: "\xff\xff\xff\xff\xff\xff\xff\x00"
        }
      }
      actions: {
        type: OUTPUT_L3
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 200256
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              AddL3RouteIpv6(kUnit, 0,
                             std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8),
                             std::string("\xff\xff\xff\xff\xff\xff\xff\x00", 8),
                             -1, 200256, true))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, InsertLpmOrHostFlowSuccessForIpv6HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_HOST
      fields: {
        type: IPV6_DST
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              AddL3HostIpv6(kUnit, 0,
                            std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8),
                            -1, 100003))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowFailureWhenIpv4LpmFlowDefinesPortNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: ETH_SRC
          value {
            u64: 0x112233445566
          }
        }
        params {
          type: ETH_DST
          value {
            u64: 0x223344556677
          }
        }
        params {
          type: LOGICAL_PORT
          value {
            u32: 33
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  auto status = bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("required defining a new port egress intf"));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowFailureWhenIpv4LpmFlowDefinesTrunkNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: OUTPUT_TRUNK
        params {
          type: ETH_SRC
          value {
            u64: 0x112233445566
          }
        }
        params {
          type: ETH_DST
          value {
            u64: 0x223344556677
          }
        }
        params {
          type: TRUNK_PORT
          value {
            u32: 2
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  auto status = bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("required defining a new trunk egress intf"));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowFailureWhenIpv4LpmFlowDefinesDropNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: DROP
        params {
          type: ETH_SRC
          value {
            u64: 0x112233445566
          }
        }
        params {
          type: ETH_DST
          value {
            u64: 0x223344556677
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  auto status = bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_OPER_NOT_SUPPORTED, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("required defining a new drop egress intf"));
}

TEST_F(BcmL3ManagerTest,
       InsertLpmOrHostFlowFailureWhenIpv4LpmFlowDefinesInvalidNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: DROP
        params {
          type: ETH_SRC
          value {
            u64: 0x112233445566
          }
        }
        params {
          type: ETH_DST
          value {
            u64: 0x223344556677
          }
        }
        params {
          type: TRUNK_PORT
          value {
            u32: 2
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  auto status = bcm_l3_manager_->InsertLpmOrHostFlow(bcm_flow_entry);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Invalid action parameters"));
}

TEST_F(BcmL3ManagerTest,
       ModifyLpmOrHostFlowSuccessForIpv4LpmFlowAndMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: OUTPUT_L3
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 200256
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3RouteIpv4(kUnit, 0, 0xc0a00100,
                                                0xffffff00, -1, 200256, true))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       ModifyLpmOrHostFlowSuccessForIpv4LpmFlowAndNonMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      actions: {
        type: OUTPUT_TRUNK
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, ModifyL3RouteIpv4(kUnit, 0, 0xc0a00100,
                                                0xffffff00, -1, 100003, false))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, ModifyLpmOrHostFlowSuccessForIpv4HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_HOST
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3HostIpv4(kUnit, 0, 0xc0a00100, -1, 100003))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest,
       ModifyLpmOrHostFlowSuccessForIpv6LpmFlowAndMultipathNexthop) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_LPM
      fields: {
        type: IPV6_DST
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
        mask {
          b: "\xff\xff\xff\xff\xff\xff\xff\x00"
        }
      }
      actions: {
        type: OUTPUT_L3
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 200256
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(
      *bcm_sdk_mock_,
      ModifyL3RouteIpv6(
          kUnit, 0, std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8),
          std::string("\xff\xff\xff\xff\xff\xff\xff\x00", 8), -1, 200256, true))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, ModifyLpmOrHostFlowSuccessForIpv6HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_HOST
      fields: {
        type: IPV6_DST
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
      }
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              ModifyL3HostIpv6(
                  kUnit, 0, std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8),
                  -1, 100003))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->ModifyLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, DeleteLpmOrHostFlowSuccessForIpv4LpmFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_LPM
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
        mask {
          u32: 0xffffff00
        }
      }
      fields: {
        type: VRF
        value {
          u32: 80
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              DeleteL3RouteIpv4(kUnit, 80, 0xc0a00100, 0xffffff00))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, DeleteLpmOrHostFlowSuccessForIpv4HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV4_HOST
      fields: {
        type: IPV4_DST
        value {
          u32: 0xc0a00100
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_, DeleteL3HostIpv4(kUnit, 0, 0xc0a00100))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, DeleteLpmOrHostFlowSuccessForIpv6LpmFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_LPM
      fields: {
        type: IPV6_DST
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
        mask {
          b: "\xff\xff\xff\xff\xff\xff\xff\x00"
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              DeleteL3RouteIpv6(
                  kUnit, 0, std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8),
                  std::string("\xff\xff\xff\xff\xff\xff\xff\x00", 8)))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteLpmOrHostFlow(bcm_flow_entry));
}

TEST_F(BcmL3ManagerTest, DeleteLpmOrHostFlowSuccessForIpv6HostFlow) {
  const std::string kBcmFlowEntryText = R"(
      unit: 3
      bcm_table_type: BCM_TABLE_IPV6_HOST
      fields: {
        type: IPV6_DST_UPPER_64
        value {
          b: "\x01\x02\x03\x04\x05\x06\x07\x08"
        }
      }
      # Will be ignored
      actions: {
        type: OUTPUT_PORT
        params {
          type: EGRESS_INTF_ID
          value {
            u32: 100003
          }
        }
      }
  )";

  // Test BcmFlowEntry.
  BcmFlowEntry bcm_flow_entry;
  ASSERT_OK(ParseProtoFromString(kBcmFlowEntryText, &bcm_flow_entry));

  // Expectations for the mock objects.
  EXPECT_CALL(*bcm_sdk_mock_,
              DeleteL3HostIpv6(
                  kUnit, 0, std::string("\x01\x02\x03\x04\x05\x06\x07\x08", 8)))
      .WillOnce(Return(::util::OkStatus()));

  ASSERT_OK(bcm_l3_manager_->DeleteLpmOrHostFlow(bcm_flow_entry));
}

// TODO: Add more coverage for the failure case.

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
