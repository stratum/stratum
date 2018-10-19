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


#include "stratum/hal/lib/bcm/bcm_table_manager.h"

#include <memory>
#include <vector>

#include "stratum/glue/status/canonical_errors.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_mock.h"
#include "stratum/hal/lib/bcm/constants.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "absl/strings/substitute.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"

using ::stratum::test_utils::EqualsProto;
using ::stratum::test_utils::UnorderedEqualsProto;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

namespace stratum {
namespace hal {
namespace bcm {

class BcmTableManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bcm_chassis_ro_mock_ = absl::make_unique<BcmChassisRoMock>();
    p4_table_mapper_mock_ = absl::make_unique<P4TableMapperMock>();
    bcm_table_manager_ = BcmTableManager::CreateInstance(
        bcm_chassis_ro_mock_.get(), p4_table_mapper_mock_.get(), kUnit);
  }

  void TearDown() override { ASSERT_OK(bcm_table_manager_->Shutdown()); }

  ::util::Status PopulateConfigAndPortMaps(
      ChassisConfig* config, std::map<uint32, SdkPort>* port_id_to_sdk_port,
      std::map<uint32, SdkTrunk>* trunk_id_to_sdk_trunk) {
    if (config) {
      const std::string& config_text =
          absl::Substitute(kChassisConfigTemplate, kNodeId, kPortId1,
                           kLogicalPort1, kPortId2, kLogicalPort2);
      RETURN_IF_ERROR(ParseProtoFromString(config_text, config));
    }
    if (port_id_to_sdk_port) {
      // Two ports on this unit.
      port_id_to_sdk_port->insert({kPortId1, {kUnit, kLogicalPort1}});
      port_id_to_sdk_port->insert({kPortId2, {kUnit, kLogicalPort2}});
    }
    if (trunk_id_to_sdk_trunk) {
      // One trunk on this unit.
      trunk_id_to_sdk_trunk->insert({kTrunkId1, {kUnit, kTrunkPort1}});
    }

    return ::util::OkStatus();
  }

  void PushTestConfig() {
    ChassisConfig config;
    std::map<uint32, SdkPort> port_id_to_sdk_port = {};
    std::map<uint32, SdkTrunk> trunk_id_to_sdk_trunk = {};
    ASSERT_OK(PopulateConfigAndPortMaps(&config, &port_id_to_sdk_port,
                                        &trunk_id_to_sdk_trunk));

    EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
        .WillOnce(Return(port_id_to_sdk_port));
    EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkIdToSdkTrunkMap(kNodeId))
        .WillOnce(Return(trunk_id_to_sdk_trunk));

    ASSERT_OK(bcm_table_manager_->PushChassisConfig(config, kNodeId));

    ASSERT_OK(VerifyInternalState());
  }

  ::util::Status VerifyInternalState() {
    CHECK_RETURN_IF_FALSE(kNodeId == bcm_table_manager_->node_id_);
    CHECK_RETURN_IF_FALSE(2U ==
                          bcm_table_manager_->port_id_to_logical_port_.size());
    CHECK_RETURN_IF_FALSE(1U ==
                          bcm_table_manager_->trunk_id_to_trunk_port_.size());
    CHECK_RETURN_IF_FALSE(
        bcm_table_manager_->port_id_to_logical_port_[kPortId1] ==
        kLogicalPort1);
    CHECK_RETURN_IF_FALSE(
        bcm_table_manager_->port_id_to_logical_port_[kPortId2] ==
        kLogicalPort2);
    CHECK_RETURN_IF_FALSE(
        bcm_table_manager_->trunk_id_to_trunk_port_[kTrunkId1] == kTrunkPort1);

    return ::util::OkStatus();
  }

  ::util::Status VerifyTableEntry(const ::p4::v1::TableEntry& entry,
                                  bool table_id_exists, bool key_match,
                                  bool proto_match) {
    ::util::StatusOr<::p4::v1::TableEntry> result =
        bcm_table_manager_->LookupTableEntry(entry);
    ::util::Status status = result.status();
    if (!table_id_exists) {
      CHECK_RETURN_IF_FALSE(
          !status.ok() &&
          absl::StrContains(status.error_message(), "Could not find table"))
          << "Did not expect table id to exist. Status: " << status;
      return ::util::OkStatus();
    }
    if (!key_match) {
      CHECK_RETURN_IF_FALSE(
          !status.ok() && absl::StrContains(status.error_message(),
                                            "does not contain a matching flow"))
          << "Did not expect key match. Status: " << status;
      return ::util::OkStatus();
    }
    RETURN_IF_ERROR(status);
    CHECK_RETURN_IF_FALSE(proto_match ==
                          ProtoEqual(result.ValueOrDie(), entry));
    return ::util::OkStatus();
  }

  ::util::Status VerifyActionProfileMember(
      const ::p4::v1::ActionProfileMember& member,
      BcmNonMultipathNexthop::Type type, int egress_intf_id, int bcm_port,
      uint32 group_ref_count, uint32 flow_ref_count) {
    CHECK_RETURN_IF_FALSE(
        bcm_table_manager_->ActionProfileMemberExists(member.member_id()));
    const auto& members = bcm_table_manager_->members_;
    auto it = members.find(member.member_id());
    CHECK_RETURN_IF_FALSE(it != members.end());
    CHECK_RETURN_IF_FALSE(ProtoEqual(member, it->second));
    BcmNonMultipathNexthopInfo info;
    RETURN_IF_ERROR(bcm_table_manager_->GetBcmNonMultipathNexthopInfo(
        member.member_id(), &info));
    CHECK_RETURN_IF_FALSE(type == info.type);
    CHECK_RETURN_IF_FALSE(egress_intf_id == info.egress_intf_id);
    CHECK_RETURN_IF_FALSE(bcm_port == info.bcm_port);
    CHECK_RETURN_IF_FALSE(group_ref_count == info.group_ref_count);
    CHECK_RETURN_IF_FALSE(flow_ref_count == info.flow_ref_count);

    return ::util::OkStatus();
  }

  ::util::Status VerifyActionProfileGroup(
      const ::p4::v1::ActionProfileGroup& group, int egress_intf_id,
      uint32 flow_ref_count,
      std::map<uint32, std::tuple<uint32, uint32, int>>
          member_id_to_weight_group_ref_count_port) {
    CHECK_RETURN_IF_FALSE(
        bcm_table_manager_->ActionProfileGroupExists(group.group_id()));
    const auto& groups = bcm_table_manager_->groups_;
    auto it = groups.find(group.group_id());
    CHECK_RETURN_IF_FALSE(it != groups.end());
    CHECK_RETURN_IF_FALSE(ProtoEqual(group, it->second));
    BcmMultipathNexthopInfo group_info;
    RETURN_IF_ERROR(bcm_table_manager_->GetBcmMultipathNexthopInfo(
        group.group_id(), &group_info));
    CHECK_RETURN_IF_FALSE(egress_intf_id == group_info.egress_intf_id);
    CHECK_RETURN_IF_FALSE(flow_ref_count == group_info.flow_ref_count);
    CHECK_RETURN_IF_FALSE(member_id_to_weight_group_ref_count_port.size() ==
                          group_info.member_id_to_weight.size());
    for (const auto& e : member_id_to_weight_group_ref_count_port) {
      CHECK_RETURN_IF_FALSE(std::get<0>(e.second) ==
                            group_info.member_id_to_weight[e.first]);
      BcmNonMultipathNexthopInfo member_info;
      RETURN_IF_ERROR(bcm_table_manager_->GetBcmNonMultipathNexthopInfo(
          e.first, &member_info));
      CHECK_RETURN_IF_FALSE(std::get<1>(e.second) ==
                            member_info.group_ref_count);
      CHECK_RETURN_IF_FALSE(std::get<2>(e.second) == member_info.bcm_port);
      // If this is a logical port, check that there is a mapping to the set of
      // referencing groups.
      auto* logical_port = gtl::FindOrNull(
          bcm_table_manager_->port_id_to_logical_port_, std::get<2>(e.second));
      if (logical_port) {
        auto* group_ids = gtl::FindOrNull(
            bcm_table_manager_->port_to_group_ids_, *logical_port);
        CHECK_RETURN_IF_FALSE(group_ids != nullptr);
        CHECK_RETURN_IF_FALSE(gtl::ContainsKey(*group_ids, group.group_id()));
      }
    }

    return ::util::OkStatus();
  }

  // Insert a simple action profile member with nexthop type port.
  ::util::Status InsertSimpleActionProfileMember(uint32 member_id) {
    ::p4::v1::ActionProfileMember member;
    member.set_member_id(member_id);
    member.set_action_profile_id(kActionProfileId1);
    ::util::Status profile_member_status =
        bcm_table_manager_->AddActionProfileMember(
            member, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
            kLogicalPort1);
    EXPECT_OK(profile_member_status)
        << "Failed to insert action profile member " << member_id;
    return profile_member_status;
  }

  // Insert a simple set of table entries and return a map from table_id to
  // table entry vector. Should only be run one time per node.
  absl::flat_hash_map<uint32, std::vector<::p4::v1::TableEntry>>
  InsertSimpleTableEntries(std::vector<uint32> tables, int entries_per_table) {
    absl::flat_hash_map<uint32, std::vector<::p4::v1::TableEntry>> entry_map;
    if (!InsertSimpleActionProfileMember(kMemberId1).ok()) {
      return entry_map;
    }
    for (uint32 table : tables) {
      for (int i = 0; i < entries_per_table; ++i) {
        ::p4::v1::TableEntry entry;
        entry.set_table_id(table);
        entry.add_match()->set_field_id(kFieldId1 + i);
        entry.mutable_action()->set_action_profile_member_id(kMemberId1);
        ::util::Status table_status = bcm_table_manager_->AddTableEntry(entry);
        EXPECT_OK(table_status)
            << "Failed to add entry " << i << " to table " << table;
        if (!table_status.ok()) continue;
        entry_map[table].push_back(entry);
      }
    }
    return entry_map;
  }

  // A configuration with 1 node (aka chip) and 2 ports.
  static constexpr char kChassisConfigTemplate[] = R"(
      description: "Sample Generic Trident2 config."
      chassis {
        platform: PLT_GENERIC_TRIDENT2
        name: "standalone"
      }
      nodes {
        id: $0
        slot: 1
      }
      singleton_ports {
        id: $1
        slot: 1
        port: $2
        speed_bps: 40000000000
      }
      singleton_ports {
        id: $3
        slot: 1
        port: $4
        speed_bps: 40000000000
      }
  )";
  static constexpr char kErrorMsg[] = "Some error";
  static constexpr uint64 kNodeId = 123123123;
  static constexpr int kUnit = 0;
  static constexpr uint64 kPortId1 = 111111111;
  static constexpr uint64 kPortId2 = 222222222;
  static constexpr uint64 kPortId3 = 333333333;
  static constexpr uint64 kTrunkId1 = 444444444;
  static constexpr int kPort1 = 1;
  static constexpr int kPort2 = 2;
  static constexpr int kLogicalPort1 = 33;
  static constexpr int kLogicalPort2 = 34;
  static constexpr int kTrunkPort1 = 77;
  static constexpr int kCpuPort = 0;
  static constexpr uint64 kSrcMac1 = 0x1122334455;
  static constexpr uint64 kDstMac1 = 0x1234512345;
  static constexpr uint32 kActionProfileId1 = 841;
  static constexpr uint32 kActionProfileId2 = 952;
  static constexpr uint32 kMemberId1 = 123;
  static constexpr uint32 kMemberId2 = 456;
  static constexpr uint32 kMemberId3 = 789;
  static constexpr uint32 kGroupId1 = 111;
  static constexpr uint32 kGroupId2 = 222;
  static constexpr uint32 kGroupId3 = 333;
  static constexpr int kEgressIntfId1 = 10001;
  static constexpr int kEgressIntfId2 = 10002;
  static constexpr int kEgressIntfId3 = 10003;
  static constexpr int kEgressIntfId4 = 20001;
  static constexpr int kEgressIntfId5 = 20002;
  static constexpr int kEgressIntfId6 = 20003;
  static constexpr uint32 kTableId1 = 345678;
  static constexpr uint32 kTableId2 = 456789;
  static constexpr uint32 kFieldId1 = 1;
  static constexpr uint32 kFieldId2 = 2;
  static constexpr int kClassId1 = 23;

  std::unique_ptr<BcmChassisRoMock> bcm_chassis_ro_mock_;
  std::unique_ptr<P4TableMapperMock> p4_table_mapper_mock_;
  std::unique_ptr<BcmTableManager> bcm_table_manager_;
};

constexpr char BcmTableManagerTest::kChassisConfigTemplate[];
constexpr char BcmTableManagerTest::kErrorMsg[];
constexpr uint64 BcmTableManagerTest::kNodeId;
constexpr int BcmTableManagerTest::kUnit;
constexpr uint64 BcmTableManagerTest::kPortId1;
constexpr uint64 BcmTableManagerTest::kPortId2;
constexpr uint64 BcmTableManagerTest::kPortId3;
constexpr uint64 BcmTableManagerTest::kTrunkId1;
constexpr int BcmTableManagerTest::kPort1;
constexpr int BcmTableManagerTest::kPort2;
constexpr int BcmTableManagerTest::kLogicalPort1;
constexpr int BcmTableManagerTest::kLogicalPort2;
constexpr int BcmTableManagerTest::kTrunkPort1;
constexpr int BcmTableManagerTest::kCpuPort;
constexpr uint64 BcmTableManagerTest::kSrcMac1;
constexpr uint64 BcmTableManagerTest::kDstMac1;
constexpr uint32 BcmTableManagerTest::kActionProfileId1;
constexpr uint32 BcmTableManagerTest::kActionProfileId2;
constexpr uint32 BcmTableManagerTest::kMemberId1;
constexpr uint32 BcmTableManagerTest::kMemberId2;
constexpr uint32 BcmTableManagerTest::kMemberId3;
constexpr uint32 BcmTableManagerTest::kGroupId1;
constexpr uint32 BcmTableManagerTest::kGroupId2;
constexpr uint32 BcmTableManagerTest::kGroupId3;
constexpr int BcmTableManagerTest::kEgressIntfId1;
constexpr int BcmTableManagerTest::kEgressIntfId2;
constexpr int BcmTableManagerTest::kEgressIntfId3;
constexpr int BcmTableManagerTest::kEgressIntfId4;
constexpr int BcmTableManagerTest::kEgressIntfId5;
constexpr int BcmTableManagerTest::kEgressIntfId6;
constexpr uint32 BcmTableManagerTest::kTableId1;
constexpr uint32 BcmTableManagerTest::kTableId2;
constexpr uint32 BcmTableManagerTest::kFieldId1;
constexpr uint32 BcmTableManagerTest::kFieldId2;
constexpr int BcmTableManagerTest::kClassId1;

namespace {

// Returns a BcmField containing the const condition for a P4HeaderType.
::util::StatusOr<BcmField> ConstCondition(P4HeaderType p4_header_type) {
  static const auto* field_map = new absl::flat_hash_map<P4HeaderType, string>({
      {P4_HEADER_ARP, "type: IP_TYPE value { u32: 0x0806 }"},
      {P4_HEADER_IPV4, "type: IP_TYPE value { u32: 0x0800 }"},
      {P4_HEADER_IPV6, "type: IP_TYPE value { u32: 0x86dd }"},
      {P4_HEADER_TCP, "type: IP_PROTO_NEXT_HDR value { u32: 6 }"},
      {P4_HEADER_UDP, "type: IP_PROTO_NEXT_HDR value { u32: 17 }"},
      {P4_HEADER_UDP_PAYLOAD, "type: IP_PROTO_NEXT_HDR value { u32: 17 }"},
      {P4_HEADER_GRE, "type: IP_PROTO_NEXT_HDR value { u32: 47 }"},
      {P4_HEADER_ICMP, "type: IP_PROTO_NEXT_HDR value { u32: 1 }"},
  });

  BcmField bcm_field;
  string bcm_field_proto_string =
      gtl::FindWithDefault(*field_map, p4_header_type, "");
  if (bcm_field_proto_string.empty()) {
    return util::NotFoundErrorBuilder(GTL_LOC)
           << "No const condition for header type "
           << P4HeaderType_Name(p4_header_type);
  }
  CHECK_OK(ParseProtoFromString(bcm_field_proto_string, &bcm_field));
  return bcm_field;
}

// Returns the name of a P4HeaderType parameter.
string ParamName(testing::TestParamInfo<P4HeaderType> param_info) {
  return P4HeaderType_Name(param_info.param);
}

void FillBcmTableEntryValue(const MappedField::Value& source,
                            BcmTableEntryValue* destination) {
  switch (source.data_case()) {
    case MappedField::Value::kU32:
      destination->set_u32(source.u32());
      break;
    case MappedField::Value::kU64:
      destination->set_u64(source.u64());
      break;
    case MappedField::Value::kB:
      destination->set_b(source.b());
      break;
    case MappedField::Value::kRawPiMatch:
      break;  // Unhandled for now.
    case MappedField::Value::DATA_NOT_SET:
      break;  // Don't do anything if there is no value.
  }
}

// Strip "P4_FIELD_TYPE_" from the type name and copy all parameters from a
// MappedField to a BcmField.
bool StripFieldTypeAndCopyToBcm(const MappedField& p4_field,
                                BcmField* bcm_field) {
  bcm_field->Clear();
  std::string bcm_field_name = std::string(
      absl::StripPrefix(P4FieldType_Name(p4_field.type()), "P4_FIELD_TYPE_"));
  BcmField::Type bcm_field_type;
  if (!BcmField::Type_Parse(bcm_field_name, &bcm_field_type)) {
    return false;
  }
  bcm_field->set_type(bcm_field_type);
  if (p4_field.has_value()) {
    FillBcmTableEntryValue(p4_field.value(), bcm_field->mutable_value());
  }
  if (p4_field.has_mask()) {
    FillBcmTableEntryValue(p4_field.mask(), bcm_field->mutable_mask());
  }
  return true;
}

// Return a constant reference to a vector set of pairs containing default
// values for all implemented p4 --> bcm field translations.
const std::vector<std::pair<MappedField, BcmField>>& P4ToBcmFields() {
  static auto* field_map = []() {
    auto* field_map = new std::vector<std::pair<MappedField, BcmField>>();
    MappedField p4_field;
    BcmField bcm_field;
    // P4_FIELD_TYPE_UNKNOWN: No conversion.
    // P4_FIELD_TYPE_ANNOTATED: No conversion.
    //
    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_SRC
      value { u64: 11111111111111 }
      mask { u64: 99999999999999 }
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_DST
      value { u64: 22222222222222 }
      mask { u64: 99999999999999 }
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));
    // P4_FIELD_TYPE_ETH_TYPE: No currentf conversion.
    // P4_FIELD_TYPE_VLAN_VID: No current conversion.
    // P4_FIELD_TYPE_VLAN_PCP: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV4_SRC
      value { u32: 11111111 }
      mask { u32: 99999999 }
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV4_DST
      value { u32: 22222222 }
      mask { u32: 99999999 }
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    // P4_FIELD_TYPE_IPV4_PROTO: No current conversion.
    // P4_FIELD_TYPE_IPV4_DIFFSERV: No current conversion.
    // P4_FIELD_TYPE_NW_TTL: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV6_SRC
      value { b: "\x00\x01\x02\x03\x04\x05" }
      mask { b: "\xaf\xaf\xaf\xaf\xaf\xaf" }
    )PROTO", &p4_field));
    // IPV6_SRC translated to IPV6_SRC_UPPER_64.
    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: IPV6_SRC_UPPER_64
      value { b: "\x00\x01\x02\x03\x04\x05" }
      mask { b: "\xaf\xaf\xaf\xaf\xaf\xaf" }
    )PROTO", &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV6_DST
      value { b: "\x10\x11\x12\x13\x14\x15" }
      mask { b: "\xcf\xcf\xcf\xcf\xcf\xcf" }
    )PROTO", &p4_field));
    // IPV6_DST translated to IPV6_SRC_UPPER_64.
    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: IPV6_DST_UPPER_64
      value { b: "\x10\x11\x12\x13\x14\x15" }
      mask { b: "\xcf\xcf\xcf\xcf\xcf\xcf" }
    )PROTO", &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    // P4_FIELD_TYPE_IPV6_NEXT_HDR: No current conversion.
    // P4_FIELD_TYPE_IPV6_TRAFFIC_CLASS: No current conversion.
    // P4_FIELD_TYPE_ICMP_CODE: No current conversion.
    // P4_FIELD_TYPE_L4_SRC_PORT: No current conversion.
    // P4_FIELD_TYPE_L4_DST_PORT: No current conversion.
    // P4_FIELD_TYPE_ARP_TPA: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_VRF
      value { u32: 1234 }
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_field));
    field_map->push_back(std::make_pair(p4_field, bcm_field));

    // P4_FIELD_TYPE_CLASS_ID: No current conversion.
    // P4_FIELD_TYPE_COLOR: No current conversion.
    // P4_FIELD_TYPE_EGRESS_PORT: No current conversion.
    // P4_FIELD_TYPE_INGRESS_PORT: No current conversion.
    // P4_FIELD_TYPE_IN_METER: No current conversion.

    return field_map;
  }();
  return *field_map;
}

// Strip "P4_FIELD_TYPE_" from the type name and copy all parameters from a
// MappedField to a BcmField.
bool StripFieldTypeAndCopyToBcm(
    const P4ActionFunction::P4ActionFields& p4_field, BcmAction* bcm_action) {
  bcm_action->Clear();
  std::string bcm_field_name = std::string(absl::StripPrefix(
      P4FieldType_Name(p4_field.type()).c_str(), "P4_FIELD_TYPE_"));
  BcmAction::Type bcm_action_type;
  BcmAction::Param::Type bcm_action_param_type;
  if (bcm_field_name == "CLASS_ID") {
    // TODO: Remove this block once P4 class id qualifier
    // handling is fixed.
    bcm_action_type = BcmAction::SET_VFP_DST_CLASS_ID;
    bcm_action_param_type = BcmAction::Param::VFP_DST_CLASS_ID;
  } else if (bcm_field_name == "VLAN_VID") {
    // TODO: This if-else block will need to be changed to
    // accommodate actions beyond setting a field (e.g adding VLAN tag as
    // opposed to seetting the current outer VLAN tag).
    bcm_action_type = BcmAction::ADD_OUTER_VLAN;
    bcm_action_param_type = BcmAction::Param::VLAN_VID;
  } else if (!BcmAction::Type_Parse("SET_" + bcm_field_name,
                                    &bcm_action_type) ||
             !BcmAction::Param::Type_Parse(bcm_field_name,
                                           &bcm_action_param_type)) {
    return false;
  }
  bcm_action->set_type(bcm_action_type);
  auto* param = bcm_action->add_params();
  param->set_type(bcm_action_param_type);
  switch (p4_field.value_case()) {
    case P4ActionFunction::P4ActionFields::kU32:
      param->mutable_value()->set_u32(p4_field.u32());
      break;
    case P4ActionFunction::P4ActionFields::kU64:
      param->mutable_value()->set_u64(p4_field.u64());
      break;
    case P4ActionFunction::P4ActionFields::kB:
      param->mutable_value()->set_b(p4_field.b());
      break;
    default:
      break;
  }
  return true;
}

// Return a constant reference to a vector set of pairs containing default
// values for all implemented p4 --> bcm field translations.
const std::vector<std::pair<P4ActionFunction::P4ActionFields, BcmAction>>&
P4ToBcmActions() {
  static auto* field_map = []() {
    auto* field_map = new std::vector<
        std::pair<P4ActionFunction::P4ActionFields, BcmAction>>();
    P4ActionFunction::P4ActionFields p4_field;
    BcmAction bcm_action;
    // P4_FIELD_TYPE_UNKNOWN: No conversion.
    // P4_FIELD_TYPE_ANNOTATED: No conversion.
    //
    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_SRC
      u64: 11111111111111
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_DST
      u64: 22222222222222
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));
    // P4_FIELD_TYPE_ETH_TYPE: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_VLAN_VID u32: 22
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_VLAN_PCP u32: 22
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV4_SRC
      u32: 11111111
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV4_DST
      u32: 22222222
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    // P4_FIELD_TYPE_IPV4_PROTO: No current conversion.
    // P4_FIELD_TYPE_IPV4_DIFFSERV: No current conversion.
    // P4_FIELD_TYPE_NW_TTL: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV6_SRC
      b: "\x00\x01\x02\x03\x04\x05"
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_IPV6_DST
      b: "\x10\x11\x12\x13\x14\x15"
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    // P4_FIELD_TYPE_IPV6_NEXT_HDR: No current conversion.
    // P4_FIELD_TYPE_IPV6_TRAFFIC_CLASS: No current conversion.
    // P4_FIELD_TYPE_ICMP_CODE: No current conversion.
    // P4_FIELD_TYPE_L4_SRC_PORT: No current conversion.
    // P4_FIELD_TYPE_L4_DST_PORT: No current conversion.
    // P4_FIELD_TYPE_ARP_TPA: No current conversion.

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_VRF u32: 1234
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));

    EXPECT_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_CLASS_ID
      u32: 1234
    )PROTO", &p4_field));
    EXPECT_TRUE(StripFieldTypeAndCopyToBcm(p4_field, &bcm_action));
    field_map->push_back(std::make_pair(p4_field, bcm_action));
    // P4_FIELD_TYPE_COLOR: No current conversion.
    // P4_FIELD_TYPE_EGRESS_PORT: No current conversion.
    // P4_FIELD_TYPE_INGRESS_PORT: No current conversion.
    // P4_FIELD_TYPE_IN_METER: No current conversion.

    return field_map;
  }();
  return *field_map;
}

enum Color { kRed = 1, kYellow, kGreen };
constexpr int kNumColors = 3;

struct ColorSet {
  absl::flat_hash_set<Color> colors;
  std::size_t hash() {
    return (colors.count(kRed) << kRed) + (colors.count(kYellow) << kYellow) +
           (colors.count(kGreen) << kGreen);
  }
};

struct CopyDropColors {
  ColorSet copy;
  ColorSet drop;
  std::size_t hash() { return (copy.hash() << 16) + drop.hash(); }
};

struct ColorTestCase {
  CopyDropColors input;
  CopyDropColors output;
};

P4MeterColor ToP4MeterColor(Color color) {
  switch (color) {
    case kRed:
      return P4_METER_RED;
    case kYellow:
      return P4_METER_YELLOW;
    case kGreen:
      return P4_METER_GREEN;
  }
}

BcmAction::Param::Color ToBcmActionParamColor(Color color) {
  switch (color) {
    case kRed:
      return BcmAction::Param::RED;
    case kYellow:
      return BcmAction::Param::YELLOW;
    case kGreen:
      return BcmAction::Param::GREEN;
  }
}

void FillP4CopyToCpuAction(uint32 cpu_queue, CopyDropColors params,
                           CommonFlowEntry* entry) {
  entry->mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  // Queue ID
  P4ActionFunction::P4ActionFields* queue_id_action =
      entry->mutable_action()->mutable_function()->add_modify_fields();
  queue_id_action->set_type(P4_FIELD_TYPE_CPU_QUEUE_ID);
  queue_id_action->set_u32(cpu_queue);
  // Packets to CPU now have the clone port set.
  P4ActionFunction::P4ActionFields* clone_port_action =
      entry->mutable_action()->mutable_function()->add_modify_fields();
  clone_port_action->set_type(P4_FIELD_TYPE_CLONE_PORT);
  clone_port_action->set_u32(cpu_queue);  // The actual port doesn't matter.
  // Clone
  if (!params.copy.colors.empty()) {
    P4ActionFunction::P4ActionPrimitive* clone_action =
        entry->mutable_action()->mutable_function()->add_primitives();
    clone_action->set_op_code(P4_ACTION_OP_CLONE);
    for (Color color : params.copy.colors) {
      clone_action->add_meter_colors(ToP4MeterColor(color));
    }
  }
  // Drop
  if (!params.drop.colors.empty()) {
    P4ActionFunction::P4ActionPrimitive* drop_action =
        entry->mutable_action()->mutable_function()->add_primitives();
    drop_action->set_op_code(P4_ACTION_OP_DROP);
    for (Color color : params.drop.colors) {
      drop_action->add_meter_colors(ToP4MeterColor(color));
    }
  }
}

void FillP4SendToCpuAction(uint32 cpu_queue, CopyDropColors params,
                           CommonFlowEntry* entry) {
  entry->mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  // Queue ID
  P4ActionFunction::P4ActionFields* queue_id_action =
      entry->mutable_action()->mutable_function()->add_modify_fields();
  queue_id_action->set_type(P4_FIELD_TYPE_CPU_QUEUE_ID);
  queue_id_action->set_u32(cpu_queue);
  // Packets to CPU now have the clone port set.
  P4ActionFunction::P4ActionFields* clone_port_action =
      entry->mutable_action()->mutable_function()->add_modify_fields();
  clone_port_action->set_type(P4_FIELD_TYPE_CLONE_PORT);
  clone_port_action->set_u32(cpu_queue);  // The actual port doesn't matter.
  // Egress
  if (!params.copy.colors.empty()) {
    P4ActionFunction::P4ActionFields* send_action =
        entry->mutable_action()->mutable_function()->add_modify_fields();
    send_action->set_type(P4_FIELD_TYPE_EGRESS_PORT);
    send_action->set_u64(kCpuPortId);
    if (params.copy.colors.size() < kNumColors) {
      for (Color color : params.copy.colors) {
        send_action->add_meter_colors(ToP4MeterColor(color));
      }
    }
  }
  // Drop
  if (!params.drop.colors.empty()) {
    P4ActionFunction::P4ActionPrimitive* drop_action =
        entry->mutable_action()->mutable_function()->add_primitives();
    drop_action->set_op_code(P4_ACTION_OP_DROP);
    if (params.drop.colors.size() < kNumColors) {
      for (Color color : params.drop.colors) {
        drop_action->add_meter_colors(ToP4MeterColor(color));
      }
    }
  }
}

void FillBcmCopyToCpuAction(uint32 cpu_queue, CopyDropColors params,
                            BcmFlowEntry* entry) {
  // Copy
  BcmAction copy_template;
  copy_template.set_type(BcmAction::COPY_TO_CPU);
  copy_template.add_params()->set_type(BcmAction::Param::QUEUE);
  copy_template.mutable_params(0)->mutable_value()->set_u32(cpu_queue);
  if (!params.copy.colors.empty()) {
    if (params.copy.colors.size() == kNumColors) {
      *entry->add_actions() = copy_template;
    } else {
      for (Color color : params.copy.colors) {
        BcmAction action = copy_template;
        action.add_params()->set_type(BcmAction::Param::COLOR);
        action.mutable_params(1)->mutable_value()->set_u32(
            ToBcmActionParamColor(color));
        *entry->add_actions() = action;
      }
    }
  }
  // Drop
  BcmAction drop_template;
  drop_template.set_type(BcmAction::DROP);
  if (!params.drop.colors.empty()) {
    if (params.drop.colors.size() == kNumColors) {
      *entry->add_actions() = drop_template;
    } else {
      for (Color color : params.drop.colors) {
        BcmAction action = drop_template;
        action.add_params()->set_type(BcmAction::Param::COLOR);
        action.mutable_params(0)->mutable_value()->set_u32(
            ToBcmActionParamColor(color));
        *entry->add_actions() = action;
      }
    }
  }
}

const std::vector<ColorTestCase>& SendToCpuTestCases() {
  static const auto* test_cases = []() {
    const absl::flat_hash_set<Color> all = {kRed, kYellow, kGreen};
    auto* test_cases = new std::vector<ColorTestCase>();
    ColorTestCase test_case;
    // NO Drop
    test_case.input.drop.colors = {};
    //   Red Send
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {kRed};
    test_case.output.drop.colors = {kRed};
    test_cases->push_back(test_case);
    //   Yellow Send
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //   Green Send
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {kGreen};
    test_case.output.drop.colors = {kGreen};
    test_cases->push_back(test_case);
    //   Red/Yellow Send
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {kRed, kYellow};
    test_case.output.drop.colors = {kRed, kYellow};
    test_cases->push_back(test_case);
    //   Red/Green Send
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {kRed, kGreen};
    test_case.output.drop.colors = {kRed, kGreen};
    test_cases->push_back(test_case);
    //   Yellow/Green Send
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Triple Color Send
    test_case.input.copy.colors = {all};
    test_case.output.copy.colors = {all};
    test_case.output.drop.colors = {all};
    test_cases->push_back(test_case);
    // RED Drop
    test_case.input.drop.colors = {kRed};
    //   Red Send
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow Send
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = {kRed, kYellow};
    test_cases->push_back(test_case);
    //   Green Send
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {kGreen};
    test_case.output.drop.colors = {kRed, kGreen};
    test_cases->push_back(test_case);
    //   Red/Yellow Send
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Red/Green Send
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow/Green Send
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    //   Triple Color Send
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    // RED/GREEN Drop
    test_case.input.drop.colors = {kRed, kGreen};
    //   Red Send
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow Send
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    //   Green Send
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Red/Yellow Send
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Red/Green Send
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow/Green Send
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Triple Color Send
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    // ALL Drop
    test_case.input.drop.colors = all;
    //  Red Send
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //  Red/Green Send
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //  Triple Color Send
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = {};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);

    return test_cases;
  }();
  return *test_cases;
}

const std::vector<ColorTestCase>& CopyToCpuTestCases() {
  static const auto* test_cases = []() {
    const absl::flat_hash_set<Color> all = {kRed, kYellow, kGreen};
    auto* test_cases = new std::vector<ColorTestCase>();
    ColorTestCase test_case;
    // NO Drop
    test_case.input.drop.colors = {};
    //   Red Copy
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {kRed};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow Copy
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Green Copy
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {kGreen};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Red/Yellow Copy
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {kRed, kYellow};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Red/Green Copy
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {kRed, kGreen};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Yellow/Green Copy
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    //   Triple Color Copy
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = all;
    test_case.output.drop.colors = {};
    test_cases->push_back(test_case);
    // YELLOW Drop
    test_case.input.drop.colors = {kYellow};
    //   Red Copy
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {kRed};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //   Yellow Copy
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //   Green Copy
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {kGreen};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //  Red/Yellow Copy
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {kRed, kYellow};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //  Red/Green Copy
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {kRed, kGreen};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //  Yellow/Green Copy
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    //  Triple Color Copy
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = all;
    test_case.output.drop.colors = {kYellow};
    test_cases->push_back(test_case);
    // YELLOW/GREEN Drop
    test_case.input.drop.colors = {kYellow, kGreen};
    //   Red Copy
    test_case.input.copy.colors = {kRed};
    test_case.output.copy.colors = {kRed};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Yellow Copy
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Green Copy
    test_case.input.copy.colors = {kGreen};
    test_case.output.copy.colors = {kGreen};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Red/Yellow Copy
    test_case.input.copy.colors = {kRed, kYellow};
    test_case.output.copy.colors = {kRed, kYellow};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Red/Green Copy
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {kRed, kGreen};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Yellow/Green Copy
    test_case.input.copy.colors = {kYellow, kGreen};
    test_case.output.copy.colors = {kYellow, kGreen};
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    //   Triple Color Copy
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = all;
    test_case.output.drop.colors = {kYellow, kGreen};
    test_cases->push_back(test_case);
    // Triple Color Drop
    test_case.input.drop.colors = all;
    //   Yellow Copy
    test_case.input.copy.colors = {kYellow};
    test_case.output.copy.colors = {kYellow};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    //   Red/Green Copy
    test_case.input.copy.colors = {kRed, kGreen};
    test_case.output.copy.colors = {kRed, kGreen};
    test_case.output.drop.colors = all;
    test_cases->push_back(test_case);
    //   Triple Color Copy
    test_case.input.copy.colors = all;
    test_case.output.copy.colors = all;
    test_case.output.drop.colors = all;

    return test_cases;
  }();
  return *test_cases;
}

AclTable CreateAclTable(
    uint32 p4_id, std::vector<uint32> match_fields, BcmAclStage stage, int size,
    int16 priority = 0, int physical_table_id = 0,
    const absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>&
        const_conditions = {}) {
  ::p4::config::v1::Table p4_table;
  p4_table.mutable_preamble()->set_id(p4_id);
  for (uint32 match_field : match_fields) {
    p4_table.add_match_fields()->set_id(match_field);
  }
  p4_table.set_size(size);
  AclTable table(p4_table, stage, priority, const_conditions);
  table.SetPhysicalTableId(physical_table_id);
  return table;
}

}  // namespace

TEST_F(BcmTableManagerTest, PushChassisConfigSuccess) {
  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  std::map<uint32, SdkTrunk> trunk_id_to_sdk_trunk = {};
  ASSERT_OK(PopulateConfigAndPortMaps(&config, &port_id_to_sdk_port,
                                      &trunk_id_to_sdk_trunk));

  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
      .Times(3)
      .WillRepeatedly(Return(port_id_to_sdk_port));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkIdToSdkTrunkMap(kNodeId))
      .Times(3)
      .WillRepeatedly(Return(trunk_id_to_sdk_trunk));

  // Call verify and then push multiple times with no issues. The make sure
  // the interna state is as expected.
  for (int i = 0; i < 3; ++i) {
    ASSERT_OK(bcm_table_manager_->VerifyChassisConfig(config, kNodeId));
    ASSERT_OK(bcm_table_manager_->PushChassisConfig(config, kNodeId));
  }

  ASSERT_OK(VerifyInternalState());
}

TEST_F(BcmTableManagerTest, PushChassisConfigFailure_ChassisManagerCallFails) {
  ChassisConfig config;

  // Failure when GetPortIdToSdkPortMap fails
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));
  ::util::Status status =
      bcm_table_manager_->PushChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());

  // Failure when GetTrunkIdToSdkTrunkMap fails
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
      .WillOnce(Return(std::map<uint32, SdkPort>()));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkIdToSdkTrunkMap(kNodeId))
      .WillOnce(
          Return(::util::Status(StratumErrorSpace(), ERR_CANCELLED, "Blah")));
  status = bcm_table_manager_->PushChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_CANCELLED, status.error_code());
}

TEST_F(BcmTableManagerTest,
       PushChassisConfigFailure_BadPortDataFromChassisManager) {
  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  std::map<uint32, SdkTrunk> trunk_id_to_sdk_trunk = {};
  ASSERT_OK(PopulateConfigAndPortMaps(&config, &port_id_to_sdk_port,
                                      &trunk_id_to_sdk_trunk));

  // Add a port from an unknown unit.
  port_id_to_sdk_port.insert({kPortId1 + 1, {kUnit + 1, kLogicalPort1 + 1}});

  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
      .WillOnce(Return(port_id_to_sdk_port));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkIdToSdkTrunkMap(kNodeId))
      .WillOnce(Return(trunk_id_to_sdk_trunk));

  ::util::Status status =
      bcm_table_manager_->PushChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("1 != 0 for a singleton port"));
}

TEST_F(BcmTableManagerTest,
       PushChassisConfigFailure_BadTrunkDataFromChassisManager) {
  ChassisConfig config;
  std::map<uint32, SdkPort> port_id_to_sdk_port = {};
  std::map<uint32, SdkTrunk> trunk_id_to_sdk_trunk = {};
  ASSERT_OK(PopulateConfigAndPortMaps(&config, &port_id_to_sdk_port,
                                      &trunk_id_to_sdk_trunk));

  // Add trunk from an unknown unit.
  trunk_id_to_sdk_trunk.insert({kTrunkId1 + 1, {kUnit + 1, kTrunkPort1 + 1}});

  EXPECT_CALL(*bcm_chassis_ro_mock_, GetPortIdToSdkPortMap(kNodeId))
      .WillOnce(Return(port_id_to_sdk_port));
  EXPECT_CALL(*bcm_chassis_ro_mock_, GetTrunkIdToSdkTrunkMap(kNodeId))
      .WillOnce(Return(trunk_id_to_sdk_trunk));

  ::util::Status status =
      bcm_table_manager_->PushChassisConfig(config, kNodeId);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("1 != 0 for a trunk"));
}

TEST_F(BcmTableManagerTest, VerifyChassisConfigSuccess) {
  ChassisConfig config;
  config.add_nodes()->set_id(kNodeId);
  EXPECT_OK(bcm_table_manager_->VerifyChassisConfig(config, kNodeId));
}

TEST_F(BcmTableManagerTest, VerifyChassisConfigFailure) {
  ChassisConfig config;

  // Failure for invalid node_id
  ::util::Status status = bcm_table_manager_->VerifyChassisConfig(config, 0);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());

  // After the first config push, any change in node_id is reboot required.
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  status = bcm_table_manager_->VerifyChassisConfig(config, kNodeId + 1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
}

TEST_F(BcmTableManagerTest, Shutdown) {
  EXPECT_OK(bcm_table_manager_->Shutdown());
}

TEST_F(BcmTableManagerTest, PushForwardingPipelineConfigSuccess) {
  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_OK(bcm_table_manager_->PushForwardingPipelineConfig(config));
}

TEST_F(BcmTableManagerTest, PushForwardingPipelineConfigFailure) {
  // TODO: Implement if needed.
}

TEST_F(BcmTableManagerTest, VerifyForwardingPipelineConfigSuccess) {
  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_OK(bcm_table_manager_->VerifyForwardingPipelineConfig(config));
}

TEST_F(BcmTableManagerTest, VerifyForwardingPipelineConfigFailure) {
  // TODO: Implement if needed.
}

TEST_F(BcmTableManagerTest, FillBcmFlowEntrySuccess) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, FillBcmFlowEntryFailure) {
  // TODO: Implement this test.
}

// Test that valid meter configuration for ACL flow is correctly copied from
// P4 TableEntry to BcmFlowEntry.
TEST_F(BcmTableManagerTest, FillBcmMeterConfigSuccess) {
  ::p4::v1::MeterConfig p4_meter;
  p4_meter.set_cir(512);
  p4_meter.set_cburst(64);
  p4_meter.set_pir(1024);
  p4_meter.set_pburst(128);
  BcmMeterConfig bcm_meter;
  EXPECT_OK(bcm_table_manager_->FillBcmMeterConfig(p4_meter, &bcm_meter));
  BcmMeterConfig expected;
  expected.set_committed_rate(512);
  expected.set_committed_burst(64);
  expected.set_peak_rate(1024);
  expected.set_peak_burst(128);
  EXPECT_TRUE(ProtoEqual(expected, bcm_meter))
      << "Expected: " << expected.ShortDebugString()
      << ", got: " << bcm_meter.ShortDebugString();
}

// Test failure to copy bad meter configuration to BcmMeterConfig.
TEST_F(BcmTableManagerTest, FillBcmMeterConfigBadValueFailure) {
  BcmMeterConfig bcm_meter;
  ::p4::v1::MeterConfig p4_meter;
  p4_meter.set_cir(-1);
  EXPECT_FALSE(
      bcm_table_manager_->FillBcmMeterConfig(p4_meter, &bcm_meter).ok());
  bcm_meter.Clear();
  p4_meter.set_cir(0x1ffffffffLL);
  EXPECT_FALSE(
      bcm_table_manager_->FillBcmMeterConfig(p4_meter, &bcm_meter).ok());
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_NoPipelineStage) {
  CommonFlowEntry source;

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid stage for the table entry"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_UnknownTableType) {
  CommonFlowEntry source;

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Could not find BCM table id from"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidMyStationFlow_WithNoAction) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);

  // Setup empty action for source.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidMyStationFlow_WithValidAction) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);

  // Setup empty action for source.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_L3_ADMIT);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidMyStationFlow_WithInvalidAction) {
  CommonFlowEntry source;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);

  // Setup empty action for source.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_UNKNOWN);

  // Set priorities.
  source.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("P4 Field Type P4_FIELD_TYPE_UNKNOWN (0) is not supported"));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidMulticastFlow_WithValidAction) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MULTICAST);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_L2_MULTICAST);

  // Set up empty action for source.
  source.mutable_action()->set_type(P4ActionType::P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_u32(1);
  field->set_type(P4_FIELD_TYPE_MCAST_GROUP_ID);
  auto* action = expected.add_actions();
  action->set_type(BcmAction::SET_L2_MCAST_GROUP);
  auto* param = action->add_params();
  param->mutable_value()->set_u32(1);
  param->set_type(BcmAction::Param::L2_MCAST_GROUP_ID);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidMulticastFlow_WithInvalidAction) {
  CommonFlowEntry source;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MULTICAST);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);

  // Set up empty action for source.
  source.mutable_action()->set_type(P4ActionType::P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_u32(1);
  field->set_type(P4_FIELD_TYPE_UNKNOWN);

  // Set priorities.
  source.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("P4 Field Type P4_FIELD_TYPE_UNKNOWN (0) is not supported"));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Delete_ValidMyStationFlow_WithValidAction) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);

  // Setup empty action for source.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_L3_ADMIT);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::DELETE, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Delete_ValidMulticastFlow_WithValidAction) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MULTICAST);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L2);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_L2_MULTICAST);

  // Set up empty action for source.
  source.mutable_action()->set_type(P4ActionType::P4_ACTION_TYPE_FUNCTION);
  auto* field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  field->set_u32(1);
  field->set_type(P4_FIELD_TYPE_MCAST_GROUP_ID);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::DELETE, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidIPv4LpmFlowFields) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv6 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV6") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV4_LPM);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntrys_ValidIPv6LpmFlowFields) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup priority. Although not used, we still accept priority set by
  // controller for LPM flows.
  source.set_priority(10);
  expected.set_priority(10);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmFlowFields_InvalidVrf) {
  CommonFlowEntry source;

  // Setup L3 table type and stage
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  // Setup the DST IP field.
  MappedField* ip_field = source.add_fields();
  ASSERT_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_IPV4_DST
    value { u32: 1 }
    mask { u32: 0xffffffff }
  )PROTO", ip_field));

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;

  // VRF fields cannot have a mask.
  MappedField* vrf_field = source.add_fields();
  ASSERT_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_VRF
    value { u32: 1 }
    mask { u32: 1 }
  )PROTO", vrf_field));
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("VRF match fields do not accept a mask value."));

  // VRF fields cannot have an out-of-range value.
  ASSERT_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_VRF
    value { u32: 99999999 }
  )PROTO", vrf_field));
  status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("VRF (99999999) is out of range"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmFlowFields_NoVrfForIpv4) {
  CommonFlowEntry source;

  ASSERT_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_IPV4_DST
    value { u32: 22 }
    mask { u32: 99 }
  )PROTO", source.add_fields()));

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;

  // This should fail because the vrf is not set.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("VRF not set for an L3 LPM flow"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmFlowFields_NoVrfForIpv6) {
  CommonFlowEntry source;

  ASSERT_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_IPV6_DST
    value { b: "\x22\x23" }
    mask { b: "\xff\xff" }
  )PROTO", source.add_fields()));

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;

  // This should fail because the vrf is not set.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("VRF not set for an L3 LPM flow"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmFlow_NoAction) {
  CommonFlowEntry source;

  // Setup the fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;

  // Entries need an action.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr(
          "Invalid or unsupported P4 action type: P4_ACTION_TYPE_UNKNOWN"));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmDirectPortNexthop_Priority) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Set priority for one flow. Althought it is not used, the stack should
  // accept the flow.
  expected.set_priority(10000);
  source.set_priority(10000);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    switch (pair.first.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
      case P4_FIELD_TYPE_ETH_DST:
        *source.mutable_action()->mutable_function()->add_modify_fields() =
            pair.first;
        *expected.add_actions() = pair.second;
        break;
      default:
        break;
    }
  }
  auto* p4_egress_field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  p4_egress_field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  p4_egress_field->set_u32(kPortId1);
  auto* bcm_egress_action = expected.add_actions();
  bcm_egress_action->set_type(BcmAction::OUTPUT_PORT);
  auto* bcm_egress_param = bcm_egress_action->add_params();
  bcm_egress_param->set_type(BcmAction::Param::LOGICAL_PORT);
  bcm_egress_param->mutable_value()->set_u32(kLogicalPort1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmDirectTrunkNexthop_Priority) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Set priority for one flow. Althought it is not used, the stack should
  // accept the flow.
  expected.set_priority(10000);
  source.set_priority(10000);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    switch (pair.first.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
      case P4_FIELD_TYPE_ETH_DST:
        *source.mutable_action()->mutable_function()->add_modify_fields() =
            pair.first;
        *expected.add_actions() = pair.second;
        break;
      default:
        break;
    }
  }
  auto* p4_egress_field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  p4_egress_field->set_type(P4_FIELD_TYPE_EGRESS_TRUNK);
  p4_egress_field->set_u32(kTrunkId1);
  auto* bcm_egress_action = expected.add_actions();
  bcm_egress_action->set_type(BcmAction::OUTPUT_TRUNK);
  auto* bcm_egress_param = bcm_egress_action->add_params();
  bcm_egress_param->set_type(BcmAction::Param::TRUNK_PORT);
  bcm_egress_param->mutable_value()->set_u32(kTrunkPort1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Delete_ValidLpmDirectNexthop_Priority) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Set priority for one flow. Althought it is not used, the stack should
  // accept the flow.
  expected.set_priority(10000);
  source.set_priority(10000);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions. Although it is a DELETE, controller can populate the
  // actions in the flow. We ignore it.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    switch (pair.first.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
      case P4_FIELD_TYPE_ETH_DST:
        *source.mutable_action()->mutable_function()->add_modify_fields() =
            pair.first;
        break;
      default:
        break;
    }
  }
  auto* p4_egress_field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  p4_egress_field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  p4_egress_field->set_u32(kPortId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::DELETE, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmMemberPortNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // We first need to add one member before.
  ::p4::v1::ActionProfileMember member;

  member.set_member_id(kMemberId1);
  member.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
  source.mutable_action()->set_profile_member_id(kMemberId1);
  auto* bcm_action = expected.add_actions();
  bcm_action->set_type(BcmAction::OUTPUT_PORT);
  auto* param = bcm_action->add_params();
  param->set_type(BcmAction::Param::EGRESS_INTF_ID);
  param->mutable_value()->set_u32(kEgressIntfId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Delete_ValidLpmMemberPortNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::DELETE, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmMemberTrunkNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // We first need to add one member before.
  ::p4::v1::ActionProfileMember member;

  member.set_member_id(kMemberId1);
  member.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId1,
      kTrunkPort1));

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
  source.mutable_action()->set_profile_member_id(kMemberId1);
  auto* bcm_action = expected.add_actions();
  bcm_action->set_type(BcmAction::OUTPUT_TRUNK);
  auto* param = bcm_action->add_params();
  param->set_type(BcmAction::Param::EGRESS_INTF_ID);
  param->mutable_value()->set_u32(kEgressIntfId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmMemberDropNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // We first need to add one member before.
  ::p4::v1::ActionProfileMember member;

  member.set_member_id(kMemberId1);
  member.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member, BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP, kEgressIntfId1,
      kLogicalPort1));

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
  source.mutable_action()->set_profile_member_id(kMemberId1);
  auto* bcm_action = expected.add_actions();
  bcm_action->set_type(BcmAction::DROP);
  auto* param = bcm_action->add_params();
  param->set_type(BcmAction::Param::EGRESS_INTF_ID);
  param->mutable_value()->set_u32(kEgressIntfId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidLpmGroupNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // We first need to add one group with one member before.
  ::p4::v1::ActionProfileMember member;
  ::p4::v1::ActionProfileGroup group;

  member.set_member_id(kMemberId1);
  member.set_action_profile_id(kActionProfileId1);
  group.set_group_id(kGroupId1);
  group.set_action_profile_id(kActionProfileId1);
  group.add_members()->set_member_id(kMemberId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group, kEgressIntfId1));

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_GROUP_ID);
  source.mutable_action()->set_profile_group_id(kGroupId1);
  auto* bcm_action = expected.add_actions();
  bcm_action->set_type(BcmAction::OUTPUT_L3);
  auto* param = bcm_action->add_params();
  param->set_type(BcmAction::Param::EGRESS_INTF_ID);
  param->mutable_value()->set_u32(kEgressIntfId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmDirectPortNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Set up the normal/working parameters & expectations.
  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    switch (pair.first.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
      case P4_FIELD_TYPE_ETH_DST:
        *source.mutable_action()->mutable_function()->add_modify_fields() =
            pair.first;
        *expected.add_actions() = pair.second;
        break;
      default:
        break;
    }
  }
  auto* p4_egress_field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  p4_egress_field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  p4_egress_field->set_u32(kPortId1);
  auto* bcm_egress_action = expected.add_actions();
  bcm_egress_action->set_type(BcmAction::OUTPUT_PORT);
  auto* bcm_egress_param = bcm_egress_action->add_params();
  bcm_egress_param->set_type(BcmAction::Param::LOGICAL_PORT);
  bcm_egress_param->mutable_value()->set_u32(kLogicalPort1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // No parameter may have a value of 0.
  for (auto& field :
       *source.mutable_action()->mutable_function()->mutable_modify_fields()) {
    auto original_field = field;
    field.clear_u32();
    field.clear_u64();
    BcmFlowEntry actual;
    SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
    EXPECT_FALSE(bcm_table_manager_
                     ->CommonFlowEntryToBcmFlowEntry(
                         source, ::p4::v1::Update::INSERT, &actual)
                     .ok());
    field = original_field;
  }

  // The egress port may not be the CPU port.
  {
    p4_egress_field->clear_u32();
    p4_egress_field->set_u64(kCpuPortId);
    BcmFlowEntry actual;
    SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));
    EXPECT_FALSE(bcm_table_manager_
                     ->CommonFlowEntryToBcmFlowEntry(
                         source, ::p4::v1::Update::INSERT, &actual)
                     .ok());
  }
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmDirectCpuNexthop) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_IPV6_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
      *expected.add_fields() = pair.second;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    switch (pair.first.type()) {
      case P4_FIELD_TYPE_ETH_SRC:
      case P4_FIELD_TYPE_ETH_DST:
        *source.mutable_action()->mutable_function()->add_modify_fields() =
            pair.first;
        *expected.add_actions() = pair.second;
        break;
      default:
        break;
    }
  }
  auto* p4_egress_field =
      source.mutable_action()->mutable_function()->add_modify_fields();
  // CPU port as direct nexthop action will result in parse failures.
  p4_egress_field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  p4_egress_field->set_u32(kCpuPortId);
  auto* bcm_egress_action = expected.add_actions();
  bcm_egress_action->set_type(BcmAction::DROP);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));

  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("A P4_FIELD_TYPE_EGRESS_PORT to CPU or a "
                        "P4_ACTION_OP_CLONE action was requested but no "
                        "P4_FIELD_TYPE_CPU_QUEUE_ID action was provided"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmMemberNexthop_NotFound) {
  CommonFlowEntry source;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
  source.mutable_action()->set_profile_member_id(kMemberId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));

  // Member is not found.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unknown member_id"));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmMemberNexthop_BadType) {
  CommonFlowEntry source;

  // We first need to add one member before.
  ::p4::v1::ActionProfileMember member;

  member.set_member_id(kMemberId1);
  member.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member, BcmNonMultipathNexthop::NEXTHOP_TYPE_UNKNOWN, kEgressIntfId1,
      kLogicalPort1));

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_MEMBER_ID);
  source.mutable_action()->set_profile_member_id(kMemberId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));

  // Bad member type.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid or unsupported nexthop type"));
}

TEST_F(
    BcmTableManagerTest,
    CommonFlowEntryToBcmFlowEntry_Insert_InvalidLpmGroupNexthop_GroupNotFound) {
  CommonFlowEntry source;

  source.mutable_table_info()->set_id(kTableId1);
  source.mutable_table_info()->set_type(P4_TABLE_L3_IP);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  // Setup fields.
  for (const auto& pair : P4ToBcmFields()) {
    // Skip IPv4 fields.
    if (P4FieldType_Name(pair.first.type()).find("IPV4") == string::npos) {
      *source.add_fields() = pair.first;
    }
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup actions.
  source.mutable_action()->set_type(P4_ACTION_TYPE_PROFILE_GROUP_ID);
  source.mutable_action()->set_profile_member_id(kGroupId1);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  SCOPED_TRACE(absl::StrCat("CommonFlowEntry:\n", source.DebugString()));

  // Group is not found.
  ::util::Status status = bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual);
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unknown group_id"));
}

// Verify the ACL translations for CommonFlowEntryToBcmEntry.
TEST_F(BcmTableManagerTest, CommonFlowEntryToBcmFlowEntryAclSuccess) {
  AclTable acl_table = CreateAclTable(/*p4_id=*/88, /*match_fields=*/{},
                                      /*stage=*/BCM_ACL_STAGE_EFP, /*size=*/10,
                                      /*priority=*/20);

  CommonFlowEntry source;
  BcmFlowEntry expected;
  source.mutable_table_info()->set_id(88);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::INGRESS_ACL);
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Set up table type.
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_ACL);
  expected.set_acl_stage(BCM_ACL_STAGE_EFP);

  // Set up action.
  source.mutable_action()->set_type(P4ActionType::P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    *source.mutable_action()->mutable_function()->add_modify_fields() =
        pair.first;
    *expected.add_actions() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Set up priority.
  source.set_priority(2000);
  expected.set_priority(2000 + (20 << 16));

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));
  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidAclPriority) {
  AclTable acl_table = CreateAclTable(/*p4_id=*/88, /*match_fields=*/{},
                                      /*stage=*/BCM_ACL_STAGE_EFP, /*size=*/10,
                                      /*priority=*/20);
  CommonFlowEntry source;
  source.mutable_table_info()->set_id(88);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::INGRESS_ACL);
  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Set up action.
  source.mutable_action()->set_type(P4ActionType::P4_ACTION_TYPE_FUNCTION);
  for (const auto& pair : P4ToBcmActions()) {
    *source.mutable_action()->mutable_function()->add_modify_fields() =
        pair.first;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Set up priority. This priority is too high and eats into the table priority
  // range.
  source.set_priority(20 << 16);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));
  BcmFlowEntry actual;
  EXPECT_FALSE(bcm_table_manager_
                   ->CommonFlowEntryToBcmFlowEntry(
                       source, ::p4::v1::Update::INSERT, &actual)
                   .ok());

  // Set up priority. This priority is too low and won't translate well.
  source.set_priority(-1);
  EXPECT_FALSE(bcm_table_manager_
                   ->CommonFlowEntryToBcmFlowEntry(
                       source, ::p4::v1::Update::INSERT, &actual)
                   .ok());
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidSendToCpuAction) {
  uint64 cpu_queue = 100;
  AclTable acl_table = CreateAclTable(/*p4_id=*/88, /*match_fields=*/{},
                                      /*stage=*/BCM_ACL_STAGE_EFP, /*size=*/10);

  // Setup the preconditions.
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  CommonFlowEntry p4_entry_template;
  p4_entry_template.mutable_table_info()->set_id(acl_table.Id());
  p4_entry_template.mutable_table_info()->set_pipeline_stage(
      P4Annotation::INGRESS_ACL);

  BcmFlowEntry bcm_entry_template;
  bcm_entry_template.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_ACL);
  bcm_entry_template.set_bcm_acl_table_id(acl_table.PhysicalTableId());
  bcm_entry_template.set_acl_stage(acl_table.Stage());

  for (const ColorTestCase& test_case : SendToCpuTestCases()) {
    // Set up the input P4 entry.
    CommonFlowEntry p4_entry = p4_entry_template;
    FillP4SendToCpuAction(cpu_queue, test_case.input, &p4_entry);
    SCOPED_TRACE(absl::StrCat("Failed to convert CommonFlowEntry:\n",
                              p4_entry.DebugString()));

    // Set up the expected Bcm action.
    bool valid_output = true;
    if (test_case.output.copy.colors.empty() &&
        test_case.output.drop.colors.empty()) {
      valid_output = false;
    }
    BcmFlowEntry expected_entry = bcm_entry_template;
    FillBcmCopyToCpuAction(cpu_queue, test_case.output, &expected_entry);

    BcmFlowEntry converted_entry;
    if (valid_output) {
      ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
          p4_entry, ::p4::v1::Update::INSERT, &converted_entry));
      EXPECT_THAT(converted_entry, UnorderedEqualsProto(expected_entry));
    } else {
      EXPECT_FALSE(bcm_table_manager_
                       ->CommonFlowEntryToBcmFlowEntry(
                           p4_entry, ::p4::v1::Update::INSERT, &converted_entry)
                       .ok());
    }
  }
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidCopyToCpuAction) {
  uint64 cpu_queue = 100;
  AclTable acl_table = CreateAclTable(/*p4_id=*/88, /*match_fields=*/{},
                                      /*stage=*/BCM_ACL_STAGE_EFP, /*size=*/10);
  // Setup the preconditions.
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  CommonFlowEntry p4_entry_template;
  p4_entry_template.mutable_table_info()->set_id(acl_table.Id());
  p4_entry_template.mutable_table_info()->set_pipeline_stage(
      P4Annotation::INGRESS_ACL);

  BcmFlowEntry bcm_entry_template;
  bcm_entry_template.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_ACL);
  bcm_entry_template.set_bcm_acl_table_id(acl_table.PhysicalTableId());
  bcm_entry_template.set_acl_stage(acl_table.Stage());

  for (const ColorTestCase& test_case : CopyToCpuTestCases()) {
    // Set up the input P4 entry.
    CommonFlowEntry p4_entry = p4_entry_template;
    FillP4CopyToCpuAction(cpu_queue, test_case.input, &p4_entry);
    SCOPED_TRACE(absl::StrCat("Failed to convert CommonFlowEntry:\n",
                              p4_entry.DebugString()));

    // Set up the expected Bcm action.
    bool valid_output = true;
    if (test_case.output.copy.colors.empty() &&
        test_case.output.drop.colors.empty()) {
      valid_output = false;
    }
    BcmFlowEntry expected_entry = bcm_entry_template;
    FillBcmCopyToCpuAction(cpu_queue, test_case.output, &expected_entry);

    BcmFlowEntry converted_entry;
    if (valid_output) {
      ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
          p4_entry, ::p4::v1::Update::INSERT, &converted_entry));
      EXPECT_THAT(converted_entry, UnorderedEqualsProto(expected_entry));
    } else {
      EXPECT_FALSE(bcm_table_manager_
                       ->CommonFlowEntryToBcmFlowEntry(
                           p4_entry, ::p4::v1::Update::INSERT, &converted_entry)
                       .ok());
    }
  }
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InvalidCopyOrSendToCpuAction) {
  ::p4::config::v1::Table p4_acl_table;
  AclTable acl_table = CreateAclTable(/*p4_id=*/88, /*match_fields=*/{},
                                      /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10);
  // Setup the preconditions.
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  CommonFlowEntry p4_entry_template;
  p4_entry_template.mutable_table_info()->set_id(acl_table.Id());
  p4_entry_template.mutable_table_info()->set_pipeline_stage(
      P4Annotation::INGRESS_ACL);
  p4_entry_template.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  // Copy-to-CPU actions without a CPU Queue ID should fail.
  {
    SCOPED_TRACE(
        "Expected failure when missing CPU queue ID in a copy-to-cpu action.");
    CommonFlowEntry p4_entry = p4_entry_template;
    p4_entry.mutable_action()
        ->mutable_function()
        ->add_primitives()
        ->set_op_code(P4_ACTION_OP_CLONE);
    BcmFlowEntry bcm_entry;
    EXPECT_FALSE(bcm_table_manager_
                     ->CommonFlowEntryToBcmFlowEntry(
                         p4_entry, ::p4::v1::Update::INSERT, &bcm_entry)
                     .ok())
        << "CommonFlowEntry: " << p4_entry.DebugString();
  }

  // Send-to-CPU actions without a CPU Queue ID should fail.
  {
    SCOPED_TRACE(
        "Expected failure when missing CPU queue ID in a send-to-cpu action.");
    CommonFlowEntry p4_entry = p4_entry_template;
    P4ActionFunction::P4ActionFields* field =
        p4_entry.mutable_action()->mutable_function()->add_modify_fields();
    field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
    field->set_u64(kCpuPortId);
    BcmFlowEntry bcm_entry;
    EXPECT_FALSE(bcm_table_manager_
                     ->CommonFlowEntryToBcmFlowEntry(
                         p4_entry, ::p4::v1::Update::INSERT, &bcm_entry)
                     .ok())
        << "CommonFlowEntry: " << p4_entry.DebugString();
  }

  // Actions with both send-to-cpu & copy-to-cpu should fail.
  P4ActionFunction::P4ActionFields* template_field =
      p4_entry_template.mutable_action()
          ->mutable_function()
          ->add_modify_fields();
  template_field->set_type(P4_FIELD_TYPE_CPU_QUEUE_ID);
  template_field->set_u32(100);
  {
    SCOPED_TRACE(
        "Expected failure when specifying both send-to-cpu & copy-to-cpu "
        "actions.");
    CommonFlowEntry p4_entry = p4_entry_template;
    p4_entry.mutable_action()
        ->mutable_function()
        ->add_primitives()
        ->set_op_code(P4_ACTION_OP_CLONE);
    P4ActionFunction::P4ActionFields* field =
        p4_entry.mutable_action()->mutable_function()->add_modify_fields();
    field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
    field->set_u64(kCpuPortId);
    BcmFlowEntry bcm_entry;
    EXPECT_FALSE(bcm_table_manager_
                     ->CommonFlowEntryToBcmFlowEntry(
                         p4_entry, ::p4::v1::Update::INSERT, &bcm_entry)
                     .ok())
        << "CommonFlowEntry: " << p4_entry.DebugString();
  }
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidDecap) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  for (const auto& pair : P4ToBcmFields()) {
    *source.add_fields() = pair.first;
    *expected.add_fields() = pair.second;
  }
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table stage; decap has no explicit P4 table type.
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::DECAP);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_TUNNEL);

  // Setup empty action for source.
  // TODO(teverman) : Add any special decap action needs, such as P4TunnelType.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  // Set priorities.
  source.set_priority(2);
  expected.set_priority(2);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_ValidPortFields) {
  CommonFlowEntry source;
  BcmFlowEntry expected;

  // Set up a field for each port type.
  source.add_fields()->set_type(P4_FIELD_TYPE_INGRESS_PORT);
  source.mutable_fields(0)->mutable_value()->set_u32(kPortId1);
  source.mutable_fields(0)->mutable_mask()->set_u32(511);
  source.add_fields()->set_type(P4_FIELD_TYPE_CLONE_PORT);
  source.mutable_fields(1)->mutable_value()->set_u32(kPortId2);
  source.mutable_fields(1)->mutable_mask()->set_u32(511);
  source.add_fields()->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  source.mutable_fields(2)->mutable_value()->set_u32(kTrunkId1);
  source.mutable_fields(2)->mutable_mask()->set_u32(511);
  source.add_fields()->set_type(P4_FIELD_TYPE_INGRESS_PORT);
  source.mutable_fields(3)->mutable_value()->set_u32(kCpuPortId);
  source.mutable_fields(3)->mutable_mask()->set_u32(511);

  expected.add_fields()->set_type(BcmField::IN_PORT);
  expected.mutable_fields(0)->mutable_value()->set_u32(kLogicalPort1);
  expected.mutable_fields(0)->mutable_mask()->set_u32(0xFFFFFFFF);
  expected.add_fields()->set_type(BcmField::CLONE_PORT);
  expected.mutable_fields(1)->mutable_value()->set_u32(kLogicalPort2);
  expected.mutable_fields(1)->mutable_mask()->set_u32(0xFFFFFFFF);
  expected.add_fields()->set_type(BcmField::OUT_PORT);
  expected.mutable_fields(2)->mutable_value()->set_u32(kTrunkPort1);
  expected.mutable_fields(2)->mutable_mask()->set_u32(0xFFFFFFFF);
  expected.add_fields()->set_type(BcmField::IN_PORT);
  expected.mutable_fields(3)->mutable_value()->set_u32(kCpuLogicalPort);
  expected.mutable_fields(3)->mutable_mask()->set_u32(0xFFFFFFFF);

  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);
  expected.set_bcm_table_type(BcmFlowEntry::BCM_TABLE_MY_STATION);

  source.set_priority(10);
  expected.set_priority(10);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, EqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_Insert_InValidPortFields) {
  CommonFlowEntry source;
  // Set up a field for each port type.
  source.add_fields()->set_type(P4_FIELD_TYPE_INGRESS_PORT);
  source.mutable_fields(0)->mutable_value()->set_u32(kPortId3);
  source.mutable_fields(0)->mutable_mask()->set_u32(511);
  ASSERT_FALSE(HasFailure());  // Stop if P4ToBcmFields failed.

  // Setup table type and stage.
  source.mutable_table_info()->set_type(P4_TABLE_L2_MY_STATION);
  source.mutable_table_info()->set_pipeline_stage(P4Annotation::L3_LPM);

  source.set_priority(10);

  // Setup empty action.
  source.mutable_action()->set_type(P4_ACTION_TYPE_FUNCTION);

  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  BcmFlowEntry actual;
  EXPECT_THAT(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
                  source, ::p4::v1::Update::INSERT, &actual),
              StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                       HasSubstr(absl::StrCat(kPortId3))));
}

TEST_F(BcmTableManagerTest, FillBcmNonMultipathNexthopSuccessForCpuPort) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  MappedAction mapped_action;
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kCpuPortId);
  BcmNonMultipathNexthop expected_nexthop;
  expected_nexthop.set_unit(kUnit);
  expected_nexthop.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
  expected_nexthop.set_logical_port(kCpuPort);

  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));

  BcmNonMultipathNexthop returned_nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmNonMultipathNexthop(member,
                                                           &returned_nexthop));
  EXPECT_TRUE(ProtoEqual(expected_nexthop, returned_nexthop))
      << "Expected {" << expected_nexthop.ShortDebugString() << "}, got {"
      << returned_nexthop.ShortDebugString() << "}.";
}

TEST_F(BcmTableManagerTest, FillBcmNonMultipathNexthopSuccessForRegularPort) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  MappedAction mapped_action;
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_SRC);
  field->set_u64(kSrcMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_DST);
  field->set_u64(kDstMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kPortId1);
  BcmNonMultipathNexthop expected_nexthop;
  expected_nexthop.set_unit(kUnit);
  expected_nexthop.set_src_mac(kSrcMac1);
  expected_nexthop.set_dst_mac(kDstMac1);
  expected_nexthop.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
  expected_nexthop.set_logical_port(kLogicalPort1);

  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));

  BcmNonMultipathNexthop returned_nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmNonMultipathNexthop(member,
                                                           &returned_nexthop));
  EXPECT_TRUE(ProtoEqual(expected_nexthop, returned_nexthop))
      << "Expected {" << expected_nexthop.ShortDebugString() << "}, got {"
      << returned_nexthop.ShortDebugString() << "}.";
}

TEST_F(BcmTableManagerTest,
       FillBcmNonMultipathNexthopSuccessForRegularPortAndClassId) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  MappedAction mapped_action;
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_SRC);
  field->set_u64(kSrcMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_DST);
  field->set_u64(kDstMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kPortId1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_L3_CLASS_ID);
  field->set_u32(kClassId1);
  BcmNonMultipathNexthop expected_nexthop;
  expected_nexthop.set_unit(kUnit);
  expected_nexthop.set_src_mac(kSrcMac1);
  expected_nexthop.set_dst_mac(kDstMac1);
  expected_nexthop.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT);
  expected_nexthop.set_logical_port(kLogicalPort1);

  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));

  BcmNonMultipathNexthop returned_nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmNonMultipathNexthop(member,
                                                           &returned_nexthop));
  EXPECT_TRUE(ProtoEqual(expected_nexthop, returned_nexthop))
      << "Expected {" << expected_nexthop.ShortDebugString() << "}, got {"
      << returned_nexthop.ShortDebugString() << "}.";
}

TEST_F(BcmTableManagerTest, FillBcmNonMultipathNexthopSuccessForTrunk) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  MappedAction mapped_action;
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_SRC);
  field->set_u64(kSrcMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_DST);
  field->set_u64(kDstMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kTrunkId1);
  BcmNonMultipathNexthop expected_nexthop;
  expected_nexthop.set_unit(kUnit);
  expected_nexthop.set_src_mac(kSrcMac1);
  expected_nexthop.set_dst_mac(kDstMac1);
  expected_nexthop.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK);
  expected_nexthop.set_trunk_port(kTrunkPort1);

  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));

  BcmNonMultipathNexthop returned_nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmNonMultipathNexthop(member,
                                                           &returned_nexthop));
  EXPECT_TRUE(ProtoEqual(expected_nexthop, returned_nexthop))
      << "Expected {" << expected_nexthop.ShortDebugString() << "}, got {"
      << returned_nexthop.ShortDebugString() << "}.";
}

TEST_F(BcmTableManagerTest, FillBcmNonMultipathNexthopSuccessForDrop) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  MappedAction mapped_action;
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* primitive = function->add_primitives();
  primitive->set_op_code(P4_ACTION_OP_DROP);
  BcmNonMultipathNexthop expected_nexthop;
  expected_nexthop.set_unit(kUnit);
  expected_nexthop.set_logical_port(kCpuPort);
  expected_nexthop.set_type(BcmNonMultipathNexthop::NEXTHOP_TYPE_DROP);

  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));

  BcmNonMultipathNexthop returned_nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmNonMultipathNexthop(member,
                                                           &returned_nexthop));
  EXPECT_TRUE(ProtoEqual(expected_nexthop, returned_nexthop))
      << "Expected {" << expected_nexthop.ShortDebugString() << "}, got {"
      << returned_nexthop.ShortDebugString() << "}.";
}

TEST_F(BcmTableManagerTest, FillBcmNonMultipathNexthopFailure) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member;
  BcmNonMultipathNexthop nexthop;

  // Should fail if the action profile member cannot be translated.
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(Return(
          ::util::Status(StratumErrorSpace(), ERR_HARDWARE_ERROR, "Blah")));
  auto status =
      bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_HARDWARE_ERROR, status.error_code());
  EXPECT_THAT(status.error_message(), HasSubstr("Blah"));

  // Should fail if mapped action type is not P4_ACTION_TYPE_FUNCTION.
  MappedAction mapped_action;
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));
  status = bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid or unsupported P4 mapped action type"));

  // Should fail if mapped action has any primitives and the primitive is of
  // type P4_ACTION_OP_DROP.
  mapped_action.set_type(P4_ACTION_TYPE_FUNCTION);
  auto* function = mapped_action.mutable_function();
  auto* field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_SRC);
  field->set_u64(kSrcMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_ETH_DST);
  field->set_u64(kDstMac1);
  field = function->add_modify_fields();
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kPortId1);
  function->add_primitives();
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));
  status = bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid action premitives, found in"));

  // Should fail if port cannot be mapped.
  function->clear_primitives();
  field = function->mutable_modify_fields(2);
  field->set_u32(1234);
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));
  status = bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Could not find logical port or trunk port for port"));

  // Should fail if a field is not src/dst mac or egress port.
  field->set_type(P4_FIELD_TYPE_UNKNOWN);
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));
  status = bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Invalid or unsupported P4 field type"));

  // Should fail if any field is not given (or zero).
  field->set_type(P4_FIELD_TYPE_EGRESS_PORT);
  field->set_u32(kPortId1);
  field = function->mutable_modify_fields(0);
  field->set_u64(0);
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileMember(EqualsProto(member), _))
      .WillOnce(
          DoAll(SetArgPointee<1>(mapped_action), Return(::util::OkStatus())));
  status = bcm_table_manager_->FillBcmNonMultipathNexthop(member, &nexthop);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Detected invalid port nexthop"));
}

MATCHER_P(SdkPortEq, sdk_port, "") {
  return sdk_port.unit == arg.unit && sdk_port.logical_port == arg.logical_port;
}

TEST_F(BcmTableManagerTest, FillBcmMultipathNexthopSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up P4 members and group.
  ::p4::v1::ActionProfileMember member1, member2, member3;
  ::p4::v1::ActionProfileGroup group1;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  member3.set_member_id(kMemberId3);
  member3.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.mutable_members(0)->set_weight(1);
  group1.add_members()->set_member_id(kMemberId2);
  group1.mutable_members(1)->set_weight(2);
  group1.add_members()->set_member_id(kMemberId3);
  group1.mutable_members(2)->set_weight(3);

  // Add P4 members.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member3, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId3,
      kLogicalPort2));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(member3,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId3, kLogicalPort2, 0, 0));

  // Set up expectations for FillBcmMultipathNexthop.
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileGroup(EqualsProto(group1), _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort1))))
      .WillOnce(Return(PORT_STATE_UP));
  // This member should not be included in the created group.
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort2))))
      .WillOnce(Return(PORT_STATE_DOWN));

  // Make call and check created BcmMultipathNexthop.
  BcmMultipathNexthop nexthop;
  EXPECT_OK(bcm_table_manager_->FillBcmMultipathNexthop(group1, &nexthop));

  EXPECT_EQ(kUnit, nexthop.unit());
  ASSERT_EQ(2, nexthop.members_size());
  EXPECT_EQ(kEgressIntfId1, nexthop.members(0).egress_intf_id());
  EXPECT_EQ(1, nexthop.members(0).weight());
  EXPECT_EQ(kEgressIntfId2, nexthop.members(1).egress_intf_id());
  EXPECT_EQ(2, nexthop.members(1).weight());
}

TEST_F(BcmTableManagerTest, FillBcmMultipathNexthopFailure) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Setup members and group.
  ::p4::v1::ActionProfileMember member1, member2;
  ::p4::v1::ActionProfileGroup group1;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.mutable_members(0)->set_weight(3);
  group1.add_members()->set_member_id(kMemberId2);
  group1.mutable_members(1)->set_weight(1);

  // Add P4 members.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId2,
      kLogicalPort2));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(member2,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId2, kLogicalPort2, 0, 0));

  // Set up expectations. Each call will fail due to a different failure during
  // the execution of FillBcmMultipathNexthop().
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileGroup(EqualsProto(group1), _))
      .WillOnce(Return(::util::UnknownErrorBuilder(GTL_LOC) << "error1"))
      .WillRepeatedly(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort1))))
      .WillOnce(Return(::util::UnknownErrorBuilder(GTL_LOC) << "error2"));
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort2))))
      .Times(0);

  BcmMultipathNexthop nexthop;
  auto status = bcm_table_manager_->FillBcmMultipathNexthop(group1, &nexthop);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_UNKNOWN, status.error_code());
  EXPECT_EQ("error1", status.error_message());
  status = bcm_table_manager_->FillBcmMultipathNexthop(group1, &nexthop);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ERR_UNKNOWN, status.error_code());
  EXPECT_EQ("error2", status.error_message());
}

TEST_F(BcmTableManagerTest, FillBcmMultipathNexthopsWithPortSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up P4 members and groups, with one member, shared by 2 groups, pointing
  // to the same output port.
  ::p4::v1::ActionProfileMember member1, member2, member3;
  ::p4::v1::ActionProfileGroup group1, group2, group3;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);
  member3.set_member_id(kMemberId3);
  member3.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.add_members()->set_member_id(kMemberId2);
  group2.set_group_id(kGroupId2);
  group2.set_action_profile_id(kActionProfileId1);
  group2.add_members()->set_member_id(kMemberId1);
  group2.add_members()->set_member_id(kMemberId3);
  group3.set_group_id(kGroupId3);
  group3.set_action_profile_id(kActionProfileId1);
  group3.add_members()->set_member_id(kMemberId2);
  group3.add_members()->set_member_id(kMemberId3);

  // Add and verify the members and groups.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member3, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId3,
      kLogicalPort2));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group2, kEgressIntfId5));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group3, kEgressIntfId6));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 2, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 2, 0));
  ASSERT_OK(VerifyActionProfileMember(member3,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId3, kLogicalPort2, 2, 0));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 2, kLogicalPort1)},
       {kMemberId2, std::make_tuple(1, 2, kTrunkPort1)}}));
  ASSERT_OK(VerifyActionProfileGroup(
      group2, kEgressIntfId5, 0,
      {{kMemberId1, std::make_tuple(1, 2, kLogicalPort1)},
       {kMemberId3, std::make_tuple(1, 2, kLogicalPort2)}}));
  ASSERT_OK(VerifyActionProfileGroup(
      group3, kEgressIntfId6, 0,
      {{kMemberId2, std::make_tuple(1, 2, kTrunkPort1)},
       {kMemberId3, std::make_tuple(1, 2, kLogicalPort2)}}));

  // Set up expectations for the FillBcmMultipathNexthop() calls. This should
  // only be called for group1 and group2 which share kLogicalPort1.
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileGroup(EqualsProto(group1), _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*p4_table_mapper_mock_,
              MapActionProfileGroup(EqualsProto(group2), _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort1))))
      .Times(2)
      .WillRepeatedly(Return(PORT_STATE_UP));
  EXPECT_CALL(*bcm_chassis_ro_mock_,
              GetPortState(SdkPortEq(SdkPort(kUnit, kLogicalPort2))))
      .WillOnce(Return(PORT_STATE_UP));

  auto status_or_nexthops =
      bcm_table_manager_->FillBcmMultipathNexthopsWithPort(kPortId1);
  ASSERT_TRUE(status_or_nexthops.ok());
  auto nexthops = std::move(status_or_nexthops).ValueOrDie();

  // Check that the nexthop groups are filled as expected.
  ASSERT_EQ(2, nexthops.size());
  bool nexthop1_ok = false, nexthop2_ok = false;
  for (auto& e : nexthops) {
    int egress_intf_id = e.first;
    BcmMultipathNexthop& nexthop = e.second;
    EXPECT_EQ(kUnit, nexthop.unit());
    ASSERT_EQ(2, nexthop.members_size());
    if (egress_intf_id == kEgressIntfId4) {
      EXPECT_EQ(kEgressIntfId1, nexthop.members(0).egress_intf_id());
      EXPECT_EQ(1, nexthop.members(0).weight());
      EXPECT_EQ(kEgressIntfId2, nexthop.members(1).egress_intf_id());
      EXPECT_EQ(1, nexthop.members(1).weight());
      nexthop1_ok = true;
    } else if (egress_intf_id == kEgressIntfId5) {
      EXPECT_EQ(kEgressIntfId1, nexthop.members(0).egress_intf_id());
      EXPECT_EQ(1, nexthop.members(0).weight());
      EXPECT_EQ(kEgressIntfId3, nexthop.members(1).egress_intf_id());
      EXPECT_EQ(1, nexthop.members(1).weight());
      nexthop2_ok = true;
    }
  }
  EXPECT_TRUE(nexthop1_ok);
  EXPECT_TRUE(nexthop2_ok);
}

TEST_F(BcmTableManagerTest, FillBcmMultipathNexthopsWithPortFailure) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Failure due to unknown port.
  auto status_or_nexthops =
      bcm_table_manager_->FillBcmMultipathNexthopsWithPort(10493232);
  EXPECT_FALSE(status_or_nexthops.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, status_or_nexthops.status().error_code());
  // No groups reference the port. Empty map should be returned.
  status_or_nexthops =
      bcm_table_manager_->FillBcmMultipathNexthopsWithPort(kPortId1);
  EXPECT_TRUE(status_or_nexthops.ok());
  EXPECT_TRUE(status_or_nexthops.ValueOrDie().empty());
}

TEST_F(BcmTableManagerTest, AddTableEntrySuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::ActionProfileGroup group1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);  // one member in group1

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_group_id(kGroupId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));

  // Now add the table entries.
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry1));
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry2));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, true));
  ASSERT_OK(VerifyTableEntry(entry2, true, true, true));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 1));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 1,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));
}

TEST_F(BcmTableManagerTest, AddTableEntryFailureWhenNoTableIdInEntry) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::TableEntry entry1;

  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Now add the table entry without adding the member.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_FALSE(status.ok());
}

TEST_F(BcmTableManagerTest, AddTableEntryFailureWhenTableEntryExists) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::TableEntry entry1;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Now add the table entry two times.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_OK(status);
  status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), ERR_ENTRY_EXISTS);
}

TEST_F(BcmTableManagerTest, AddTableEntryFailureWhenReferencedMemberNotFound) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::TableEntry entry1;

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Now add the table entry without adding the member.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(), HasSubstr("Unknown member_id"));
}

TEST_F(BcmTableManagerTest, UpdateTableEntrySuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2;
  ::p4::v1::ActionProfileGroup group1;
  ::p4::v1::TableEntry entry1, entry2, entry3;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.add_members()->set_member_id(kMemberId2);  // two members in group1

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_group_id(kGroupId1);
  entry3.set_table_id(kTableId1);               // same as entry1
  entry3.add_match()->set_field_id(kFieldId1);  // same as entry1
  entry3.mutable_action()->set_action_profile_member_id(kMemberId2);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)},
       {kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));

  // Now add and update the table entries.
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry1));
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry2));
  ASSERT_OK(bcm_table_manager_->UpdateTableEntry(entry3));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, false));
  ASSERT_OK(VerifyTableEntry(entry2, true, true, true));
  // entry3 replaces entry1
  ASSERT_OK(VerifyTableEntry(entry3, true, true, true));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 1, 1));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 1,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)},
       {kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));
}

TEST_F(BcmTableManagerTest, UpdateTableEntryFailureWhenNodeNotFound) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::TableEntry entry1;

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Update the table entry when there is no refrence of the node.
  ASSERT_FALSE(bcm_table_manager_->UpdateTableEntry(entry1).ok());
}

TEST_F(BcmTableManagerTest, UpdateTableEntryFailureWhenTableNotFound) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId1);
  entry2.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Now add entry1 and update entry2 which points to a non existing table.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_OK(status);
  status = bcm_table_manager_->UpdateTableEntry(entry2);
  ASSERT_FALSE(status.ok());
}

TEST_F(BcmTableManagerTest, UpdateTableEntryFailureWhenEntryNotFoundInTable) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId1);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_member_id(kMemberId2);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Now add entry1 and update entry2 which points to the same table but the
  // entry does not exist in the table.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_OK(status);
  status = bcm_table_manager_->UpdateTableEntry(entry2);
  ASSERT_FALSE(status.ok());
  EXPECT_EQ(status.error_code(), ERR_ENTRY_NOT_FOUND);
}

TEST_F(BcmTableManagerTest, DeleteTableEntrySuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::ActionProfileGroup group1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);  // one member in group1

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_group_id(kGroupId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));

  // Now add the table entries and then remove them one by one.
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry1));
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry2));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, true));
  ASSERT_OK(VerifyTableEntry(entry2, true, true, true));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 1));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 1,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));

  ASSERT_OK(bcm_table_manager_->DeleteTableEntry(entry2));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, true));
  ASSERT_OK(VerifyTableEntry(entry2, false, false, false));

  ASSERT_OK(bcm_table_manager_->DeleteTableEntry(entry1));

  ASSERT_OK(VerifyTableEntry(entry1, false, false, false));
  ASSERT_OK(VerifyTableEntry(entry2, false, false, false));
  ASSERT_OK(VerifyActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1, 1, 0));  // flow_ref_count back to 0
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1,
        std::make_tuple(1, 1, kLogicalPort1)}}));  // flow_ref_count back to 0
}

TEST_F(BcmTableManagerTest, DeleteTableEntryFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, DeleteTableEntryFailureWhenNodeNotFound) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::TableEntry entry1;

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Delete the table entry when there is no refrence of the node.
  ::util::Status status = bcm_table_manager_->DeleteTableEntry(entry1);
  ASSERT_EQ(status.error_code(), ERR_ENTRY_NOT_FOUND);
}

TEST_F(BcmTableManagerTest, DeleteTableEntryFailureWhenTableNotFound) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId1);
  entry2.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Now add entry1 and delete entry2 which points to a non existing table.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_OK(status);
  status = bcm_table_manager_->DeleteTableEntry(entry2);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), ERR_ENTRY_NOT_FOUND);
}

TEST_F(BcmTableManagerTest, DeleteTableEntryFailureWhenEntryNotFoundInTable) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId1);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_member_id(kMemberId2);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Now add entry1 and delete entry2 which points to the same table but the
  // entry does not exist in the table.
  ::util::Status status = bcm_table_manager_->AddTableEntry(entry1);
  ASSERT_OK(status);
  status = bcm_table_manager_->DeleteTableEntry(entry2);
  ASSERT_FALSE(status.ok());
  ASSERT_EQ(status.error_code(), ERR_ENTRY_NOT_FOUND);
}

TEST_F(BcmTableManagerTest, AddAndDeleteAclTable) {
  PushTestConfig();

  // Need to first add the members and groups the flow will point to.
  ::p4::v1::ActionProfileMember member1;
  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Add an ACL table with a single entry.
  AclTable table =
      CreateAclTable(/*p4_id=*/kTableId1, /*match_fields=*/{kFieldId1},
                     /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
                     /*priority=*/20);
  EXPECT_OK(bcm_table_manager_->AddAclTable(table));

  // Add an entry.
  ::p4::v1::TableEntry entry;
  entry.set_table_id(kTableId1);
  entry.add_match()->set_field_id(kFieldId1);
  entry.mutable_action()->set_action_profile_member_id(kMemberId1);
  EXPECT_OK(bcm_table_manager_->AddAclTableEntry(entry, 15));

  // Sanity check the table contents.
  const AclTable* read_only_table;
  ASSERT_OK_AND_ASSIGN(read_only_table,
                       bcm_table_manager_->GetReadOnlyAclTable(table.Id()));
  EXPECT_EQ(read_only_table->Id(), kTableId1);
  EXPECT_EQ(read_only_table->Size(), 10);
  EXPECT_EQ(read_only_table->EntryCount(), 1);
  EXPECT_TRUE(read_only_table->HasEntry(entry));
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(),
              UnorderedElementsAre(kTableId1));

  // Delete the table.
  EXPECT_OK(bcm_table_manager_->DeleteTable(kTableId1));
  EXPECT_THAT(bcm_table_manager_->GetAllAclTableIDs(), UnorderedElementsAre());
  EXPECT_THAT(bcm_table_manager_->GetReadOnlyAclTable(kTableId1),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
}

TEST_F(BcmTableManagerTest, GetReadOnlyAclTable) {
  PushTestConfig();
  // Need to first add the members and groups the flow will point to.
  ::p4::v1::ActionProfileMember member1;
  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Add an ACL table with a single entry.
  AclTable table =
      CreateAclTable(/*p4_id=*/kTableId1, /*match_fields=*/{kFieldId1},
                     /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
                     /*priority=*/20);
  EXPECT_OK(bcm_table_manager_->AddAclTable(table));

  // Add a non-ACL table.
  ::p4::v1::TableEntry entry;
  entry.set_table_id(kTableId2);
  entry.add_match()->set_field_id(kFieldId1);
  entry.mutable_action()->set_action_profile_member_id(kMemberId1);
  EXPECT_OK(bcm_table_manager_->AddTableEntry(entry));

  const AclTable* read_only_table;
  // Test ACL table retrieval.
  ASSERT_OK_AND_ASSIGN(read_only_table,
                       bcm_table_manager_->GetReadOnlyAclTable(kTableId1));
  EXPECT_EQ(read_only_table->Id(), kTableId1);
  // Test non-ACL table failure.
  EXPECT_THAT(bcm_table_manager_->GetReadOnlyAclTable(kTableId2),
              StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM, _));
  // Test unknown table failure.
  EXPECT_THAT(bcm_table_manager_->GetReadOnlyAclTable(0),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
}

TEST_F(BcmTableManagerTest, AddAclTableEntry) {
  PushTestConfig();
  // Need to first add the members and groups the flow will point to.
  ::p4::v1::ActionProfileMember member1;
  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Add an ACL table with a single entry.
  AclTable table =
      CreateAclTable(/*p4_id=*/kTableId1, /*match_fields=*/{kFieldId1},
                     /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
                     /*priority=*/20);
  EXPECT_OK(bcm_table_manager_->AddAclTable(table));

  // Create the table entry.
  ::p4::v1::TableEntry entry;
  entry.set_table_id(kTableId1);
  entry.add_match()->set_field_id(kFieldId1);
  entry.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Add the entry.
  EXPECT_OK(bcm_table_manager_->AddAclTableEntry(entry, 11));

  // Verify the entry.
  const AclTable* read_only_table;
  ASSERT_OK_AND_ASSIGN(read_only_table,
                       bcm_table_manager_->GetReadOnlyAclTable(kTableId1));
  EXPECT_TRUE(read_only_table->HasEntry(entry));
  EXPECT_THAT(read_only_table->BcmAclId(entry), IsOkAndHolds(11));
}

TEST_F(BcmTableManagerTest, AddAclTableEntryRejection) {
  // Need to first add the members and groups the flow will point to.
  ::p4::v1::ActionProfileMember member1;
  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));

  // Create the table entry.
  ::p4::v1::TableEntry entry;
  entry.set_table_id(kTableId1);
  entry.add_match()->set_field_id(kFieldId1);
  entry.mutable_action()->set_action_profile_member_id(kMemberId1);

  // Attempt to add an entry for an unknown table.
  EXPECT_THAT(bcm_table_manager_->AddAclTableEntry(entry, 1),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
  // Attempt to add an entry into a non-ACL table.
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry));
  EXPECT_THAT(bcm_table_manager_->AddAclTableEntry(entry, 1),
              StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM, _));
}

TEST_F(BcmTableManagerTest, DeleteTableSuccess) {
  PushTestConfig();
  // Make table1 an AclTable.
  AclTable table1 =
      CreateAclTable(/*p4_id=*/kTableId1, /*match_fields=*/{kFieldId1},
                     /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
                     /*priority=*/20);
  EXPECT_OK(bcm_table_manager_->AddAclTable(table1));

  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::ActionProfileGroup group1;
  ::p4::v1::TableEntry entry1, entry2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);  // one member in group1

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);
  entry2.set_table_id(kTableId2);
  entry2.add_match()->set_field_id(kFieldId2);
  entry2.mutable_action()->set_action_profile_group_id(kGroupId1);

  // Need to first add the members and groups the flow will point to.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 0));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));

  // Now add the table entries and then remove them one by one.
  ASSERT_OK(bcm_table_manager_->AddAclTableEntry(entry1, 111));
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry2));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, true));
  ASSERT_OK(VerifyTableEntry(entry2, true, true, true));
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 1, 1));
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 1,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)}}));

  ASSERT_OK(bcm_table_manager_->DeleteTable(kTableId2));

  ASSERT_OK(VerifyTableEntry(entry1, true, true, true));
  ASSERT_OK(VerifyTableEntry(entry2, false, false, false));

  ASSERT_OK(bcm_table_manager_->DeleteTable(kTableId1));
  ASSERT_OK(VerifyTableEntry(entry1, false, false, false));
  ASSERT_OK(VerifyTableEntry(entry2, false, false, false));
  ASSERT_OK(VerifyActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1, 1, 0));  // flow_ref_count back to 0
  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1,
        std::make_tuple(1, 1, kLogicalPort1)}}));  // flow_ref_count back to 0
  // Expect that the AclTable object is deleted.
  EXPECT_THAT(bcm_table_manager_->GetReadOnlyAclTable(kTableId1),
              StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, _));
}

// DeleteTable should return a failure if the table cannot be found.
TEST_F(BcmTableManagerTest, DeleteTableFailure) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  EXPECT_THAT(
      bcm_table_manager_->DeleteTable(999999),
      StatusIs(StratumErrorSpace(), ERR_ENTRY_NOT_FOUND, HasSubstr("999999")));
}

TEST_F(BcmTableManagerTest, UpdateTableEntryMeterSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up dummy ACL table.
  AclTable acl_table =
      CreateAclTable(/*p4_id=*/kTableId1, /*match_fields=*/{kFieldId1},
                     /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10);
  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  // Add dummy flow for which to modify meter.
  ::p4::v1::TableEntry entry;
  entry.set_priority(1);
  entry.set_table_id(kTableId1);
  entry.add_match()->set_field_id(kFieldId1);
  ASSERT_OK(bcm_table_manager_->AddAclTableEntry(entry, 1));

  ::p4::v1::DirectMeterEntry meter;
  *meter.mutable_table_entry() = entry;
  meter.mutable_config()->set_pir(512);
  meter.mutable_config()->set_pburst(128);
  meter.mutable_config()->set_cir(512);
  meter.mutable_config()->set_cburst(128);

  // Store fresh meter configuration.
  EXPECT_OK(bcm_table_manager_->UpdateTableEntryMeter(meter));

  // Store modified meter configuration meter.
  meter.mutable_config()->set_pir(1024);

  EXPECT_OK(bcm_table_manager_->UpdateTableEntryMeter(meter));
}

TEST_F(BcmTableManagerTest, UpdateTableEntryMeterFailure) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::DirectMeterEntry meter;
  auto* entry = meter.mutable_table_entry();
  entry->set_priority(1);
  entry->set_table_id(1234);
  entry->add_match()->set_field_id(kFieldId1);
  meter.mutable_config();

  // State update should fail if table specified in meter does not exist.
  EXPECT_FALSE(bcm_table_manager_->UpdateTableEntryMeter(meter).ok());

  entry->set_table_id(kTableId1);

  // State update should fail if table entry specified does not exist.
  EXPECT_FALSE(bcm_table_manager_->UpdateTableEntryMeter(meter).ok());

  // Add dummy non-ACL flow.
  ASSERT_OK(bcm_table_manager_->AddTableEntry(*entry));

  // State update should fail if table entry is not an ACL flow.
  EXPECT_FALSE(bcm_table_manager_->UpdateTableEntryMeter(meter).ok());
}

TEST_F(BcmTableManagerTest, AddActionProfileMemberSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));

  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));

  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 0, 0));
}

TEST_F(BcmTableManagerTest, AddActionProfileMemberFailureForNoMemberId) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;

  member1.set_action_profile_id(kActionProfileId1);

  ::util::Status status = bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Need non-zero member_id and action_profile_id:"));
}

TEST_F(BcmTableManagerTest, AddActionProfileMemberFailureForNoActionProfileId) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1;

  member1.set_member_id(kMemberId1);

  ::util::Status status = bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Need non-zero member_id and action_profile_id:"));
}

TEST_F(BcmTableManagerTest, AddActionProfileGroupSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2, member3;
  ::p4::v1::ActionProfileGroup group1, group2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);
  member3.set_member_id(kMemberId3);
  member3.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.add_members()->set_member_id(kMemberId2);  // two members in group1
  group2.set_group_id(kGroupId2);
  group2.set_action_profile_id(kActionProfileId1);
  group2.add_members()->set_member_id(kMemberId3);  // one member in group2

  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId2));

  // Need to first add the members, otherwise the groups cannot be added.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member3, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId3,
      kLogicalPort2));

  // Now the groups can be added.
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group2, kEgressIntfId5));

  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)},
       {kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));
  ASSERT_OK(VerifyActionProfileGroup(
      group2, kEgressIntfId5, 0,
      {{kMemberId3, std::make_tuple(1, 1, kLogicalPort2)}}));
}

TEST_F(BcmTableManagerTest, AddActionProfileGroupFailureForNoGroupId) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileGroup group1;

  group1.set_action_profile_id(kActionProfileId1);

  ::util::Status status =
      bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Need non-zero group_id and action_profile_id:"));
}

TEST_F(BcmTableManagerTest, AddActionProfileGroupFailureForNoActionProfileId) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileGroup group1;

  group1.set_group_id(kGroupId1);

  ::util::Status status =
      bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4);
  ASSERT_FALSE(status.ok());
  EXPECT_THAT(status.error_message(),
              HasSubstr("Need non-zero group_id and action_profile_id:"));
}

TEST_F(BcmTableManagerTest, UpdateActionProfileMemberSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2, member3;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);
  member3.set_member_id(kMemberId1);  // the same as member1
  member3.set_action_profile_id(kActionProfileId1);

  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));

  // Add the two members.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));

  // Now update the member with ID kMemberId1.
  ASSERT_OK(bcm_table_manager_->UpdateActionProfileMember(
      member3, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kLogicalPort2));

  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(member3,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort2, 0, 0));
}

TEST_F(BcmTableManagerTest, UpdateActionProfileMemberFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, UpdateActionProfileGroupSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2, member3;
  ::p4::v1::ActionProfileGroup group1, group2, group3, group4;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);
  member3.set_member_id(kMemberId3);
  member3.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.add_members()->set_member_id(kMemberId2);  // two members in group1
  group2.set_group_id(kGroupId2);
  group2.set_action_profile_id(kActionProfileId1);
  group2.add_members()->set_member_id(kMemberId3);  // one member in group2
  group3.set_group_id(kGroupId1);                   // same as group 1
  group3.set_action_profile_id(kActionProfileId1);
  group3.add_members()->set_member_id(kMemberId2);  // one member in group3
  group4.set_group_id(kGroupId2);                   // same as group 2
  group4.set_action_profile_id(kActionProfileId1);
  group4.add_members()->set_member_id(kMemberId1);
  group4.add_members()->set_member_id(kMemberId3);  // two members in group4
  group4.mutable_members(0)->set_weight(5);         // new weight for kMemberId1

  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId2));

  // Need to first add the members, otherwise the groups cannot be added.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member3, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId3,
      kLogicalPort2));

  // Add the two groups.
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group2, kEgressIntfId5));

  // Now modify the two groups. The members of the groups and the weights of
  // some members are changing.
  ASSERT_OK(bcm_table_manager_->UpdateActionProfileGroup(group3));
  ASSERT_OK(VerifyActionProfileGroup(
      group3, kEgressIntfId4, 0,
      {{kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));
  ASSERT_OK(VerifyActionProfileGroup(
      group2, kEgressIntfId5, 0,
      {{kMemberId3, std::make_tuple(1, 1, kLogicalPort2)}}));

  ASSERT_OK(bcm_table_manager_->UpdateActionProfileGroup(group4));
  ASSERT_OK(VerifyActionProfileGroup(
      group3, kEgressIntfId4, 0,
      {{kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));
  ASSERT_OK(VerifyActionProfileGroup(
      group4, kEgressIntfId5, 0,
      {{kMemberId1, std::make_tuple(5, 1, kLogicalPort1)},
       {kMemberId3, std::make_tuple(1, 1, kLogicalPort2)}}));
}

TEST_F(BcmTableManagerTest, UpdateActionProfileGroupFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, DeleteActionProfileMemberSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));

  // Add the two members.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));

  ASSERT_TRUE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_TRUE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));

  // Remove member1.
  ASSERT_OK(bcm_table_manager_->DeleteActionProfileMember(member1));

  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_TRUE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));

  // Remove member2.
  ASSERT_OK(bcm_table_manager_->DeleteActionProfileMember(member2));

  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileMemberExists(kMemberId2));
}

TEST_F(BcmTableManagerTest, DeleteActionProfileMemberFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, DeleteActionProfileGroupSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  ::p4::v1::ActionProfileMember member1, member2;
  ::p4::v1::ActionProfileGroup group1, group2;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);
  member2.set_member_id(kMemberId2);
  member2.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);
  group1.add_members()->set_member_id(kMemberId2);  // two members in group1
  group2.set_group_id(kGroupId2);                   // empty group2
  group2.set_action_profile_id(kActionProfileId1);

  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId2));

  // Need to first add the members, otherwise the groups cannot be added.
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1));

  // Add the two groups.
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group2, kEgressIntfId5));

  ASSERT_OK(VerifyActionProfileGroup(
      group1, kEgressIntfId4, 0,
      {{kMemberId1, std::make_tuple(1, 1, kLogicalPort1)},
       {kMemberId2, std::make_tuple(1, 1, kTrunkPort1)}}));
  ASSERT_OK(VerifyActionProfileGroup(group2, kEgressIntfId5, 0, {}));

  // Remove group1.
  ASSERT_OK(bcm_table_manager_->DeleteActionProfileGroup(group1));

  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId1));
  ASSERT_TRUE(bcm_table_manager_->ActionProfileGroupExists(kGroupId2));

  // Also make sure the group_ref_count for old members of group1 are 0 now.
  ASSERT_OK(VerifyActionProfileMember(member1,
                                      BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT,
                                      kEgressIntfId1, kLogicalPort1, 0, 0));
  ASSERT_OK(VerifyActionProfileMember(
      member2, BcmNonMultipathNexthop::NEXTHOP_TYPE_TRUNK, kEgressIntfId2,
      kTrunkPort1, 0, 0));

  // Remove group2.
  ASSERT_OK(bcm_table_manager_->DeleteActionProfileGroup(group2));

  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId1));
  ASSERT_FALSE(bcm_table_manager_->ActionProfileGroupExists(kGroupId2));
}

TEST_F(BcmTableManagerTest, DeleteActionProfileGroupFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetGroupsForMemberSuccess) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetGroupsForMemberFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, ActionProfileMemberExists) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, ActionProfileGroupExists) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetBcmNonMultipathNexthopInfoSuccess) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetBcmNonMultipathNexthopInfoFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetBcmMultipathNexthopInfoSuccess) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, GetBcmMultipathNexthopInfoFailure) {
  // TODO: Implement this test.
}

TEST_F(BcmTableManagerTest, ReadActionProfileMembersSuccess) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  WriterMock<::p4::v1::ReadResponse> writer_mock;

  // First make sure read works even before anything is added. At this time
  // a read should not should return empty response.
  {
    ::p4::v1::ReadResponse resp;
    EXPECT_CALL(writer_mock, Write(EqualsProto(resp)))
        .Times(2)
        .WillRepeatedly(Return(true));
    std::vector<::p4::v1::TableEntry*> acl_flows;
    ASSERT_OK(bcm_table_manager_->ReadTableEntries({}, &resp, &acl_flows));
    ASSERT_OK(bcm_table_manager_->ReadActionProfileMembers({}, &writer_mock));
    ASSERT_OK(bcm_table_manager_->ReadActionProfileGroups({}, &writer_mock));
  }

  // Now try to add some members, groups and flow.
  ::p4::v1::ActionProfileMember member1;
  ::p4::v1::ActionProfileGroup group1;
  ::p4::v1::TableEntry entry1;

  member1.set_member_id(kMemberId1);
  member1.set_action_profile_id(kActionProfileId1);

  group1.set_group_id(kGroupId1);
  group1.set_action_profile_id(kActionProfileId1);
  group1.add_members()->set_member_id(kMemberId1);  // one member in group1

  entry1.set_table_id(kTableId1);
  entry1.add_match()->set_field_id(kFieldId1);
  entry1.mutable_action()->set_action_profile_member_id(kMemberId1);

  ASSERT_OK(bcm_table_manager_->AddActionProfileMember(
      member1, BcmNonMultipathNexthop::NEXTHOP_TYPE_PORT, kEgressIntfId1,
      kLogicalPort1));
  ASSERT_OK(bcm_table_manager_->AddActionProfileGroup(group1, kEgressIntfId4));
  ASSERT_OK(bcm_table_manager_->AddTableEntry(entry1));

  // Now try to read the entries back.
  {
    ::p4::v1::ReadResponse resp;
    *resp.add_entities()->mutable_action_profile_member() = member1;
    EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));
    ASSERT_OK(bcm_table_manager_->ReadActionProfileMembers({}, &writer_mock));
  }
  {
    ::p4::v1::ReadResponse resp;
    *resp.add_entities()->mutable_action_profile_group() = group1;
    EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));
    ASSERT_OK(bcm_table_manager_->ReadActionProfileGroups({}, &writer_mock));
  }
  {
    ::p4::v1::ReadResponse resp;
    *resp.add_entities()->mutable_table_entry() = entry1;
    std::vector<::p4::v1::TableEntry*> acl_flows;
    ASSERT_OK(bcm_table_manager_->ReadTableEntries({}, &resp, &acl_flows));
  }
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_AclWithMultipleConstConditions) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up the ACL table.
  absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>
      const_conditions = {{P4_HEADER_IPV4, true}, {P4_HEADER_TCP, true}};
  AclTable acl_table = CreateAclTable(
      /*p4_id=*/100, /*match_fields=*/{kFieldId1},
      /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
      /*priority=*/20, /*physical_table_id=*/1, const_conditions);
  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  // Set up the input CommonFlowEntry. This does not have const condition data.
  CommonFlowEntry source;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_info { id: 100 name: "test_table" pipeline_stage: INGRESS_ACL }
    fields { type: P4_FIELD_TYPE_ETH_TYPE value { u32: 10 } }
    action { type: P4_ACTION_TYPE_FUNCTION }
    priority: 10
  )PROTO", &source));

  BcmFlowEntry expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    bcm_table_type: BCM_TABLE_ACL
    bcm_acl_table_id: 1
    fields { type: ETH_TYPE value { u32: 10 } }
    acl_stage: BCM_ACL_STAGE_IFP
  )PROTO", &expected));
  expected.set_priority((20 << 16) + 10);
  ASSERT_OK_AND_ASSIGN(*expected.add_fields(), ConstCondition(P4_HEADER_IPV4));
  ASSERT_OK_AND_ASSIGN(*expected.add_fields(), ConstCondition(P4_HEADER_TCP));

  BcmFlowEntry actual;
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_AclWithIPv6IcmpConstConditions) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up the ACL table.
  absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>
      const_conditions = {{P4_HEADER_IPV6, true}, {P4_HEADER_ICMP, true}};
  AclTable acl_table = CreateAclTable(
      /*p4_id=*/100, /*match_fields=*/{kFieldId1},
      /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
      /*priority=*/20, /*physical_table_id=*/1, const_conditions);
  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  // Set up the input CommonFlowEntry. This does not have const condition data.
  CommonFlowEntry source;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_info { id: 100 name: "test_table" pipeline_stage: INGRESS_ACL }
    fields { type: P4_FIELD_TYPE_ETH_TYPE value { u32: 10 } }
    action { type: P4_ACTION_TYPE_FUNCTION }
    priority: 10
  )PROTO", &source));

  BcmFlowEntry expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    bcm_table_type: BCM_TABLE_ACL
    bcm_acl_table_id: 1
    fields { type: ETH_TYPE value { u32: 10 } }
    fields { type: IP_TYPE value { u32: 0x86dd } }
    fields { type: IP_PROTO_NEXT_HDR value { u32: 58 } }
    acl_stage: BCM_ACL_STAGE_IFP
  )PROTO", &expected));
  expected.set_priority((20 << 16) + 10);

  BcmFlowEntry actual;
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

TEST_F(BcmTableManagerTest,
       CommonFlowEntryToBcmFlowEntry_AclWithUnsupportedConstConditions) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  // Set up the ACL table.
  absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>
      const_conditions = {{P4_HEADER_VLAN, true}};
  AclTable acl_table = CreateAclTable(
      /*p4_id=*/100, /*match_fields=*/{kFieldId1},
      /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
      /*priority=*/20, /*physical_table_id=*/1, const_conditions);
  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  // Set up the input CommonFlowEntry. This does not have const condition data.
  CommonFlowEntry source;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_info { id: 100 name: "test_table" pipeline_stage: INGRESS_ACL }
    fields { type: P4_FIELD_TYPE_ETH_TYPE value { u32: 10 } }
    action { type: P4_ACTION_TYPE_FUNCTION }
    priority: 10
  )PROTO", &source));

  BcmFlowEntry actual;
  EXPECT_THAT(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
                  source, ::p4::v1::Update::INSERT, &actual),
              StatusIs(HerculesErrorSpace(), ERR_OPER_NOT_SUPPORTED, _));
}

class ConstConditionTest : public BcmTableManagerTest,
                           public testing::WithParamInterface<P4HeaderType> {};

TEST_P(ConstConditionTest,
       CommonFlowEntryToBcmFlowEntry_AclWithConstCondition) {
  ASSERT_NO_FATAL_FAILURE(PushTestConfig());

  P4HeaderType header_type = GetParam();

  // Set up the ACL table.
  absl::flat_hash_map<P4HeaderType, bool, EnumHash<P4HeaderType>>
      const_conditions = {{header_type, true}};
  AclTable acl_table = CreateAclTable(
      /*p4_id=*/100, /*match_fields=*/{kFieldId1},
      /*stage=*/BCM_ACL_STAGE_IFP, /*size=*/10,
      /*priority=*/20, /*physical_table_id=*/1, const_conditions);
  ASSERT_OK(bcm_table_manager_->AddAclTable(acl_table));

  // Set up the input CommonFlowEntry. This does not have const condition data.
  CommonFlowEntry source;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    table_info { id: 100 name: "test_table" pipeline_stage: INGRESS_ACL }
    fields { type: P4_FIELD_TYPE_ETH_TYPE value { u32: 10 } }
    action { type: P4_ACTION_TYPE_FUNCTION }
    priority: 10
  )PROTO", &source));

  BcmFlowEntry expected;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    bcm_table_type: BCM_TABLE_ACL
    bcm_acl_table_id: 1
    fields { type: ETH_TYPE value { u32: 10 } }
    acl_stage: BCM_ACL_STAGE_IFP
  )PROTO", &expected));
  expected.set_priority((20 << 16) + 10);
  ASSERT_OK_AND_ASSIGN(*expected.add_fields(), ConstCondition(header_type));

  BcmFlowEntry actual;
  ASSERT_OK(bcm_table_manager_->CommonFlowEntryToBcmFlowEntry(
      source, ::p4::v1::Update::INSERT, &actual));
  EXPECT_THAT(actual, UnorderedEqualsProto(expected));
}

INSTANTIATE_TEST_CASE_P(BcmTableManagerTest, ConstConditionTest,
                        ::testing::Values(P4_HEADER_ARP, P4_HEADER_IPV4,
                                          P4_HEADER_IPV6, P4_HEADER_TCP,
                                          P4_HEADER_UDP, P4_HEADER_UDP_PAYLOAD,
                                          P4_HEADER_GRE, P4_HEADER_ICMP),
                        ParamName);

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
