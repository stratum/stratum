// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for TableMapGenerator.

#include "stratum/p4c_backends/fpm/table_map_generator.h"

#include <string>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "gtest/gtest.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"
#include "p4/config/v1/p4info.pb.h"

namespace stratum {
namespace p4c_backends {

class TableMapGeneratorTest : public testing::Test {
 protected:
  const std::string kTestFieldName = "test-field";
  const std::string kTestFieldName2 = "test-field2";
  const std::string kTestFieldName3 = "test-field3";
  const std::string kTestFieldName4 = "test-field4";
  const std::string kTestFieldName5 = "test-field5";
  const std::string kTestActionName = "test-action";
  const std::string kTestActionParamName = "test-action-param";
  const std::string kTestActionParamName2 = "test-action-param2";
  const std::string kTestTableName = "test-table";
  const std::string kTestHeaderName = "test-header";
  const std::string kTestHeaderName2 = "test-header2";

  TableMapGeneratorTest()
      : test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {}

  // Sets up test data for testing actions based on meter colors.
  void SetUpTestColorAction(
      const std::vector<P4MeterColor>& colors,
      const std::vector<P4ActionOp>& primitives,
      hal::P4ActionDescriptor::P4MeterColorAction* test_color_action) {
    for (auto color : colors)
      test_color_action->add_colors(color);
    for (auto primitive : primitives)
      test_color_action->add_ops()->add_primitives(primitive);
  }

  TableMapGenerator map_generator_;

  // This test uses its own p4c context since it doesn't need IRTestHelperJson.
  AutoCompileContext test_p4c_context_;
};

TEST_F(TableMapGeneratorTest, TestEmptyMap) {
  EXPECT_TRUE(map_generator_.generated_map().table_map().empty());
}

// Tests adding a new field.
TEST_F(TableMapGeneratorTest, TestAddField) {
  map_generator_.AddField(kTestFieldName);
  EXPECT_FALSE(map_generator_.generated_map().table_map().empty());
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_EQ(kTestFieldName, iter->first);
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ANNOTATED, iter->second.field_descriptor().type());
  EXPECT_EQ(0, iter->second.field_descriptor().valid_conversions_size());
}

// Tests setting type of field for an existing field.
TEST_F(TableMapGeneratorTest, TestSetFieldType) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_ETH_SRC);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_SRC, iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_UNKNOWN, iter->second.field_descriptor().header_type());
}

// Tests setting type of field for an undefined field.
TEST_F(TableMapGeneratorTest, TestSetFieldTypeUndefined) {
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_ETH_SRC);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Verifies that a known field type is not replaced by P4_FIELD_TYPE_UNKNOWN.
TEST_F(TableMapGeneratorTest, TestSetFieldTypeKnownToUnknown) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_ETH_DST);
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_UNKNOWN);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, iter->second.field_descriptor().type());
}

// Verifies that a known field type replaces a previous known field type.
TEST_F(TableMapGeneratorTest, TestSetFieldTypeKnownToKnown) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_ETH_DST);
  map_generator_.SetFieldType(kTestFieldName, P4_FIELD_TYPE_ETH_SRC);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_SRC, iter->second.field_descriptor().type());
}

// Tests setting field type, header type, offset, and width for an
// existing field.
TEST_F(TableMapGeneratorTest, TestSetFieldAttributes) {
  const uint32_t kTestOffset = 96;
  const uint32_t kTestWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_TYPE, P4_HEADER_ETHERNET,
       kTestOffset, kTestWidth);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_TYPE, iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_ETHERNET, iter->second.field_descriptor().header_type());
  EXPECT_EQ(kTestOffset, iter->second.field_descriptor().bit_offset());
  EXPECT_EQ(kTestWidth, iter->second.field_descriptor().bit_width());
}

// Tests setting field type, offset, and width for an undefined field.
TEST_F(TableMapGeneratorTest, TestSetFieldAttributesUndefined) {
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_SRC, P4_HEADER_ETHERNET, 48, 48);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests replacing bit offset and width for an existing field.
TEST_F(TableMapGeneratorTest, TestReplaceOffsetWidth) {
  const uint32_t kTestOffset = 96;
  const uint32_t kTestWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_TYPE, P4_HEADER_ETHERNET,
      kTestOffset, kTestWidth);
  const uint32_t kNewTestOffset = 48;
  const uint32_t kNewTestWidth = 48;
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_TYPE, P4_HEADER_ETHERNET,
      kNewTestOffset, kNewTestWidth);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_TYPE, iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_ETHERNET, iter->second.field_descriptor().header_type());
  EXPECT_EQ(kNewTestOffset, iter->second.field_descriptor().bit_offset());
  EXPECT_EQ(kNewTestWidth, iter->second.field_descriptor().bit_width());
}

// Verifies that a known header type is not replaced by P4_HEADER_UNKNOWN.
TEST_F(TableMapGeneratorTest, TestSetFieldHeaderTypeKnownToUnknown) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_DST, P4_HEADER_ETHERNET, 48, 48);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_DST, P4_HEADER_UNKNOWN, 48, 48);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_ETHERNET, iter->second.field_descriptor().header_type());
}

// Verifies that a known header type replaces a previous known header type.
TEST_F(TableMapGeneratorTest, TestSetFieldHeaderTypeKnownToKnown) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_DST, P4_HEADER_ARP, 48, 48);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_DST, P4_HEADER_ETHERNET, 48, 48);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(P4_FIELD_TYPE_ETH_DST, iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_ETHERNET, iter->second.field_descriptor().header_type());
}

// Tests setting local metadata flag for an existing field.
TEST_F(TableMapGeneratorTest, TestSetFieldLocalMeta) {
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldLocalMetadataFlag(kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_TRUE(iter->second.field_descriptor().is_local_metadata());
}

// Tests setting local metadata flag for an undefined field.
TEST_F(TableMapGeneratorTest, TestSetFieldLocalMetaUndefined) {
  map_generator_.SetFieldLocalMetadataFlag(kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests setting value set attributes in an existing field.
TEST_F(TableMapGeneratorTest, TestSetFieldValueSet) {
  map_generator_.AddField(kTestFieldName);
  const std::string kValueSetName = "test-value-set";
  map_generator_.SetFieldValueSet(
      kTestFieldName, kValueSetName, P4_HEADER_UDP_PAYLOAD);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(kValueSetName, iter->second.field_descriptor().value_set());
  EXPECT_EQ(P4_FIELD_TYPE_UDF_VALUE_SET,
            iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_UDP_PAYLOAD,
            iter->second.field_descriptor().header_type());
}

// Tests overwriting value set attributes in an existing field.
TEST_F(TableMapGeneratorTest, TestOverwriteFieldValueSet) {
  map_generator_.AddField(kTestFieldName);
  const std::string kValueSetName = "test-value-set";
  map_generator_.SetFieldValueSet(
      kTestFieldName, kValueSetName, P4_HEADER_UDP_PAYLOAD);
  const std::string kValueSetNameUpdate = "test-value-set-2";
  map_generator_.SetFieldValueSet(
      kTestFieldName, kValueSetNameUpdate, P4_HEADER_TCP);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(kValueSetNameUpdate, iter->second.field_descriptor().value_set());
  EXPECT_EQ(P4_FIELD_TYPE_UDF_VALUE_SET,
            iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_TCP, iter->second.field_descriptor().header_type());
}

// Tests setting value set attributes does not disturb unaffected field data.
// Tests replacing bit offset and width for an existing field.
TEST_F(TableMapGeneratorTest, TestSetValueSetSideEffects) {
  const uint32_t kTestOffset = 96;
  const uint32_t kTestWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_ETH_TYPE, P4_HEADER_ETHERNET,
      kTestOffset, kTestWidth);
  const std::string kValueSetName = "test-value-set";
  map_generator_.SetFieldValueSet(
      kTestFieldName, kValueSetName, P4_HEADER_UDP_PAYLOAD);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_field_descriptor());
  EXPECT_EQ(kValueSetName, iter->second.field_descriptor().value_set());
  EXPECT_EQ(P4_FIELD_TYPE_UDF_VALUE_SET,
            iter->second.field_descriptor().type());
  EXPECT_EQ(P4_HEADER_UDP_PAYLOAD,
            iter->second.field_descriptor().header_type());
  EXPECT_EQ(kTestOffset, iter->second.field_descriptor().bit_offset());
  EXPECT_EQ(kTestWidth, iter->second.field_descriptor().bit_width());
}

// Tests setting value set for an undefined field.
TEST_F(TableMapGeneratorTest, TestSetFieldValueSetUndefined) {
  map_generator_.SetFieldValueSet(
      kTestFieldName, "test-value-set", P4_HEADER_UDP_PAYLOAD);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// TODO(unknown): Parameterize the next two tests to cover many combinations
// of match type and field width.
TEST_F(TableMapGeneratorTest, TestAddFieldMatchExact) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_UNKNOWN, P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());

  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);
  ASSERT_EQ(1, field_descriptor->valid_conversions_size());
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
}

TEST_F(TableMapGeneratorTest, TestAddFieldMatchLpm) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_UNKNOWN, P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().lpm_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());
  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);
  ASSERT_EQ(1, field_descriptor->valid_conversions_size());
  EXPECT_EQ(::p4::config::v1::MatchField::LPM,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
}

TEST_F(TableMapGeneratorTest, TestAddFieldMatchRange) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(kTestFieldName, P4_FIELD_TYPE_UNKNOWN,
                                    P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(kTestFieldName, GetP4ModelNames().range_match(),
                               kMatchWidth);

  // This should produce a program error since the Stratum switch stack does
  // not currently support range matches.
  EXPECT_NE(0, ::errorCount());
}

// Verifies that adding the same field name does not disturb the
// existing field_descriptor.
TEST_F(TableMapGeneratorTest, TestAddFieldAgain) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().lpm_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());

  // The line below adds the same field again.
  map_generator_.AddField(kTestFieldName);
  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);
  ASSERT_EQ(1, field_descriptor->valid_conversions_size());
  EXPECT_EQ(::p4::config::v1::MatchField::LPM,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
}

// Verifies multiple match type uses for the same field.
TEST_F(TableMapGeneratorTest, TestAddFieldMultiMatch) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_UNKNOWN, P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().lpm_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());
  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);
  ASSERT_EQ(2, field_descriptor->valid_conversions_size());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
  EXPECT_EQ(::p4::config::v1::MatchField::LPM,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            field_descriptor->valid_conversions(1).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32,
            field_descriptor->valid_conversions(1).conversion());
}

// Verifies uses of the same field with different bit widths for the same
// match type.
TEST_F(TableMapGeneratorTest, TestAddFieldSameMatchTypeMultiBitWidth) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_UNKNOWN, P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth + 1);
  EXPECT_EQ(0, ::errorCount());
  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);

  // The field descriptor should keep the first width and reject the second.
  ASSERT_EQ(1, field_descriptor->valid_conversions_size());
  EXPECT_EQ(::p4::config::v1::MatchField::EXACT,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
}

// Verifies uses of the same field with different bit widths for different
// match types.
TEST_F(TableMapGeneratorTest, TestAddFieldDifferentMatchTypeMultiBitWidth) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddField(kTestFieldName);
  map_generator_.SetFieldAttributes(
      kTestFieldName, P4_FIELD_TYPE_UNKNOWN, P4_HEADER_UNKNOWN, 0, kMatchWidth);
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().lpm_match(), kMatchWidth);
  EXPECT_EQ(0, ::errorCount());
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth + 1);
  EXPECT_EQ(0, ::errorCount());
  const auto field_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(field_descriptor != nullptr);

  // The field descriptor should keep the first width and reject the second.
  ASSERT_EQ(1, field_descriptor->valid_conversions_size());
  EXPECT_EQ(::p4::config::v1::MatchField::LPM,
            field_descriptor->valid_conversions(0).match_type());
  EXPECT_EQ(hal::P4FieldDescriptor::P4_CONVERT_TO_U32_AND_MASK,
            field_descriptor->valid_conversions(0).conversion());
  EXPECT_EQ(kMatchWidth, field_descriptor->bit_width());
}

// Verifies adding a match for an undefined field.
TEST_F(TableMapGeneratorTest, TestAddFieldMatchUndefined) {
  SetUpTestP4ModelNames();
  const int kMatchWidth = 16;
  map_generator_.AddFieldMatch(
      kTestFieldName, GetP4ModelNames().exact_match(), kMatchWidth);
  auto iter = map_generator_.generated_map().table_map().find(kTestFieldName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests replacement of an existing field descriptor.
TEST_F(TableMapGeneratorTest, TestReplaceFieldDescriptor) {
  map_generator_.AddField(kTestFieldName);
  hal::P4FieldDescriptor new_descriptor;
  new_descriptor.set_type(P4_FIELD_TYPE_IPV4_DST);
  new_descriptor.set_is_local_metadata(true);
  new_descriptor.set_bit_width(32);
  new_descriptor.add_metadata_keys()->set_table_name("dummy-table");
  map_generator_.ReplaceFieldDescriptor(kTestFieldName, new_descriptor);

  const hal::P4FieldDescriptor* replaced_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  ASSERT_TRUE(replaced_descriptor != nullptr);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(new_descriptor,
                                                         *replaced_descriptor));
}

// Tests field descriptor replacement with undefined field.
TEST_F(TableMapGeneratorTest, TestReplaceUndefinedFieldDescriptor) {
  hal::P4FieldDescriptor new_descriptor;
  new_descriptor.set_type(P4_FIELD_TYPE_IPV4_DST);
  new_descriptor.set_is_local_metadata(true);
  new_descriptor.set_bit_width(32);
  new_descriptor.add_metadata_keys()->set_table_name("dummy-table");
  map_generator_.ReplaceFieldDescriptor(kTestFieldName, new_descriptor);

  const hal::P4FieldDescriptor* replaced_descriptor =
      FindFieldDescriptorOrNull(kTestFieldName, map_generator_.generated_map());
  EXPECT_TRUE(replaced_descriptor == nullptr);
}

// Tests adding a new action.
TEST_F(TableMapGeneratorTest, TestAddAction) {
  map_generator_.AddAction(kTestActionName);
  EXPECT_FALSE(map_generator_.generated_map().table_map().empty());
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_EQ(kTestActionName, iter->first);
  EXPECT_TRUE(iter->second.has_action_descriptor());
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, iter->second.action_descriptor().type());
  EXPECT_EQ(0, iter->second.action_descriptor().assignments_size());
  EXPECT_EQ(0, iter->second.action_descriptor().primitive_ops_size());
}

// Tests action parameter assignment to field.
TEST_F(TableMapGeneratorTest, TestActionAssignParameterField) {
  map_generator_.AddAction(kTestActionName);
  map_generator_.AssignActionParameterToField(
      kTestActionName, kTestActionParamName, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.assignments_size());
  const auto& param_descriptor = action_descriptor.assignments(0);
  EXPECT_EQ(kTestFieldName, param_descriptor.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kParameterName,
            param_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(kTestActionParamName,
            param_descriptor.assigned_value().parameter_name());
}

// Tests action constant assignment to field.
TEST_F(TableMapGeneratorTest, TestActionAssignConstantField) {
  map_generator_.AddAction(kTestActionName);
  const int64_t kTestConstant = 0xf00f00f00f00;
  P4AssignSourceValue source_value;
  source_value.set_constant_param(kTestConstant);
  source_value.set_bit_width(48);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.assignments_size());
  const auto& param_descriptor = action_descriptor.assignments(0);
  EXPECT_EQ(kTestFieldName, param_descriptor.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kConstantParam,
            param_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(kTestConstant, param_descriptor.assigned_value().constant_param());
  EXPECT_EQ(source_value.bit_width(),
            param_descriptor.assigned_value().bit_width());
}

// Tests action field assignment to another field.
TEST_F(TableMapGeneratorTest, TestActionAssignFieldToField) {
  map_generator_.AddAction(kTestActionName);
  P4AssignSourceValue source_value;
  source_value.set_source_field_name(kTestFieldName2);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.assignments_size());
  const auto& param_descriptor = action_descriptor.assignments(0);
  EXPECT_EQ(kTestFieldName, param_descriptor.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kSourceFieldName,
            param_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(kTestFieldName2,
            param_descriptor.assigned_value().source_field_name());
}

// Tests action header-to-header copy.
TEST_F(TableMapGeneratorTest, TestActionCopyHeaderToHeader) {
  map_generator_.AddAction(kTestActionName);
  P4AssignSourceValue source_value;
  source_value.set_source_header_name("source-header");
  map_generator_.AssignHeaderToHeader(
      kTestActionName, source_value, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.assignments_size());
  const auto& copy_descriptor = action_descriptor.assignments(0);
  EXPECT_EQ(kTestFieldName, copy_descriptor.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kSourceHeaderName,
            copy_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(source_value.source_header_name(),
            copy_descriptor.assigned_value().source_header_name());
}

// Tests action copy of the same header to multiple destination headers.
TEST_F(TableMapGeneratorTest, TestActionCopyHeaderToMultipleHeaders) {
  map_generator_.AddAction(kTestActionName);
  P4AssignSourceValue source_value;
  source_value.set_source_header_name("source-header");
  map_generator_.AssignHeaderToHeader(
      kTestActionName, source_value, kTestFieldName);
  map_generator_.AssignHeaderToHeader(
      kTestActionName, source_value, kTestFieldName2);

  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.assignments_size());
  const auto& copy_descriptor0 = action_descriptor.assignments(0);
  EXPECT_EQ(kTestFieldName, copy_descriptor0.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kSourceHeaderName,
            copy_descriptor0.assigned_value().source_value_case());
  EXPECT_EQ(source_value.source_header_name(),
            copy_descriptor0.assigned_value().source_header_name());

  const auto& copy_descriptor1 = action_descriptor.assignments(1);
  EXPECT_EQ(kTestFieldName2, copy_descriptor1.destination_field_name());
  EXPECT_EQ(P4AssignSourceValue::kSourceHeaderName,
            copy_descriptor1.assigned_value().source_value_case());
  EXPECT_EQ(source_value.source_header_name(),
            copy_descriptor1.assigned_value().source_header_name());
}

// Tests drop action.
TEST_F(TableMapGeneratorTest, TestActionDrop) {
  map_generator_.AddAction(kTestActionName);
  map_generator_.AddDropPrimitive(kTestActionName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  ASSERT_EQ(1, action_descriptor.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_DROP, action_descriptor.primitive_ops(0));
}

// Tests nop action.
TEST_F(TableMapGeneratorTest, TestActionNop) {
  map_generator_.AddAction(kTestActionName);
  map_generator_.AddNopPrimitive(kTestActionName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  ASSERT_EQ(1, action_descriptor.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_NOP, action_descriptor.primitive_ops(0));
}

// Tests addition of one meter color action.
TEST_F(TableMapGeneratorTest, TestActionMeterColor) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4MeterColorAction test_color_action;
  SetUpTestColorAction(
      {P4_METER_GREEN}, {P4_ACTION_OP_CLONE}, &test_color_action);
  map_generator_.AddMeterColorAction(kTestActionName, test_color_action);

  // The test_color_action should appear as the only color_actions entry
  // in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.color_actions_size());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      test_color_action, action_descriptor.color_actions(0)));
}

// Tests addition of multiple meter color actions with disjoint color sets.
TEST_F(TableMapGeneratorTest, TestActionMeterDisjointColors) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4MeterColorAction test_green;
  SetUpTestColorAction(
      {P4_METER_GREEN}, {P4_ACTION_OP_CLONE}, &test_green);
  hal::P4ActionDescriptor::P4MeterColorAction test_red_yellow;
  SetUpTestColorAction(
      {P4_METER_RED, P4_METER_YELLOW}, {P4_ACTION_OP_DROP}, &test_red_yellow);
  map_generator_.AddMeterColorAction(kTestActionName, test_green);
  map_generator_.AddMeterColorAction(kTestActionName, test_red_yellow);

  // The two test color actions should appear as separate color_actions entries
  // in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.color_actions_size());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      test_green, action_descriptor.color_actions(0)));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      test_red_yellow, action_descriptor.color_actions(1)));
}

// Tests addition of multiple meter color actions with partially
// overlapping color sets.
TEST_F(TableMapGeneratorTest, TestActionMeterPartialColorsOverlap) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4MeterColorAction test_green_yellow;
  SetUpTestColorAction({P4_METER_GREEN, P4_METER_YELLOW},
                       {P4_ACTION_OP_CLONE}, &test_green_yellow);
  hal::P4ActionDescriptor::P4MeterColorAction test_red_yellow;
  SetUpTestColorAction(
      {P4_METER_RED, P4_METER_YELLOW}, {P4_ACTION_OP_DROP}, &test_red_yellow);
  map_generator_.AddMeterColorAction(kTestActionName, test_green_yellow);
  map_generator_.AddMeterColorAction(kTestActionName, test_red_yellow);

  // The two test color actions should appear as separate color_actions entries
  // in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.color_actions_size());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      test_green_yellow, action_descriptor.color_actions(0)));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      test_red_yellow, action_descriptor.color_actions(1)));
}

// Tests addition of multiple meter color actions with fully
// overlapping color sets.
// different target ports.
TEST_F(TableMapGeneratorTest, TestActionMeterFullColorsOverlap) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4MeterColorAction green_clone1;
  SetUpTestColorAction({P4_METER_GREEN, P4_METER_YELLOW},
                       {P4_ACTION_OP_CLONE}, &green_clone1);
  green_clone1.mutable_ops(0)->mutable_assigned_value()->set_constant_param(1);
  hal::P4ActionDescriptor::P4MeterColorAction green_clone2;
  SetUpTestColorAction({P4_METER_YELLOW, P4_METER_GREEN},
                       {P4_ACTION_OP_CLONE}, &green_clone2);
  green_clone2.mutable_ops(0)->mutable_assigned_value()->set_constant_param(3);
  map_generator_.AddMeterColorAction(kTestActionName, green_clone1);
  map_generator_.AddMeterColorAction(kTestActionName, green_clone2);

  // The two test color actions should appear as joined color_actions entries
  // in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.color_actions_size());
  const hal::P4ActionDescriptor::P4MeterColorAction& map_color_action =
      action_descriptor.color_actions(0);
  ASSERT_EQ(2, map_color_action.colors_size());
  EXPECT_EQ(P4_METER_GREEN, map_color_action.colors(0));
  EXPECT_EQ(P4_METER_YELLOW, map_color_action.colors(1));
  ASSERT_EQ(2, map_color_action.ops_size());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      green_clone1.ops(0), map_color_action.ops(0)));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      green_clone2.ops(0), map_color_action.ops(1)));
}

// Tests addition of multiple meter color actions via
// AddMeterColorActionsFromString.
TEST_F(TableMapGeneratorTest, TestAddMeterColorActionsFromString) {
  // This is the same setup as TestActionMeterDisjointColors, but the test
  // values are converted to text format before calling the tested method.
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4MeterColorAction test_green;
  SetUpTestColorAction(
      {P4_METER_GREEN}, {P4_ACTION_OP_CLONE}, &test_green);
  hal::P4ActionDescriptor::P4MeterColorAction test_red_yellow;
  SetUpTestColorAction(
      {P4_METER_RED, P4_METER_YELLOW}, {P4_ACTION_OP_DROP}, &test_red_yellow);
  hal::P4ActionDescriptor color_actions_message;
  *(color_actions_message.add_color_actions()) = test_green;
  *(color_actions_message.add_color_actions()) = test_red_yellow;
  std::string color_actions_text;
  ASSERT_TRUE(
      PrintProtoToString(color_actions_message, &color_actions_text).ok());
  map_generator_.AddMeterColorActionsFromString(
      kTestActionName, color_actions_text);

  // The updated action descriptor should match the input color_actions_message
  // adjusted for the basic descriptor settings.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  color_actions_message.set_type(P4_ACTION_TYPE_FUNCTION);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      color_actions_message, action_descriptor));
}

// Tests addition of one tunnel action.
TEST_F(TableMapGeneratorTest, TestActionTunnelEncap) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4TunnelAction test_tunnel_action;
  test_tunnel_action.set_header_op(P4_HEADER_SET_VALID);
  test_tunnel_action.set_header_name("encap-header");
  map_generator_.AddTunnelAction(kTestActionName, test_tunnel_action);

  // The test_tunnel_action should appear as the only tunnel_actions entry
  // in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(1, action_descriptor.tunnel_actions_size());
  const hal::P4ActionDescriptor::P4TunnelAction& tunnel0 =
      action_descriptor.tunnel_actions(0);
  EXPECT_EQ(test_tunnel_action.tunnel_op(), tunnel0.tunnel_op());
  EXPECT_EQ(test_tunnel_action.header_name(), tunnel0.header_name());
}

// Tests addition of multiple tunnel actions.
TEST_F(TableMapGeneratorTest, TestActionTunnelDecapEncap) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4TunnelAction test_decap_action;
  test_decap_action.set_header_op(P4_HEADER_SET_INVALID);
  test_decap_action.set_header_name("decap-header");
  map_generator_.AddTunnelAction(kTestActionName, test_decap_action);
  hal::P4ActionDescriptor::P4TunnelAction test_encap_action;
  test_encap_action.set_header_op(P4_HEADER_SET_VALID);
  test_encap_action.set_header_name("encap-header");
  map_generator_.AddTunnelAction(kTestActionName, test_encap_action);

  // The encap and decap actions should both appear in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.tunnel_actions_size());
  const hal::P4ActionDescriptor::P4TunnelAction& decap_tunnel =
      action_descriptor.tunnel_actions(0);
  EXPECT_EQ(test_decap_action.tunnel_op(), decap_tunnel.tunnel_op());
  EXPECT_EQ(test_decap_action.header_name(), decap_tunnel.header_name());
  const hal::P4ActionDescriptor::P4TunnelAction& encap_tunnel =
      action_descriptor.tunnel_actions(1);
  EXPECT_EQ(test_encap_action.tunnel_op(), encap_tunnel.tunnel_op());
  EXPECT_EQ(test_encap_action.header_name(), encap_tunnel.header_name());
}

// Tests repetition of the same tunnel action.
TEST_F(TableMapGeneratorTest, TestActionTunnelRepeatedDecap) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor::P4TunnelAction test_decap_action;
  test_decap_action.set_header_op(P4_HEADER_SET_INVALID);
  test_decap_action.set_header_name("decap-header");
  map_generator_.AddTunnelAction(kTestActionName, test_decap_action);
  map_generator_.AddTunnelAction(kTestActionName, test_decap_action);

  // Both copies of the decap action should  appear in the action descriptor.
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const hal::P4ActionDescriptor& action_descriptor =
      iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.tunnel_actions_size());
  const hal::P4ActionDescriptor::P4TunnelAction& decap_tunnel0 =
      action_descriptor.tunnel_actions(0);
  EXPECT_EQ(test_decap_action.tunnel_op(), decap_tunnel0.tunnel_op());
  EXPECT_EQ(test_decap_action.header_name(), decap_tunnel0.header_name());
  const hal::P4ActionDescriptor::P4TunnelAction& decap_tunnel1 =
      action_descriptor.tunnel_actions(1);
  EXPECT_EQ(test_decap_action.tunnel_op(), decap_tunnel1.tunnel_op());
  EXPECT_EQ(test_decap_action.header_name(), decap_tunnel1.header_name());
}

// Tests replacement of an action descriptor.
TEST_F(TableMapGeneratorTest, TestReplaceActionDescriptor) {
  map_generator_.AddAction(kTestActionName);
  hal::P4ActionDescriptor new_descriptor;
  new_descriptor.set_type(P4_ACTION_TYPE_FUNCTION);
  new_descriptor.add_primitive_ops(P4_ACTION_OP_DROP);
  new_descriptor.add_primitive_ops(P4_ACTION_OP_CLONE);
  map_generator_.ReplaceActionDescriptor(kTestActionName, new_descriptor);

  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      new_descriptor, iter->second.action_descriptor()));
}

// Verifies that adding the same action name does not disturb the
// existing action_descriptor.
TEST_F(TableMapGeneratorTest, TestAddActionAgain) {
  map_generator_.AddAction(kTestActionName);
  map_generator_.AddNopPrimitive(kTestActionName);

  // The line below adds the same action again.
  map_generator_.AddAction(kTestActionName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.assignments_size());
  ASSERT_EQ(1, action_descriptor.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_NOP, action_descriptor.primitive_ops(0));
}

// Tests a complex action with multiple assignments.
TEST_F(TableMapGeneratorTest, TestActionMultipleAssign) {
  map_generator_.AddAction(kTestActionName);
  const int64_t kTestConstant = 0x5a5a5a5a5a5a5a5a;
  P4AssignSourceValue constant_source_value;
  constant_source_value.set_constant_param(kTestConstant);
  constant_source_value.set_bit_width(64);

  // The test assigns the first action parameter to fields kTestFieldName and
  // kTestFieldName4, a constant to kTestFieldName2 and kTestFieldName5, and
  // the second action parameter to kTestFieldName3.
  map_generator_.AssignActionParameterToField(
      kTestActionName, kTestActionParamName, kTestFieldName);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, constant_source_value, kTestFieldName2);
  map_generator_.AssignActionParameterToField(
      kTestActionName, kTestActionParamName2, kTestFieldName3);
  map_generator_.AssignActionParameterToField(
      kTestActionName, kTestActionParamName, kTestFieldName4);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, constant_source_value, kTestFieldName5);

  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(5, action_descriptor.assignments_size());

  // The first assignment represents kTestFieldName = kTestActionParamName.
  const auto& assignment0 = action_descriptor.assignments(0);
  EXPECT_EQ(P4AssignSourceValue::kParameterName,
            assignment0.assigned_value().source_value_case());
  EXPECT_EQ(kTestActionParamName,
            assignment0.assigned_value().parameter_name());
  EXPECT_EQ(kTestFieldName, assignment0.destination_field_name());

  // The second assignment represents kTestFieldName2 = <constant>.
  const auto& assignment1 = action_descriptor.assignments(1);
  EXPECT_EQ(P4AssignSourceValue::kConstantParam,
            assignment1.assigned_value().source_value_case());
  EXPECT_EQ(kTestConstant, assignment1.assigned_value().constant_param());
  EXPECT_EQ(kTestFieldName2, assignment1.destination_field_name());

  // The third assignment represents kTestFieldName3 = kTestActionParamName2.
  const auto& assignment2 = action_descriptor.assignments(2);
  EXPECT_EQ(P4AssignSourceValue::kParameterName,
            assignment2.assigned_value().source_value_case());
  EXPECT_EQ(kTestActionParamName2,
            assignment2.assigned_value().parameter_name());
  EXPECT_EQ(kTestFieldName3, assignment2.destination_field_name());

  // The fourth assignment represents kTestFieldName4 = kTestActionParamName.
  const auto& assignment3 = action_descriptor.assignments(3);
  EXPECT_EQ(P4AssignSourceValue::kParameterName,
            assignment3.assigned_value().source_value_case());
  EXPECT_EQ(kTestActionParamName,
            assignment3.assigned_value().parameter_name());
  EXPECT_EQ(kTestFieldName4, assignment3.destination_field_name());

  // The fifth assignment represents kTestFieldName5 = <constant>.
  const auto& assignment4 = action_descriptor.assignments(4);
  EXPECT_EQ(P4AssignSourceValue::kConstantParam,
            assignment4.assigned_value().source_value_case());
  EXPECT_EQ(kTestConstant, assignment4.assigned_value().constant_param());
  EXPECT_EQ(kTestFieldName5, assignment4.destination_field_name());
}

// Tests an action that assigns the same constant to different width fields.
TEST_F(TableMapGeneratorTest, TestActionAssignMultiWidthConstant) {
  map_generator_.AddAction(kTestActionName);
  const int64_t kTestConstant = 0xf00f;
  P4AssignSourceValue source_value1;
  source_value1.set_constant_param(kTestConstant);
  source_value1.set_bit_width(24);
  P4AssignSourceValue source_value2;
  source_value2.set_constant_param(kTestConstant);
  source_value2.set_bit_width(16);

  // The test assigns the same constant to two different fields with different
  // bit widths.  Each should appear as a distinct assignment in the
  // action descriptor.
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value1, kTestFieldName);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value2, kTestFieldName2);

  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  ASSERT_EQ(2, action_descriptor.assignments_size());

  // The expected pair of assignments targets two fields of different widths.
  const auto& param1_descriptor = action_descriptor.assignments(0);
  EXPECT_EQ(P4AssignSourceValue::kConstantParam,
            param1_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(kTestConstant,
            param1_descriptor.assigned_value().constant_param());
  EXPECT_EQ(source_value1.bit_width(),
            param1_descriptor.assigned_value().bit_width());
  EXPECT_EQ(kTestFieldName, param1_descriptor.destination_field_name());
  const auto& param2_descriptor = action_descriptor.assignments(1);
  EXPECT_EQ(P4AssignSourceValue::kConstantParam,
            param2_descriptor.assigned_value().source_value_case());
  EXPECT_EQ(kTestConstant,
            param2_descriptor.assigned_value().constant_param());
  EXPECT_EQ(source_value2.bit_width(),
            param2_descriptor.assigned_value().bit_width());
  EXPECT_EQ(kTestFieldName2, param2_descriptor.destination_field_name());
}

// Tests action assignment with source value not set.
TEST_F(TableMapGeneratorTest, TestActionAssignNoSourceValue) {
  map_generator_.AddAction(kTestActionName);
  P4AssignSourceValue source_value;
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  EXPECT_EQ(0, action_descriptor.assignments_size());  // No assignments added.
}

// Tests action assignment with one field assigned to two destination fields.
TEST_F(TableMapGeneratorTest, TestActionAssignFieldToMultipleFields) {
  map_generator_.AddAction(kTestActionName);
  P4AssignSourceValue source_value;
  source_value.set_source_field_name(kTestFieldName3);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName2);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  EXPECT_EQ(0, action_descriptor.primitive_ops_size());
  EXPECT_EQ(2, action_descriptor.assignments_size());

  // The first assignment is for kTestFieldName = kTestFieldName3.
  const auto& assignment0 = action_descriptor.assignments(0);
  EXPECT_EQ(P4AssignSourceValue::kSourceFieldName,
            assignment0.assigned_value().source_value_case());
  EXPECT_EQ(kTestFieldName3, assignment0.assigned_value().source_field_name());
  EXPECT_EQ(kTestFieldName, assignment0.destination_field_name());

  // The second assignment is for kTestFieldName2 = kTestFieldName3.
  const auto& assignment1 = action_descriptor.assignments(1);
  EXPECT_EQ(P4AssignSourceValue::kSourceFieldName,
            assignment1.assigned_value().source_value_case());
  EXPECT_EQ(kTestFieldName3, assignment1.assigned_value().source_field_name());
  EXPECT_EQ(kTestFieldName2, assignment1.destination_field_name());
}

// Tests action parameter assignment to field with undefined action.
TEST_F(TableMapGeneratorTest, TestUndefinedActionAssignParameterField) {
  map_generator_.AssignActionParameterToField(
      kTestActionName, kTestActionParamName, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests action constant assignment to field with undefined action.
TEST_F(TableMapGeneratorTest, TestUndefinedActionAssignConstantField) {
  const int64_t kTestConstant = 0xf00f00f00f00;
  P4AssignSourceValue source_value;
  source_value.set_constant_param(kTestConstant);
  source_value.set_bit_width(48);
  map_generator_.AssignActionSourceValueToField(
      kTestActionName, source_value, kTestFieldName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests drop with undefined action.
TEST_F(TableMapGeneratorTest, TestActionDropUndefined) {
  map_generator_.AddDropPrimitive(kTestActionName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests nop with undefined action.
TEST_F(TableMapGeneratorTest, TestActionNopUndefined) {
  map_generator_.AddNopPrimitive(kTestActionName);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests meter color update with undefined action.
TEST_F(TableMapGeneratorTest, TestUndefinedActionMeterColor) {
  hal::P4ActionDescriptor::P4MeterColorAction color_dummy;
  SetUpTestColorAction(
      {P4_METER_GREEN}, {P4_ACTION_OP_CLONE}, &color_dummy);
  map_generator_.AddMeterColorAction(kTestActionName, color_dummy);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests AddMeterColorActionsFromString with a bad input string.
TEST_F(TableMapGeneratorTest, TestAddMeterColorActionsBogusString) {
  map_generator_.AddAction(kTestActionName);
  const std::string color_actions_text("Not a color actions message");
  map_generator_.AddMeterColorActionsFromString(
      kTestActionName, color_actions_text);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& action_descriptor = iter->second.action_descriptor();
  EXPECT_EQ(0, action_descriptor.color_actions_size());  // Nothing added.
}

// Tests tunnel action update with undefined action.
TEST_F(TableMapGeneratorTest, TestUndefinedActionTunnel) {
  hal::P4ActionDescriptor::P4TunnelAction test_tunnel_action;
  test_tunnel_action.set_header_op(P4_HEADER_SET_VALID);
  test_tunnel_action.set_header_name("encap-header");
  map_generator_.AddTunnelAction(kTestActionName, test_tunnel_action);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests action descriptor replacement with undefined action.
TEST_F(TableMapGeneratorTest, TestReplaceUndefinedActionDescriptor) {
  hal::P4ActionDescriptor new_descriptor;
  new_descriptor.set_type(P4_ACTION_TYPE_FUNCTION);
  new_descriptor.add_primitive_ops(P4_ACTION_OP_DROP);
  new_descriptor.add_primitive_ops(P4_ACTION_OP_CLONE);
  map_generator_.ReplaceActionDescriptor(kTestActionName, new_descriptor);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests adding a new table.
TEST_F(TableMapGeneratorTest, TestAddTable) {
  map_generator_.AddTable(kTestTableName);
  EXPECT_FALSE(map_generator_.generated_map().table_map().empty());
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_EQ(kTestTableName, iter->first);
  EXPECT_TRUE(iter->second.has_table_descriptor());
  EXPECT_EQ(P4_TABLE_UNKNOWN, iter->second.table_descriptor().type());
}

// Tests normal table type setting.
TEST_F(TableMapGeneratorTest, TestSetTableType) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.SetTableType(kTestTableName, P4_TABLE_L3_IP);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  EXPECT_EQ(P4_TABLE_L3_IP, table_descriptor.type());
}

// Tests setting table type of an unknown table.
TEST_F(TableMapGeneratorTest, TestSetTableTypeUnknownTable) {
  map_generator_.SetTableType(kTestTableName, P4_TABLE_L3_IP);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests normal table static entry flag setting.
TEST_F(TableMapGeneratorTest, TestSetTableStaticFlag) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.SetTableStaticEntriesFlag(kTestTableName);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  EXPECT_TRUE(table_descriptor.has_static_entries());
}

// Tests setting static entry flag of an unknown table.
TEST_F(TableMapGeneratorTest, TestSetTableStaticFlagUnknownTable) {
  map_generator_.SetTableStaticEntriesFlag(kTestTableName);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests normal table setting of a single valid header.
TEST_F(TableMapGeneratorTest, TestSetTableValidHeaders) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_ICMP, 0);
  std::set<std::string> valid_header_set = {kTestHeaderName};
  map_generator_.SetTableValidHeaders(kTestTableName, valid_header_set);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  ASSERT_EQ(1, table_descriptor.valid_headers_size());
  EXPECT_EQ(P4_HEADER_ICMP, table_descriptor.valid_headers(0));
}

// Tests setting of a single valid header for an unknown table.
TEST_F(TableMapGeneratorTest, TestSetUnknownTableValidHeaders) {
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_ICMP, 0);
  std::set<std::string> valid_header_set = {kTestHeaderName};
  map_generator_.SetTableValidHeaders(kTestTableName, valid_header_set);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Tests table setting of multiple valid headers.
TEST_F(TableMapGeneratorTest, TestSetTableMultipleValidHeaders) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_IPV4, 0);
  map_generator_.AddHeader(kTestHeaderName2);
  map_generator_.SetHeaderAttributes(kTestHeaderName2, P4_HEADER_IPV6, 0);
  std::set<std::string> valid_header_set = {kTestHeaderName, kTestHeaderName2};
  map_generator_.SetTableValidHeaders(kTestTableName, valid_header_set);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  hal::P4TableDescriptor expected_descriptor;
  expected_descriptor.add_valid_headers(P4_HEADER_IPV4);
  expected_descriptor.add_valid_headers(P4_HEADER_IPV6);
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  EXPECT_TRUE(msg_differencer.Compare(expected_descriptor, table_descriptor));
}

// Tests table setting of multiple valid headers with one header unknown.
TEST_F(TableMapGeneratorTest, TestSetTableMultipleValidHeadersOneUnknown) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_IPV4, 0);
  std::set<std::string> valid_header_set = {"unknown-header", kTestHeaderName};
  map_generator_.SetTableValidHeaders(kTestTableName, valid_header_set);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  ASSERT_EQ(1, table_descriptor.valid_headers_size());
  EXPECT_EQ(P4_HEADER_IPV4, table_descriptor.valid_headers(0));
}

// Tests table setting of a valid header replaces existing valid header.
TEST_F(TableMapGeneratorTest, TestSetTableValidHeadersReplace) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_IPV4, 0);
  map_generator_.AddHeader(kTestHeaderName2);
  map_generator_.SetHeaderAttributes(kTestHeaderName2, P4_HEADER_IPV6, 0);
  std::set<std::string> first_header_set = {kTestHeaderName};
  map_generator_.SetTableValidHeaders(kTestTableName, first_header_set);
  std::set<std::string> second_header_set = {kTestHeaderName2};
  map_generator_.SetTableValidHeaders(kTestTableName, second_header_set);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  ASSERT_EQ(1, table_descriptor.valid_headers_size());
  EXPECT_EQ(P4_HEADER_IPV6, table_descriptor.valid_headers(0));
}

// Verifies that adding the same table name does not disturb the
// existing table_descriptor.
TEST_F(TableMapGeneratorTest, TestAddTableAgain) {
  map_generator_.AddTable(kTestTableName);
  map_generator_.SetTableType(kTestTableName, P4_TABLE_L3_IP);

  // The line below adds the same table again.
  map_generator_.AddTable(kTestTableName);
  auto iter = map_generator_.generated_map().table_map().find(kTestTableName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  const auto& table_descriptor = iter->second.table_descriptor();
  EXPECT_EQ(P4_TABLE_L3_IP, table_descriptor.type());
}

// Tests adding a new header.
TEST_F(TableMapGeneratorTest, TestAddHeader) {
  map_generator_.AddHeader(kTestHeaderName);
  EXPECT_FALSE(map_generator_.generated_map().table_map().empty());
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_EQ(kTestHeaderName, iter->first);
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_UNKNOWN, iter->second.header_descriptor().type());
  EXPECT_EQ(0, iter->second.header_descriptor().depth());
}

// Tests setting type for an existing header.
TEST_F(TableMapGeneratorTest, TestSetHeaderType) {
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_ICMP, 0);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_ICMP, iter->second.header_descriptor().type());
  EXPECT_EQ(0, iter->second.header_descriptor().depth());
}

// Tests setting depth for an existing header.
TEST_F(TableMapGeneratorTest, TestSetHeaderDepth) {
  map_generator_.AddHeader(kTestHeaderName);
  const int32 kDepth = 1;
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_ICMP, kDepth);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_ICMP, iter->second.header_descriptor().type());
  EXPECT_EQ(kDepth, iter->second.header_descriptor().depth());
}

// Tests setting attributes of an undefined header.
TEST_F(TableMapGeneratorTest, TestSetHeaderAttributesUndefined) {
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_UDP, 0);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  EXPECT_TRUE(iter == map_generator_.generated_map().table_map().end());
}

// Verifies that a known header type is not replaced by P4_HEADER_UNKNOWN.
TEST_F(TableMapGeneratorTest, TestSetHeaderAttributesKnownToUnknown) {
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_TCP, 0);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_UNKNOWN, 0);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_TCP, iter->second.header_descriptor().type());
}

// Verifies that a known header type replaces a previous known header type.
TEST_F(TableMapGeneratorTest, TestSetHeaderAttributesKnownToKnown) {
  map_generator_.AddHeader(kTestHeaderName);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_TCP, 0);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_GRE, 0);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_GRE, iter->second.header_descriptor().type());
}

// Verifies that a previous header depth is not replaced by a zero depth.
TEST_F(TableMapGeneratorTest, TestSetHeaderAttributesDepthToZeroDepth) {
  map_generator_.AddHeader(kTestHeaderName);
  const int32 kDepth = 1;
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_TCP, kDepth);
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_TCP, 0);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_TCP, iter->second.header_descriptor().type());
  EXPECT_EQ(kDepth, iter->second.header_descriptor().depth());
}

// Verifies that a non-zero depth replaces a previous depth.
TEST_F(TableMapGeneratorTest, TestSetHeaderAttributesNewDepth) {
  map_generator_.AddHeader(kTestHeaderName);
  const int32 kDepth1 = 1;
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_GRE, kDepth1);
  const int32 kDepth2 = 2;
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_GRE, kDepth2);
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_GRE, iter->second.header_descriptor().type());
  EXPECT_EQ(kDepth2, iter->second.header_descriptor().depth());
}

// Verifies that adding the same header name does not disturb the
// existing header_descriptor.
TEST_F(TableMapGeneratorTest, TestAddHeaderAgain) {
  map_generator_.AddHeader(kTestHeaderName);
  const int32 kDepth = 1;
  map_generator_.SetHeaderAttributes(kTestHeaderName, P4_HEADER_TCP, kDepth);
  map_generator_.AddHeader(kTestHeaderName);  // Adds the same header again.
  auto iter = map_generator_.generated_map().table_map().find(kTestHeaderName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(iter->second.has_header_descriptor());
  EXPECT_EQ(P4_HEADER_TCP, iter->second.header_descriptor().type());
  EXPECT_EQ(kDepth, iter->second.header_descriptor().depth());
}

// Verifies table map insertion of an internal action descriptor.
TEST_F(TableMapGeneratorTest, TestAddInternalAction) {
  hal::P4ActionDescriptor internal_descriptor;
  internal_descriptor.set_type(P4_ACTION_TYPE_FUNCTION);
  internal_descriptor.add_primitive_ops(P4_ACTION_OP_DROP);
  internal_descriptor.add_primitive_ops(P4_ACTION_OP_CLONE);
  map_generator_.AddInternalAction(kTestActionName, internal_descriptor);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      internal_descriptor, iter->second.internal_action()));
}

// Verifies table map insertion of an internal action descriptor overwrites
// an existing internal action with the same name.
TEST_F(TableMapGeneratorTest, TestAddInternalActionAgain) {
  hal::P4ActionDescriptor internal_descriptor;
  internal_descriptor.add_primitive_ops(P4_ACTION_OP_DROP);
  internal_descriptor.set_type(P4_ACTION_TYPE_FUNCTION);
  map_generator_.AddInternalAction(kTestActionName, internal_descriptor);
  internal_descriptor.add_primitive_ops(P4_ACTION_OP_CLONE);
  map_generator_.AddInternalAction(kTestActionName, internal_descriptor);
  auto iter = map_generator_.generated_map().table_map().find(kTestActionName);
  ASSERT_TRUE(iter != map_generator_.generated_map().table_map().end());
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      internal_descriptor, iter->second.internal_action()));
}

}  // namespace p4c_backends
}  // namespace stratum
