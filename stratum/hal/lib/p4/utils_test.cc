// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


// Unit tests for p4_utils.

#include "stratum/hal/lib/p4/utils.h"

#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gtest/gtest.h"
#include "absl/strings/substitute.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_test_util.h"

using ::testing::HasSubstr;

namespace stratum {
namespace hal {

TEST(PrintP4ObjectIDTest, TestTableID) {
  const int kBaseID = 0x12345;
  const ::p4::config::v1::P4Ids::Prefix kResourceType =
      ::p4::config::v1::P4Ids_Prefix_TABLE;
  const int kObjectId = (kResourceType << 24) + kBaseID;
  const std::string print_id = PrintP4ObjectID(kObjectId);
  EXPECT_THAT(print_id, HasSubstr("0x12345"));
  EXPECT_THAT(print_id,
              HasSubstr(::p4::config::v1::P4Ids::Prefix_Name(kResourceType)));
}

TEST(PrintP4ObjectIDTest, TestActionID) {
  const int kBaseID = 0x54321;
  const ::p4::config::v1::P4Ids::Prefix kResourceType =
      ::p4::config::v1::P4Ids_Prefix_ACTION;
  const int kObjectId = (kResourceType << 24) + kBaseID;
  const std::string print_id = PrintP4ObjectID(kObjectId);
  EXPECT_THAT(print_id, HasSubstr("0x54321"));
  EXPECT_THAT(print_id,
              HasSubstr(::p4::config::v1::P4Ids::Prefix_Name(kResourceType)));
}

TEST(PrintP4ObjectIDTest, TestCounterID) {
  const int kBaseID = 0xabcdef;
  const ::p4::config::v1::P4Ids::Prefix kResourceType =
      ::p4::config::v1::P4Ids_Prefix_COUNTER;
  const int kObjectId = (kResourceType << 24) + kBaseID;
  const std::string print_id = PrintP4ObjectID(kObjectId);
  EXPECT_THAT(print_id, HasSubstr("0xabcdef"));
  EXPECT_THAT(print_id,
              HasSubstr(::p4::config::v1::P4Ids::Prefix_Name(kResourceType)));
}

TEST(PrintP4ObjectIDTest, TestInvalidID) {
  const int kBaseID = 0x54321;
  const int kObjectId =
      ((::p4::config::v1::P4Ids_Prefix_OTHER_EXTERNS_START - 1) << 24) +
       kBaseID;
  const std::string print_id = PrintP4ObjectID(kObjectId);
  EXPECT_THAT(print_id, HasSubstr("0x54321"));
  EXPECT_THAT(print_id, HasSubstr("INVALID"));
}

// This test fixture provides a common P4PipelineConfig for these tests.
class TableMapValueTest : public testing::Test {
 protected:
  // The SetUp method populates test_pipeline_config_ with one descriptor of
  // each type.  Descriptor content is not relevant.
  void SetUp() override {
    P4TableMapValue table_map_value;
    table_map_value.mutable_table_descriptor();
    gtl::InsertOrDie(
        test_pipeline_config_.mutable_table_map(), "table", table_map_value);
    table_map_value.mutable_field_descriptor();
    gtl::InsertOrDie(
        test_pipeline_config_.mutable_table_map(), "field", table_map_value);
    table_map_value.mutable_action_descriptor();
    gtl::InsertOrDie(
        test_pipeline_config_.mutable_table_map(), "action", table_map_value);
    table_map_value.mutable_header_descriptor();
    gtl::InsertOrDie(
        test_pipeline_config_.mutable_table_map(), "header", table_map_value);
    table_map_value.mutable_internal_action();
    gtl::InsertOrDie(
        test_pipeline_config_.mutable_table_map(), "internal", table_map_value);
  }

  P4PipelineConfig test_pipeline_config_;
};

TEST_F(TableMapValueTest, FindTableDescriptor) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "table", P4TableMapValue::kTableDescriptor, "");
  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie()->has_table_descriptor());
}

TEST_F(TableMapValueTest, FindFieldDescriptor) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "field", P4TableMapValue::kFieldDescriptor, "");
  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie()->has_field_descriptor());
}

TEST_F(TableMapValueTest, FindActionDescriptor) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "action", P4TableMapValue::kActionDescriptor, "");
  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie()->has_action_descriptor());
}

TEST_F(TableMapValueTest, FindHeaderDescriptor) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "header", P4TableMapValue::kHeaderDescriptor, "");
  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie()->has_header_descriptor());
}

TEST_F(TableMapValueTest, FindInternalActionDescriptor) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "internal", P4TableMapValue::kInternalAction, "");
  EXPECT_OK(status);
  EXPECT_TRUE(status.ValueOrDie()->has_internal_action());
}

TEST_F(TableMapValueTest, FindFailDescriptorNotSet) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "table", P4TableMapValue::DESCRIPTOR_NOT_SET, "");
  EXPECT_FALSE(status.ok());
}

TEST_F(TableMapValueTest, FindFailNoKeyMatch) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "bad-key", P4TableMapValue::kTableDescriptor, "");
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().error_message(),
              HasSubstr("table map has no descriptor"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("bad-key"));
}

TEST_F(TableMapValueTest, FindFailNoKeyMatchWithLogObject) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "bad-key", P4TableMapValue::kFieldDescriptor,
      "log-table");
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().error_message(),
              HasSubstr("table map has no descriptor"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("bad-key"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("referenced by P4"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("log-table"));
}

TEST_F(TableMapValueTest, FindFailValueWithWrongDescriptorCase) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "field", P4TableMapValue::kTableDescriptor, "");
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().error_message(),
              HasSubstr("does not have the expected descriptor case"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("field"));
}

TEST_F(TableMapValueTest, FindFailValueWithWrongDescriptorCaseWithLogObject) {
  auto status = GetTableMapValueWithDescriptorCase(
      test_pipeline_config_, "field", P4TableMapValue::kTableDescriptor,
      "p4-object");
  EXPECT_FALSE(status.ok());
  EXPECT_THAT(status.status().error_message(),
              HasSubstr("does not have the expected descriptor case"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("field"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("referenced by P4"));
  EXPECT_THAT(status.status().error_message(), HasSubstr("p4-object"));
}

}  // namespace hal
}  // namespace stratum
