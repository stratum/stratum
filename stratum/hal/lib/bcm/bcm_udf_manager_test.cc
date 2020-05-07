// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_udf_manager.h"

#include <endian.h>

#include <algorithm>
#include <set>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/p4/p4_table_mapper_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {

namespace hal {
namespace bcm {
namespace {

using test_utils::PartiallyUnorderedEqualsProto;
using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using ::stratum::test_utils::StatusIs;
using UdfSpec = BcmHardwareSpecs::ChipModelSpec::UdfSpec;

// This matcher verifies that all chunks in a BcmUdfSet come from the same set
// according to a provided UdfSpec.
MATCHER_P(UsesOneUdfSet, udf_spec, "") {
  absl::flat_hash_set<int> used_ids;
  if (arg.chunks().empty()) return true;
  *result_listener << "Expected that UdfSet: " << arg.ShortDebugString()
                   << " uses one UDF set. Actual:\n";
  int udf_set = arg.chunks(0).id() / udf_spec.chunks_per_set();
  if (udf_set >= udf_spec.set_count()) {
    *result_listener << "UDF id " << arg.chunks(0).id()
                     << " is out of range for udf_spec: "
                     << udf_spec.ShortDebugString() << ".";
    return false;
  }
  for (const auto& chunk : arg.chunks()) {
    int chunk_set = chunk.id() / udf_spec.chunks_per_set();
    if (chunk_set != udf_set) {
      *result_listener << "UDF set " << chunk_set
                       << " for chunk: " << chunk.ShortDebugString()
                       << " does not match set " << udf_set
                       << " for chunk: " << arg.chunks(0).ShortDebugString();
      return false;
    }
    if (!used_ids.insert(chunk.id()).second) {
      *result_listener << "UDF set set contains repeated ID: " << chunk.id();
      return false;
    }
  }
  return true;
}

// This matcher verifies that all chunks in a BcmUdfSet are implemented
// physically by a given iterable collection of expected BcmUdfSets. Physical
// division of sets is determined by the provided UdfSpec.
MATCHER_P2(UsesUdfSets, udf_spec, expected_sets, "") {
  std::vector<BcmUdfSet> divided_sets(udf_spec.set_count());
  int used_sets = 0;
  absl::flat_hash_set<int> used_ids;
  // Split the chunks in arg into their physical sets.
  for (auto chunk : arg.chunks()) {
    int set_id = chunk.id() / udf_spec.chunks_per_set();
    if (set_id >= udf_spec.set_count()) {
      *result_listener << "UDF id " << chunk.id()
                       << " is out of range for udf_spec: "
                       << udf_spec.ShortDebugString() << ".";
      return false;
    }
    if (divided_sets[set_id].chunks().empty()) {
      ++used_sets;
    }
    if (!used_ids.insert(chunk.id()).second) {
      *result_listener << "UDF set set contains repeated ID: " << chunk.id();
      return false;
    }
    chunk.clear_id();
    *divided_sets[set_id].add_chunks() = chunk;
  }
  if (used_sets != expected_sets.size()) {
    *result_listener << "Expected " << expected_sets.size()
                     << " hardware sets, but BcmUdfSet uses " << used_sets
                     << " hardware sets.";
    return false;
  }

  struct {
    bool operator()(const BcmUdfSet::UdfChunk& x,
                    const BcmUdfSet::UdfChunk& y) {
      uint64 x_position = (static_cast<uint64>(x.layer()) << 32) + x.offset();
      uint64 y_position = (static_cast<uint64>(y.layer()) << 32) + y.offset();
      return x_position < y_position;
    }
  } udf_chunk_less;

  // Use the debug string as a hash for arg physical UDF sets.
  std::set<std::string> arg_set_strings;
  for (BcmUdfSet& udf_set : divided_sets) {
    std::sort(udf_set.mutable_chunks()->begin(),
              udf_set.mutable_chunks()->end(), udf_chunk_less);
    arg_set_strings.insert(udf_set.ShortDebugString());
  }

  // Use the debug string as a hash for expected physical UDF sets.
  std::set<std::string> expected_set_strings;
  for (BcmUdfSet udf_set : expected_sets) {
    auto& chunks = *udf_set.mutable_chunks();
    for (auto& chunk : chunks) {
      chunk.clear_id();
    }
    std::sort(chunks.begin(), chunks.end(), udf_chunk_less);
    expected_set_strings.insert(udf_set.ShortDebugString());
  }

  // Calculate the set differences.
  std::vector<std::string> expected_only(expected_set_strings.size());
  auto it = std::set_difference(
      expected_set_strings.begin(), expected_set_strings.end(),
      arg_set_strings.begin(), arg_set_strings.end(), expected_only.begin());
  expected_only.resize(it - expected_only.begin());

  std::vector<std::string> arg_only(arg_set_strings.size());
  it = std::set_difference(arg_set_strings.begin(), arg_set_strings.end(),
                           expected_set_strings.begin(),
                           expected_set_strings.end(), arg_only.begin());
  arg_only.resize(it - arg_only.begin());

  if (!expected_only.empty()) {
    *result_listener << "Unmatched expected sets:\n"
                     << absl::StrJoin(expected_only, "\n");
  }
  if (!arg_only.empty()) {
    *result_listener << "Unmatched arg sets:\n"
                     << absl::StrJoin(arg_only, "\n");
  }
  return arg_only.empty() && expected_only.empty();
}

// Helper class to build ACL Tables with the specified fields.
class AclTableBuilder {
 public:
  // Constructor.
  //   p4_table_mapper: This class will mock match field lookups in this object.
  //   id: ID for this table. used to generate table preamble.
  AclTableBuilder(P4TableMapperMock* p4_table_mapper, int id)
      : p4_table_mapper_(p4_table_mapper),
        id_(id),
        mapped_fields_(),
        stage_(BCM_ACL_STAGE_IFP) {}

  // Build and return the AclTable object. This table contains:
  // * The name of the table (from the constructor).
  // * The ID of the table (from the constructor).
  // * MappedFields of the table (from MatchField()).
  // * The stage of the table (from Stage()).
  AclTable Build() {
    ::p4::config::v1::Table p4_table;
    p4_table.mutable_preamble()->set_name(absl::StrCat("table_", id_));
    p4_table.mutable_preamble()->set_id(id_);
    for (const auto& pair : mapped_fields_) {
      p4_table.add_match_fields()->set_id(pair.first);
    }
    AclTable table(p4_table, stage_, 1, {});
    return table;
  }

  // Set (or overwrite) the stage for this table.
  AclTableBuilder& Stage(BcmAclStage stage) {
    stage_ = stage;
    return *this;
  }

  // Add a match field to this table. Also mocks the lookup from the match field
  // ID to the MappedField object.
  AclTableBuilder& MatchField(int field_id, const MappedField& mapped_field) {
    EXPECT_CALL(*p4_table_mapper_, MapMatchField(id_, field_id, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<2>(mapped_field), Return(::util::OkStatus())));
    mapped_fields_.emplace(field_id, mapped_field);
    return *this;
  }

 private:
  P4TableMapperMock* p4_table_mapper_;  // Pointer to the mapper object for
                                        // match field lookup mocking.
  int id_;                              // Table ID.
  std::map<int, MappedField>
      mapped_fields_;  // Map of this table's P4 MatchField IDs to MappedFields.
  BcmAclStage stage_;  // This table's stage.
};

// Returns a sample list of field types for ease-of-use. Crashes if type is not
// supported.
const MappedField& MappedFieldByType(P4FieldType type) {
  static const auto* field_map = []() {
    auto* field_map = new absl::flat_hash_map<P4FieldType, MappedField,
                                              EnumHash<P4FieldType>>();
    MappedField mapped_field;
    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ARP_TPA
      bit_offset: 192
      bit_width: 32
      header_type: P4_HEADER_ARP
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_DST
      bit_offset: 0
      bit_width: 48
      header_type: P4_HEADER_ETHERNET
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_SRC
      bit_offset: 48
      bit_width: 48
      header_type: P4_HEADER_ETHERNET
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_ETH_TYPE
      bit_offset: 96
      bit_width: 16
      header_type: P4_HEADER_ETHERNET
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_GRE_CHECKSUM_BIT
      bit_offset: 0
      bit_width: 1
      header_type: P4_HEADER_GRE
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_GRE_FLAGS
      bit_offset: 8
      bit_width: 5
      header_type: P4_HEADER_GRE
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_GRE_RECURSION
      bit_offset: 5
      bit_width: 3
      header_type: P4_HEADER_GRE
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      type: P4_FIELD_TYPE_COLOR
    )PROTO", &mapped_field));
    field_map->emplace(mapped_field.type(), mapped_field);

    return field_map;
  }();
  return field_map->at(type);
}

// UDF filter function to consider all fields as UDF.
bool UdfAllFields(const MappedField& /*unused*/, BcmAclStage /*unused*/) {
  return true;
}

// Returns a sample list of BcmUdfSets for ease-of-use. Crashes if type is not
// supported.
const BcmUdfSet& UdfSetByType(P4FieldType type) {
  static const auto* udf_map = []() {
    auto* udf_map = new absl::flat_hash_map<P4FieldType, BcmUdfSet,
                                            EnumHash<P4FieldType>>();
    BcmUdfSet udf_set;
    CHECK_OK(ParseProtoFromString(R"PROTO(
      chunks { layer: L3_HEADER offset: 24 }
      chunks { layer: L3_HEADER offset: 26 }
    )PROTO", &udf_set));
    udf_map->emplace(P4_FIELD_TYPE_ARP_TPA, udf_set);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      chunks { layer: L2_HEADER offset: 0 }
      chunks { layer: L2_HEADER offset: 2 }
      chunks { layer: L2_HEADER offset: 4 }
    )PROTO", &udf_set));
    udf_map->emplace(P4_FIELD_TYPE_ETH_DST, udf_set);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      chunks { layer: L2_HEADER offset: 6 }
      chunks { layer: L2_HEADER offset: 8 }
      chunks { layer: L2_HEADER offset: 10 }
    )PROTO", &udf_set));
    udf_map->emplace(P4_FIELD_TYPE_ETH_SRC, udf_set);

    CHECK_OK(ParseProtoFromString(R"PROTO(
      chunks { layer: L2_HEADER offset: 12 }
    )PROTO", &udf_set));
    udf_map->emplace(P4_FIELD_TYPE_ETH_TYPE, udf_set);

    // GRE checksum, flags, and recursion are all within the first 16-bits of
    // the GRE header.
    CHECK_OK(ParseProtoFromString(R"PROTO(
      chunks { layer: L4_HEADER offset: 0 }
    )PROTO", &udf_set));
    udf_map->emplace(P4_FIELD_TYPE_GRE_CHECKSUM_BIT, udf_set);
    udf_map->emplace(P4_FIELD_TYPE_GRE_FLAGS, udf_set);
    udf_map->emplace(P4_FIELD_TYPE_GRE_RECURSION, udf_set);

    return udf_map;
  }();
  return udf_map->at(type);
}

TEST(BcmUdfManagerTest, Constructor) {
  P4TableMapperMock p4_table_mapper;
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(8);
  udf_spec.set_set_count(2);
  EXPECT_OK(BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                          /*num_controller_sets=*/1, /*unit=*/1,
                                          &p4_table_mapper)
                .status());
}

// This case expects to fail creation when the number of controller sets is
// greater than the number of total sets.
TEST(BcmUdfManagerTest, CreateMoreControllerSetsThanTotalSets) {
  P4TableMapperMock p4_table_mapper;
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(8);
  udf_spec.set_set_count(2);
  EXPECT_THAT(BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                            /*num_controller_sets=*/3,
                                            /*unit=*/1, &p4_table_mapper)
                  .status(),
              StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM, _));
}

// This case expects no UDFs to be created when no ACL tables are used in setup.
TEST(BcmUdfManagerTest, SetUpStaticUdfsWithNoAclTables) {
  P4TableMapperMock p4_table_mapper;
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(32);
  udf_spec.set_chunks_per_set(4);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(auto bcm_udf_manager, BcmUdfManager::CreateInstance(
                                                 &bcm_sdk_interface, udf_spec,
                                                 /*num_controller_sets=*/1,
                                                 /*unit=*/1, &p4_table_mapper));
  std::vector<AclTable> acl_tables;
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  // Install should not do anything.
  ASSERT_OK(bcm_udf_manager->InstallUdfs());
}

// This case expects no UDFs to be created when ACL tables do not use UDFs.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_NonUdfAclTables) {
  // Set up 2 ACL tables.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))
          .Build());
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 2)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))
          .MatchField(3, MappedFieldByType(P4_FIELD_TYPE_ETH_SRC))
          .Build());

  // Don't consider any mapped fields to be UDFs.
  auto func = [](const MappedField& /*unused*/, BcmAclStage /*unused*/) {
    return false;
  };

  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(32);
  udf_spec.set_chunks_per_set(4);
  udf_spec.set_set_count(2);

  // Create the UDF manager and set up the tables.
  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/1,
                                    /*unit=*/1, &p4_table_mapper, func));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  EXPECT_THAT(acl_tables[0].UdfMatchFields(), IsEmpty());
  EXPECT_THAT(acl_tables[1].UdfMatchFields(), IsEmpty());
  // Install should not do anything.
  ASSERT_OK(bcm_udf_manager->InstallUdfs());
}

// This case tests ACL tables where all the fields are UDF match fields. All
// fields should be listed as UDF match fields in the ACL tables.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_FullUdfAclTables) {
  // Set up 2 ACL tables.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))
          .Build());
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 2)
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))
          .MatchField(3, MappedFieldByType(P4_FIELD_TYPE_ETH_SRC))
          .Build());

  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(8);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  EXPECT_THAT(acl_tables[0].UdfMatchFields(),
              UnorderedElementsAreArray(acl_tables[0].MatchFields()));
  EXPECT_THAT(acl_tables[1].UdfMatchFields(),
              UnorderedElementsAreArray(acl_tables[1].MatchFields()));

  // Test the installation.
  BcmUdfSet expected_udf_set;
  expected_udf_set.MergeFrom(UdfSetByType(P4_FIELD_TYPE_ARP_TPA));
  expected_udf_set.MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_TYPE));
  expected_udf_set.MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_SRC));

  BcmUdfSet actual_udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&actual_udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  EXPECT_THAT(actual_udf_set, PartiallyUnorderedEqualsProto(expected_udf_set));
  EXPECT_THAT(actual_udf_set, UsesOneUdfSet(udf_spec));
}

// This case tests ACL tables where some fields are UDF match fields. The
// specific UDF fields should be listed as UDF match fields in the ACL tables.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_PartialUdfAclTables) {
  // Set up 2 ACL tables. These tables require 6 chunks in total.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))   // 2 chunks
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))  // 1 chunk
          .Build());
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 2)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))   // 2 chunks
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))  // 1 chunk
          .MatchField(3, MappedFieldByType(P4_FIELD_TYPE_ETH_SRC))   // 3 chunks
          .Build());

  // Consider all mapped fields to be UDFs.
  auto func = [](const MappedField& mapped_field, BcmAclStage /*unused*/) {
    return mapped_field.type() != P4_FIELD_TYPE_ARP_TPA;
  };

  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(8);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, func));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  EXPECT_THAT(acl_tables[0].UdfMatchFields(), UnorderedElementsAre(2));
  EXPECT_THAT(acl_tables[1].UdfMatchFields(), UnorderedElementsAre(2, 3));

  // Test the installation.
  BcmUdfSet expected_udf_set;
  expected_udf_set.MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_TYPE));
  expected_udf_set.MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_SRC));

  BcmUdfSet actual_udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&actual_udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  EXPECT_THAT(actual_udf_set, PartiallyUnorderedEqualsProto(expected_udf_set));
  EXPECT_THAT(actual_udf_set, UsesOneUdfSet(udf_spec));
}

// This case tests static UDF setup where multiple sets are required to realize
// all the ACL tables.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_MultiSetAclDistribution) {
  // Set up 2 ACL tables. Each ACL table requires 6 chunks individually.
  // Combined into one set, they require 9 chunks. This test uses 2 8-chunk
  // sets.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))   // 2 chunks
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))  // 1 chunk
          .MatchField(3, MappedFieldByType(P4_FIELD_TYPE_ETH_DST))   // 3 chunks
          .Build());
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 2)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))   // 2 chunks
          .MatchField(2, MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE))  // 1 chunk
          .MatchField(3, MappedFieldByType(P4_FIELD_TYPE_ETH_SRC))   // 3 chunks
          .Build());

  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(8);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));

  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  EXPECT_THAT(acl_tables[0].UdfMatchFields(), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(acl_tables[1].UdfMatchFields(), UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(
      std::vector<int>({acl_tables[0].UdfSetId(), acl_tables[1].UdfSetId()}),
      UnorderedElementsAre(1, 2));

  // Test the installation.
  std::vector<BcmUdfSet> expected_udf_sets(2);
  expected_udf_sets[0].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ARP_TPA));
  expected_udf_sets[0].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_TYPE));
  expected_udf_sets[0].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_SRC));
  expected_udf_sets[1].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ARP_TPA));
  expected_udf_sets[1].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_TYPE));
  expected_udf_sets[1].MergeFrom(UdfSetByType(P4_FIELD_TYPE_ETH_DST));

  BcmUdfSet actual_udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&actual_udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());
  EXPECT_THAT(actual_udf_set, UsesUdfSets(udf_spec, expected_udf_sets));
}

// This case tests that an extra UDF chunk is used when a match field is off a
// chunk boundary.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_OffBoundaryChunks) {
  // This field is normally 2-chunks wide (at 2-byte chunks). Since the offset
  // is not on a chunk-boundary, it will need an extra chunk.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 19
    bit_width: 32
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1).MatchField(1, mapped_field).Build());

  // First, we test a UDF manager with 3 chunks-per-set. This should fit.
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  EXPECT_THAT(acl_tables[0].UdfMatchFields(), UnorderedElementsAre(1));

  BcmUdfSet actual_udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&actual_udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());
  EXPECT_EQ(actual_udf_set.chunks_size(), 3);

  // Next, we test a UDF manager with 2 chunks-per-set. This should not fit.
  udf_spec.set_chunks_per_set(2);
  ASSERT_OK_AND_ASSIGN(
      bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));
}

// Class for running MappedFieldToBcmFieldsTests. The parameterized value
// determines the offset of the MappedField (base_offset - <param>).
class MappedFieldToBcmFieldsTest : public testing::TestWithParam<int> {
 public:
  // The Buffer class provides a 64-bit buffer that acts as a uint64 and a byte
  // buffer.
  class Buffer {
   public:
    // Constructors.
    Buffer() : buffer_(0) {}
    explicit Buffer(uint64 value) : buffer_(value) {}

    // Set the uint64 value directly.
    void set_u64(uint64 value) { buffer_ = value; }

    // Write bytes to the buffer.
    void set_b(size_t pos, const std::string& value) {
      CHECK_LE(pos + value.size(), sizeof(buffer_));
      uint8* byte = reinterpret_cast<uint8*>(&buffer_);
      std::memcpy(byte + pos, value.c_str(), value.size());
    }

    // Get the buffer value.
    uint64 u64() const { return buffer_; }

    // Return a string with the real byte ordering of the buffer.
    std::string ToString() const {
      std::string s;
      for (int i = 0; i < sizeof(buffer_); ++i) {
        const uint8* byte = reinterpret_cast<const uint8*>(&buffer_);
        absl::StrAppendFormat(&s, "%02x.", *(byte + i));
      }
      return s;
    }

   private:
    uint64 buffer_;
  };
};

// MappedFieldToBcmFieldsTest::Buffer matcher.
MATCHER_P(EqualsBuffer, expected, "") {
  *result_listener << "\nExpected Buffer: " << expected.ToString()
                   << "\nActual Buffer:   " << arg.ToString();
  return arg.u64() == expected.u64();
}

// MappedFieldToBcmFields should fill UDF chunks as if the u32 value/mask is a
// right-justified network-order bit-field in the original mapped field.
TEST_P(MappedFieldToBcmFieldsTest, U32) {
  const uint32 kShift = GetParam();
  constexpr uint32 kValue = 0xf1234567;
  constexpr uint32 kMask = 0xfedcba98;

  // Set up the mapped field.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 32
    bit_width: 32
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));
  mapped_field.set_bit_offset(mapped_field.bit_offset() - kShift);
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables({AclTableBuilder(&p4_table_mapper, 1)
                                        .MatchField(1, mapped_field)
                                        .Build()});

  // Set up the static UDFs.
  UdfSpec udf_spec;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    chunk_bits: 16
    chunks_per_set: 3
    set_count: 1
  )PROTO", &udf_spec));
  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  BcmUdfSet udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  // Grab the chunk --> offset mapping.
  absl::flat_hash_map<int, int> chunk_offsets;
  for (const auto& chunk : udf_set.chunks()) {
    chunk_offsets[chunk.id()] = chunk.offset();
  }

  // Set up the field values.
  mapped_field.mutable_value()->set_u32(kValue);
  mapped_field.mutable_mask()->set_u32(kMask);

  // Map the fields (run the test code).
  ASSERT_OK_AND_ASSIGN(
      std::vector<BcmField> bcm_fields,
      bcm_udf_manager->MappedFieldToBcmFields(1, mapped_field));

  // Capture the values.
  Buffer actual_value;
  Buffer actual_mask;
  for (const BcmField& bcm_field : bcm_fields) {
    int udf_chunk_id = bcm_field.udf_chunk_id();
    ASSERT_EQ(chunk_offsets.count(udf_chunk_id), 1)
        << "MappedFieldTOBcmFields returned unknown chunk id: " << udf_chunk_id;
    actual_value.set_b(chunk_offsets.at(udf_chunk_id), bcm_field.value().b());
    actual_mask.set_b(chunk_offsets.at(udf_chunk_id), bcm_field.mask().b());
  }

  // Calculate expected values.
  Buffer expected_value(htobe64(static_cast<uint64>(kValue) << kShift));
  Buffer expected_mask(htobe64(static_cast<uint64>(kMask) << kShift));

  EXPECT_THAT(actual_value, EqualsBuffer(expected_value));
  EXPECT_THAT(actual_mask, EqualsBuffer(expected_mask));
}

// MappedFieldToBcmFields should fill UDF chunks as if the u64 value/mask is a
// right-justified network-order bit-field in the original mapped field.
TEST_P(MappedFieldToBcmFieldsTest, U64) {
  const uint64 kShift = GetParam();
  // 52-bit value.
  constexpr uint64 kValue = 0xf123456789abcdefull;
  constexpr uint64 kMask = 0xfedcba987654321full;

  // Set up the mapped field.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 64
    bit_width: 64
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));
  mapped_field.set_bit_offset(mapped_field.bit_offset() - kShift);
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables({AclTableBuilder(&p4_table_mapper, 1)
                                        .MatchField(1, mapped_field)
                                        .Build()});

  // Set up the static UDFs.
  UdfSpec udf_spec;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    chunk_bits: 16
    chunks_per_set: 5
    set_count: 1
  )PROTO", &udf_spec));
  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  BcmUdfSet udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  // Grab the chunk --> offset mapping.
  absl::flat_hash_map<int, int> chunk_offsets;
  for (const auto& chunk : udf_set.chunks()) {
    chunk_offsets[chunk.id()] = chunk.offset();
  }

  // Set up the field values.
  mapped_field.mutable_value()->set_u64(kValue);
  mapped_field.mutable_mask()->set_u64(kMask);

  // Map the fields (run the test code).
  ASSERT_OK_AND_ASSIGN(
      std::vector<BcmField> bcm_fields,
      bcm_udf_manager->MappedFieldToBcmFields(1, mapped_field));

  // Capture the values.
  Buffer actual_value_upper;
  Buffer actual_value_lower;
  Buffer actual_mask_upper;
  Buffer actual_mask_lower;
  for (const BcmField& bcm_field : bcm_fields) {
    int udf_chunk_id = bcm_field.udf_chunk_id();
    ASSERT_EQ(chunk_offsets.count(udf_chunk_id), 1)
        << "MappedFieldTOBcmFields returned unknown chunk id: " << udf_chunk_id;

    // Fill in the appropriate buffer.
    int offset = chunk_offsets.at(udf_chunk_id);
    Buffer* value_buffer = &actual_value_upper;
    Buffer* mask_buffer = &actual_mask_upper;
    if (offset >= sizeof(uint64)) {
      offset -= sizeof(uint64);
      value_buffer = &actual_value_lower;
      mask_buffer = &actual_mask_lower;
    }
    value_buffer->set_b(offset, bcm_field.value().b());
    mask_buffer->set_b(offset, bcm_field.mask().b());
  }

  // Calculate expected value.
  Buffer expected_value_lower(htobe64(kValue << kShift));
  Buffer expected_value_upper(0);
  if (kShift > 0) {
    expected_value_upper.set_u64(htobe64(kValue >> (64 - kShift)));
  }

  // Calculate expected mask.
  Buffer expected_mask_lower(htobe64(kMask << kShift));
  Buffer expected_mask_upper(0);
  if (kShift > 0) {
    expected_mask_upper.set_u64(htobe64(kMask >> (64 - kShift)));
  }

  EXPECT_THAT(actual_value_lower, EqualsBuffer(expected_value_lower));
  EXPECT_THAT(actual_mask_lower, EqualsBuffer(expected_mask_lower));
  EXPECT_THAT(actual_value_upper, EqualsBuffer(expected_value_upper));
  EXPECT_THAT(actual_mask_upper, EqualsBuffer(expected_mask_upper));
}

// MappedFieldToBcmFields should fill UDF chunks as if the u64 value/mask is a
// right-justified network-order bit-field in the original mapped field. This
// should work even if the bit-field is smaller than 64-bits.
TEST_P(MappedFieldToBcmFieldsTest, U64Partial) {
  const uint64 kShift = GetParam();
  // 52-bit value.
  constexpr uint64 kValue = 0x0003456789abcdefull;
  constexpr uint64 kMask = 0x000cba987654321full;

  // Set up the mapped field.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 64
    bit_width: 52
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));
  mapped_field.set_bit_offset(mapped_field.bit_offset() - kShift);
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables({AclTableBuilder(&p4_table_mapper, 1)
                                        .MatchField(1, mapped_field)
                                        .Build()});

  // Set up the static UDFs.
  UdfSpec udf_spec;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    chunk_bits: 16
    chunks_per_set: 5
    set_count: 1
  )PROTO", &udf_spec));
  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  BcmUdfSet udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  // Grab the chunk --> offset mapping.
  absl::flat_hash_map<int, int> chunk_offsets;
  for (const auto& chunk : udf_set.chunks()) {
    chunk_offsets[chunk.id()] = chunk.offset();
  }

  // Set up the field values.
  mapped_field.mutable_value()->set_u64(kValue);
  mapped_field.mutable_mask()->set_u64(kMask);

  // Map the fields (run the test code).
  ASSERT_OK_AND_ASSIGN(
      std::vector<BcmField> bcm_fields,
      bcm_udf_manager->MappedFieldToBcmFields(1, mapped_field));

  // Capture the values.
  Buffer actual_value_upper;
  Buffer actual_value_lower;
  Buffer actual_mask_upper;
  Buffer actual_mask_lower;
  for (const BcmField& bcm_field : bcm_fields) {
    int udf_chunk_id = bcm_field.udf_chunk_id();
    ASSERT_EQ(chunk_offsets.count(udf_chunk_id), 1)
        << "MappedFieldTOBcmFields returned unknown chunk id: " << udf_chunk_id;

    // Fill in the appropriate buffer.
    int offset = chunk_offsets.at(udf_chunk_id);
    Buffer* value_buffer = &actual_value_upper;
    Buffer* mask_buffer = &actual_mask_upper;
    if (offset >= sizeof(uint64)) {
      offset -= sizeof(uint64);
      value_buffer = &actual_value_lower;
      mask_buffer = &actual_mask_lower;
    }
    value_buffer->set_b(offset, bcm_field.value().b());
    mask_buffer->set_b(offset, bcm_field.mask().b());
  }

  // Calculate expected value.
  const uint64 kExpectedShift = kShift + /*64-52=*/12;
  Buffer expected_value_lower(htobe64(kValue << kExpectedShift));
  Buffer expected_value_upper(htobe64(kValue >> (64 - kExpectedShift)));

  // Calculate expected mask.
  Buffer expected_mask_lower(htobe64(kMask << kExpectedShift));
  Buffer expected_mask_upper(htobe64(kMask >> (64 - kExpectedShift)));

  EXPECT_THAT(actual_value_lower, EqualsBuffer(expected_value_lower));
  EXPECT_THAT(actual_mask_lower, EqualsBuffer(expected_mask_lower));
  EXPECT_THAT(actual_value_upper, EqualsBuffer(expected_value_upper));
  EXPECT_THAT(actual_mask_upper, EqualsBuffer(expected_mask_upper));
}

// MappedFieldToBcmFields should fill UDF chunks based on the b value/mask
// position and value.
TEST_P(MappedFieldToBcmFieldsTest, B) {
  const uint32 kShift = GetParam();
  constexpr uint32 kValue = 0xf1234567;
  const std::string kValueString = "\xf1\x23\x45\x67";
  constexpr uint32 kMask = 0xfedcba98;
  const std::string kMaskString = "\xfe\xdc\xba\x98";

  // Set up the mapped field.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 32
    bit_width: 32
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));
  mapped_field.set_bit_offset(mapped_field.bit_offset() - kShift);
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables({AclTableBuilder(&p4_table_mapper, 1)
                                        .MatchField(1, mapped_field)
                                        .Build()});

  // Set up the static UDFs.
  UdfSpec udf_spec;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    chunk_bits: 16
    chunks_per_set: 3
    set_count: 1
  )PROTO", &udf_spec));
  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  ASSERT_OK(bcm_udf_manager->SetUpStaticUdfs(&acl_tables));
  BcmUdfSet udf_set;
  EXPECT_CALL(bcm_sdk_interface, SetAclUdfChunks(1, _))
      .WillOnce(DoAll(SaveArg<1>(&udf_set), Return(util::OkStatus())));
  ASSERT_OK(bcm_udf_manager->InstallUdfs());

  // Grab the chunk --> offset mapping.
  absl::flat_hash_map<int, int> chunk_offsets;
  for (const auto& chunk : udf_set.chunks()) {
    chunk_offsets[chunk.id()] = chunk.offset();
  }

  // Set up the field values.
  mapped_field.mutable_value()->set_b(kValueString);
  mapped_field.mutable_mask()->set_b(kMaskString);

  // Map the fields (run the test code).
  ASSERT_OK_AND_ASSIGN(
      std::vector<BcmField> bcm_fields,
      bcm_udf_manager->MappedFieldToBcmFields(1, mapped_field));

  // Capture the values.
  Buffer actual_value;
  Buffer actual_mask;
  for (const BcmField& bcm_field : bcm_fields) {
    int udf_chunk_id = bcm_field.udf_chunk_id();
    ASSERT_EQ(chunk_offsets.count(udf_chunk_id), 1)
        << "MappedFieldTOBcmFields returned unknown chunk id: " << udf_chunk_id;
    actual_value.set_b(chunk_offsets.at(udf_chunk_id), bcm_field.value().b());
    actual_mask.set_b(chunk_offsets.at(udf_chunk_id), bcm_field.mask().b());
  }

  // Calculate expected values.
  Buffer expected_value(htobe64(static_cast<uint64>(kValue) << kShift));
  Buffer expected_mask(htobe64(static_cast<uint64>(kMask) << kShift));

  EXPECT_THAT(actual_value, EqualsBuffer(expected_value));
  EXPECT_THAT(actual_mask, EqualsBuffer(expected_mask));
}

// Run the MappedFieldToBcmFieldsTests with bit-shifts of [0-33) (i.e. [0-32]).
INSTANTIATE_TEST_SUITE_P(BcmUdfManagerTest, MappedFieldToBcmFieldsTest,
                        ::testing::Range(0, 33));

// ****************************************************************************
//   Error Tests
// ****************************************************************************

// This case expects a failure to program static UDFs if there are no static
// sets available.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_NoStaticSets) {
  // This field requires 2 bytes (1 UDF chunks at 2-byte chunks).
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 192
    bit_width: 16
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));

  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1).MatchField(1, mapped_field).Build());

  // Create the UDF manager with only controller sets and set up the tables.
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(auto bcm_udf_manager,
                       BcmUdfManager::CreateInstance(
                           &bcm_sdk_interface, udf_spec,
                           /*num_controller_sets=*/udf_spec.set_count(),
                           /*unit=*/1, &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));

  // Create the UDF manager with no sets and set up the tables.
  udf_spec.set_set_count(0);
  ASSERT_OK_AND_ASSIGN(bcm_udf_manager,
                       BcmUdfManager::CreateInstance(
                           &bcm_sdk_interface, udf_spec,
                           /*num_controller_sets=*/udf_spec.set_count(),
                           /*unit=*/1, &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));
}

// This case expects a failure to program static UDFs if a UDF conversion is
// requested on a non-UDF-convertible match field.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_NonConvertibleMatchField) {
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_COLOR
  )PROTO", &mapped_field));

  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1).MatchField(1, mapped_field).Build());

  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/2, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  auto result = bcm_udf_manager->SetUpStaticUdfs(&acl_tables);
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("cannot be converted to UDF")))
      << "Status: " << result;
}

// This case expects a failure to program static UDFs if a match field is too
// large to fit within a UDF set.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_MatchFieldIsTooBig) {
  // Each This field requires 8 bytes (4 UDF chunks at 2-byte chunks). This test
  // uses 3-chunk sets.
  MappedField mapped_field;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 192
    bit_width: 64
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field));

  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1).MatchField(1, mapped_field).Build());

  // Create the UDF manager and set up the tables.
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));
}

// This case expects a failure to program static UDFs if an ACL table
// requires too many UDF chunks.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_AclTableWithTooManyChunks) {
  // Each field requires 4 bytes (2 UDF chunks at 2-byte chunks). This test uses
  // 3 chunks per set.
  MappedField mapped_field_1;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 192
    bit_width: 32
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field_1));

  MappedField mapped_field_2;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ETH_DST
    bit_offset: 0
    bit_width: 32
    header_type: P4_HEADER_ETHERNET
  )PROTO", &mapped_field_2));

  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 1)
                           .MatchField(1, mapped_field_1)
                           .MatchField(2, mapped_field_2)
                           .Build());

  // Create the UDF manager and set up the tables.
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(2);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));
}

// This case expects a failure to program static UDFs if there aren't enough
// chunks for all the ACL tables' static UDFs.
TEST(BcmUdfManagerTest, SetUpStaticUdfs_MultipleAclTablesWithTooManyChunks) {
  // Each field requires 4 bytes (2 UDF chunks at 2-byte chunks). This test uses
  // a single 3-chunk set.
  MappedField mapped_field_1;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ARP_TPA
    bit_offset: 192
    bit_width: 32
    header_type: P4_HEADER_ARP
  )PROTO", &mapped_field_1));

  MappedField mapped_field_2;
  CHECK_OK(ParseProtoFromString(R"PROTO(
    type: P4_FIELD_TYPE_ETH_DST
    bit_offset: 0
    bit_width: 32
    header_type: P4_HEADER_ETHERNET
  )PROTO", &mapped_field_2));

  // The first table will fit. The second table will not fit on top of the first
  // table.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 1)
                           .MatchField(1, mapped_field_1)
                           .Build());
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 2)
                           .MatchField(1, mapped_field_2)
                           .Build());

  // Create the UDF manager and set up the tables.
  BcmSdkMock bcm_sdk_interface;
  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(16);
  udf_spec.set_chunks_per_set(3);
  udf_spec.set_set_count(1);
  ASSERT_OK_AND_ASSIGN(
      auto bcm_udf_manager,
      BcmUdfManager::CreateInstance(&bcm_sdk_interface, udf_spec,
                                    /*num_controller_sets=*/0, /*unit=*/1,
                                    &p4_table_mapper, UdfAllFields));
  EXPECT_THAT(bcm_udf_manager->SetUpStaticUdfs(&acl_tables),
              StatusIs(StratumErrorSpace(), ERR_NO_RESOURCE, _));
}

// This case tests MappedField to BcmFlowEntry conversion when no UDF fields
// were set up.
TEST(BcmUdfManagerTest, MappedFieldToBcmFields_NoStaticUdfs) {
  // Set up 2 ACL tables.
  P4TableMapperMock p4_table_mapper;
  MappedField mapped_field_1 = MappedFieldByType(P4_FIELD_TYPE_ARP_TPA);
  MappedField mapped_field_2 = MappedFieldByType(P4_FIELD_TYPE_ETH_TYPE);
  MappedField mapped_field_3 = MappedFieldByType(P4_FIELD_TYPE_ETH_SRC);
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 1)
                           .MatchField(1, mapped_field_1)
                           .MatchField(2, mapped_field_2)
                           .Build());
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 2)
                           .MatchField(1, mapped_field_1)
                           .MatchField(2, mapped_field_2)
                           .MatchField(3, mapped_field_3)
                           .Build());

  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(32);
  udf_spec.set_chunks_per_set(4);
  udf_spec.set_set_count(2);

  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(auto bcm_udf_manager,
                       BcmUdfManager::CreateInstance(
                           &bcm_sdk_interface, udf_spec,
                           /*num_controller_sets=*/1,
                           /*unit=*/1, &p4_table_mapper, UdfAllFields));

  auto result = bcm_udf_manager->MappedFieldToBcmFields(1, mapped_field_1);
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("is not in UDF set 1")))
      << "Status is: " << result.status();

  result = bcm_udf_manager->MappedFieldToBcmFields(2, mapped_field_2);
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("is not in UDF set 2")))
      << "Status is: " << result.status();

  result = bcm_udf_manager->MappedFieldToBcmFields(2, mapped_field_3);
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("is not in UDF set 2")))
      << "Status is: " << result.status();
}

// This case expects conversion to fail if the set ID is unknown.
TEST(BcmUdfManagerTest, MappedFieldToBcmFields_UnknownUdfSetId) {
  // Set up 2 ACL tables.
  P4TableMapperMock p4_table_mapper;
  MappedField mapped_field_1 = MappedFieldByType(P4_FIELD_TYPE_ARP_TPA);
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(AclTableBuilder(&p4_table_mapper, 1)
                           .MatchField(1, mapped_field_1)
                           .Build());

  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(32);
  udf_spec.set_chunks_per_set(4);
  udf_spec.set_set_count(2);

  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(auto bcm_udf_manager,
                       BcmUdfManager::CreateInstance(
                           &bcm_sdk_interface, udf_spec,
                           /*num_controller_sets=*/1,
                           /*unit=*/1, &p4_table_mapper, UdfAllFields));

  auto result = bcm_udf_manager->MappedFieldToBcmFields(13, mapped_field_1);
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("Unknown UDF set")))
      << "Status: " << result.status();
}

// This case expects conversion to fail if the match field cannot convert to
// UDFs.
TEST(BcmUdfManagerTest, MappedFieldToBcmFields_CannotConvert) {
  // Set up 2 ACL tables.
  P4TableMapperMock p4_table_mapper;
  std::vector<AclTable> acl_tables;
  acl_tables.push_back(
      AclTableBuilder(&p4_table_mapper, 1)
          .MatchField(1, MappedFieldByType(P4_FIELD_TYPE_ARP_TPA))
          .Build());

  UdfSpec udf_spec;
  udf_spec.set_chunk_bits(32);
  udf_spec.set_chunks_per_set(4);
  udf_spec.set_set_count(2);

  BcmSdkMock bcm_sdk_interface;
  ASSERT_OK_AND_ASSIGN(auto bcm_udf_manager,
                       BcmUdfManager::CreateInstance(
                           &bcm_sdk_interface, udf_spec,
                           /*num_controller_sets=*/1,
                           /*unit=*/1, &p4_table_mapper, UdfAllFields));

  auto result = bcm_udf_manager->MappedFieldToBcmFields(
      1, MappedFieldByType(P4_FIELD_TYPE_COLOR));
  EXPECT_THAT(result, StatusIs(StratumErrorSpace(), ERR_INVALID_PARAM,
                               HasSubstr("UDF is not supported")))
      << "Status: " << result.status();
}

}  // namespace
}  // namespace bcm
}  // namespace hal

}  // namespace stratum
