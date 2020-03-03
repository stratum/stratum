// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains MetaKeyMapper unit tests.

#include "stratum/p4c_backends/fpm/meta_key_mapper.h"

#include <string>
#include <vector>

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/match.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ReturnRef;

namespace stratum {
namespace p4c_backends {

class MetaKeyMapperTest : public testing::Test {
 protected:
  // Useful names for test tables and match fields.
  static constexpr const char* kTestTable1 = "test-table-1";
  static constexpr const char* kTestTable2 = "test-table-2";
  static constexpr const char* kTestMetadataKey1 = "metadata1-key-1";
  static constexpr const char* kTestMetadataKey2 = "metadata1-key-2";
  static constexpr const char* kTestMetadataKey3 = "metadata1-key-3";
  static constexpr const char* kTestHeaderField = "header-field";

  // Sets up table and match field P4Info and P4PipelineConfig data for testing.
  // A new P4Info table is created for table_name, with match fields defined
  // in the table for every string in test_match_fields.  Each match field also
  // gets a P4PipelineConfig field descriptor. If the field name starts with
  // "metadata", it is flagged as local metadata in the field descriptor.  The
  // P4Info table and the field descriptors all contain the minimum data
  // needed for testing.
  void SetUpP4Table(const std::string& table_name,
                    const std::vector<std::string>& test_match_fields) {
    auto p4_table = test_p4_info_.add_tables();
    p4_table->mutable_preamble()->set_name(table_name);
    for (const auto& test_match_field : test_match_fields) {
      auto match_field = p4_table->add_match_fields();
      match_field->set_name(test_match_field);
      hal::P4TableMapValue field_value;
      field_value.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_ANNOTATED);
      if (absl::StartsWith(test_match_field, "metadata")) {
        field_value.mutable_field_descriptor()->set_is_local_metadata(true);
      }
      (*test_pipeline_config_.mutable_table_map())[test_match_field] =
          field_value;
    }
  }

  // MetaKeyMapper instance for test use.
  MetaKeyMapper test_metakey_mapper_;

  // SetUpP4Table populates test_p4_info_ and test_pipeline_config_.
  ::p4::config::v1::P4Info test_p4_info_;
  hal::P4PipelineConfig test_pipeline_config_;

  TableMapGeneratorMock mock_table_mapper_;
};

// Matches if and only if exactly one table name in the field descriptor arg's
// metadata_keys is equal to expected_table_name.
MATCHER_P(MatchMetaKeyTable, expected_table_name, "") {
  VLOG(1) << "Matching table " << expected_table_name << " in "
          << arg.DebugString();
  int match_count = 0;
  for (const auto& metadata_key : arg.metadata_keys()) {
    if (expected_table_name == metadata_key.table_name()) ++match_count;
  }
  return match_count == 1;
}

TEST_F(MetaKeyMapperTest, TestNoMetadataMatchFields) {
  SetUpP4Table(kTestTable1, {kTestHeaderField});
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(mock_table_mapper_, ReplaceFieldDescriptor(_, _)).Times(0);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

TEST_F(MetaKeyMapperTest, TestOneMetadataMatchField) {
  SetUpP4Table(kTestTable1, {kTestMetadataKey1});
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey1, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

TEST_F(MetaKeyMapperTest, TestMultipleMetadataMatchFields) {
  SetUpP4Table(kTestTable1, {kTestMetadataKey1, kTestMetadataKey2});
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey1, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey2, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

TEST_F(MetaKeyMapperTest, TestMixedMatchFields) {
  SetUpP4Table(kTestTable1,
               {kTestMetadataKey1, kTestHeaderField, kTestMetadataKey2});
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey1, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey2, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, ReplaceFieldDescriptor(kTestHeaderField, _))
      .Times(0);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

TEST_F(MetaKeyMapperTest, TestSameKeyMultipleTables) {
  SetUpP4Table(kTestTable1,
               {kTestMetadataKey1, kTestHeaderField, kTestMetadataKey2});
  SetUpP4Table(kTestTable2,
               {kTestMetadataKey3, kTestHeaderField, kTestMetadataKey2});
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey1, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey2, MatchMetaKeyTable(kTestTable1)))
      .Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey2, MatchMetaKeyTable(kTestTable2)))
      .Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      ReplaceFieldDescriptor(kTestMetadataKey3, MatchMetaKeyTable(kTestTable2)))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, ReplaceFieldDescriptor(kTestHeaderField, _))
      .Times(0);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

TEST_F(MetaKeyMapperTest, TestMissingFieldDescriptor) {
  SetUpP4Table(kTestTable1, {kTestMetadataKey1});
  test_pipeline_config_.mutable_table_map()->erase(kTestMetadataKey1);
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_pipeline_config_));
  EXPECT_CALL(mock_table_mapper_, ReplaceFieldDescriptor(_, _)).Times(0);
  test_metakey_mapper_.FindMetaKeys(test_p4_info_.tables(),
                                    &mock_table_mapper_);
}

}  // namespace p4c_backends
}  // namespace stratum
