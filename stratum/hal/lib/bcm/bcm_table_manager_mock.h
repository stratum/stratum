/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_MOCK_H_

#include "stratum/hal/lib/bcm/bcm_table_manager.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmTableManagerMock : public BcmTableManager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(PushForwardingPipelineConfig,
               ::util::Status(const ::p4::ForwardingPipelineConfig& config));
  MOCK_METHOD1(VerifyForwardingPipelineConfig,
               ::util::Status(const ::p4::ForwardingPipelineConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_CONST_METHOD1(IsLpmFlowWithOverrideVrf,
                     bool(const CommonFlowEntry& common_flow_entry));
  MOCK_CONST_METHOD1(IsLpmFlowWithFallbackVrf,
                     bool(const CommonFlowEntry& common_flow_entry));
  MOCK_CONST_METHOD3(FillBcmFlowEntry,
                     ::util::Status(const ::p4::TableEntry& table_entry,
                                    ::p4::Update::Type type,
                                    BcmFlowEntry* bcm_flow_entry));
  MOCK_CONST_METHOD2(FillBcmMeterConfig,
                     ::util::Status(const ::p4::MeterConfig& p4_meter,
                                    BcmMeterConfig* bcm_meter));
  MOCK_CONST_METHOD2(CommonFlowEntryToBcmFlowEntry,
                     ::util::Status(const CommonFlowEntry& common_flow_entry,
                                    BcmFlowEntry* bcm_flow_entry));
  MOCK_CONST_METHOD2(
      FillBcmNonMultipathNexthop,
      ::util::Status(const ::p4::ActionProfileMember& action_profile_member,
                     BcmNonMultipathNexthop* bcm_non_multipath_nexthop));
  MOCK_CONST_METHOD2(
      FillBcmMultipathNexthop,
      ::util::Status(const ::p4::ActionProfileGroup& action_profile_group,
                     BcmMultipathNexthop* bcm_multipath_nexthop));
  MOCK_METHOD1(AddTableEntry,
               ::util::Status(const ::p4::TableEntry& table_entry));
  MOCK_METHOD1(UpdateTableEntry,
               ::util::Status(const ::p4::TableEntry& table_entry));
  MOCK_METHOD1(DeleteTableEntry,
               ::util::Status(const ::p4::TableEntry& table_entry));
  MOCK_METHOD1(UpdateTableEntryMeter,
               ::util::Status(const ::p4::DirectMeterEntry& meter));
  MOCK_METHOD3(
      AddActionProfileMember,
      ::util::Status(const ::p4::ActionProfileMember& action_profile_member,
                     BcmNonMultipathNexthop::Type type, int egress_intf_id));
  MOCK_METHOD2(
      AddActionProfileGroup,
      ::util::Status(const ::p4::ActionProfileGroup& action_profile_group,
                     int egress_intf_id));
  MOCK_METHOD2(
      UpdateActionProfileMember,
      ::util::Status(const ::p4::ActionProfileMember& action_profile_member,
                     BcmNonMultipathNexthop::Type type));
  MOCK_METHOD1(
      UpdateActionProfileGroup,
      ::util::Status(const ::p4::ActionProfileGroup& action_profile_group));
  MOCK_METHOD1(
      DeleteActionProfileMember,
      ::util::Status(const ::p4::ActionProfileMember& action_profile_member));
  MOCK_METHOD1(
      DeleteActionProfileGroup,
      ::util::Status(const ::p4::ActionProfileGroup& action_profile_group));
  MOCK_CONST_METHOD1(GetGroupsForMember,
                     ::util::StatusOr<std::set<uint32>>(uint32 member_id));
  MOCK_CONST_METHOD1(ActionProfileMemberExists, bool(uint32 member_id));
  MOCK_CONST_METHOD1(ActionProfileGroupExists, bool(uint32 group_id));
  MOCK_CONST_METHOD2(GetBcmNonMultipathNexthopInfo,
                     ::util::Status(uint32 member_id,
                                    BcmNonMultipathNexthopInfo* info));
  MOCK_CONST_METHOD2(GetBcmMultipathNexthopInfo,
                     ::util::Status(uint32 group_id,
                                    BcmMultipathNexthopInfo* info));
  MOCK_METHOD1(AddAclTable, ::util::Status(AclTable table));
  MOCK_CONST_METHOD1(GetReadOnlyAclTable,
                     ::util::StatusOr<const AclTable*>(uint32 table_id));
  MOCK_METHOD2(AddAclTableEntry,
               ::util::Status(const ::p4::TableEntry& table_entry,
                              int bcm_flow_id));
  MOCK_CONST_METHOD0(GetAllAclTableIDs, std::set<uint32>());
  MOCK_METHOD1(DeleteTable, ::util::Status(uint32 table_id));
  MOCK_CONST_METHOD1(P4FieldTypeToBcmFieldType,
                     BcmField::Type(P4FieldType p4_field_type));
  MOCK_CONST_METHOD3(ReadTableEntries,
                     ::util::Status(const std::set<uint32>& table_ids,
                                    ::p4::ReadResponse* resp,
                                    std::vector<p4::TableEntry*>* acl_flows));
  MOCK_CONST_METHOD2(
      ReadActionProfileMembers,
      ::util::Status(const std::set<uint32>& action_profile_ids,
                     WriterInterface<::p4::ReadResponse>* writer));
  MOCK_CONST_METHOD2(
      ReadActionProfileGroups,
      ::util::Status(const std::set<uint32>& action_profile_ids,
                     WriterInterface<::p4::ReadResponse>* writer));
  MOCK_CONST_METHOD3(MapFlowEntry,
                     ::util::Status(const ::p4::TableEntry& table_entry,
                                    ::p4::Update::Type type,
                                    CommonFlowEntry* flow_entry));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_TABLE_MANAGER_MOCK_H_
