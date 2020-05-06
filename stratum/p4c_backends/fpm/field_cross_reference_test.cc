// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for FieldCrossReference.

#include "stratum/p4c_backends/fpm/field_cross_reference.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// The test parameter is a pair with the P4FieldType to be assigned to one
// of the test fields created by SetUpFieldAssignments.
class FieldCrossReferenceTest : public testing::TestWithParam<
    std::pair<const std::string, P4FieldType>> {
 protected:
  // Sets up three field-to-field assignment statements for test use:
  //  field_0 = field_1;
  //  field_1 = field_2;
  //  field_2 = field_3;
  // Field descriptors for each field are also created, with each field's
  // type set to initial_field_type.
  void SetUpFieldAssignments(P4FieldType initial_field_type) {
    hal::P4TableMapValue field_map_value;
    field_map_value.mutable_field_descriptor()->set_type(initial_field_type);
    const int kNumFields = 4;
    for (int f = 0; f < kNumFields; ++f) {
      const std::string field_name = absl::Substitute("field_$0", f);
      test_field_paths_own_.push_back(
          absl::make_unique<IR::PathExpression>(cstring(field_name)));
      (*p4_pipeline_config_.mutable_table_map())[field_name] = field_map_value;
    }
    for (int s = 0; s < kNumFields - 1; ++s) {
      test_assignments_own_.push_back(
          absl::make_unique<IR::AssignmentStatement>(
              test_field_paths_own_[s].get(),
              test_field_paths_own_[s + 1].get()));
      test_assignments_.push_back(test_assignments_own_[s].get());
    }
  }

  FieldCrossReference test_field_xref_;  // A FieldCrossReference for test use.

  // SetUpFieldAssignments initializes test_assignments_ to look like the
  // AssignmentStatement vector that usually comes from the backend's
  // ProgramInspector.  It creates field descriptors for test use in
  // p4_pipeline_config_.
  std::vector<const IR::AssignmentStatement*> test_assignments_;
  hal::P4PipelineConfig p4_pipeline_config_;

  // These containers of unique_ptr are for managing ownership of IR nodes
  // created by SetUpFieldAssignments.
  std::vector<std::unique_ptr<IR::PathExpression>> test_field_paths_own_;
  std::vector<std::unique_ptr<IR::AssignmentStatement>> test_assignments_own_;
};

// Tests the ability to infer types of all fields from a single known field
// type in the SetUpFieldAssignments three-statement sequence.  All fields but
// one start with P4_FIELD_TYPE_ANNOTATED.  The remaining field descriptor's
// type is adjusted according to the test parameters.
TEST_P(FieldCrossReferenceTest, TestTypeInferenceToAnnotated) {
  SetUpFieldAssignments(P4_FIELD_TYPE_ANNOTATED);
  const P4FieldType kTestFieldType = GetParam().second;
  hal::P4TableMapValue field_map_value;
  field_map_value.mutable_field_descriptor()->set_type(kTestFieldType);
  (*p4_pipeline_config_.mutable_table_map())[GetParam().first] =
      field_map_value;
  test_field_xref_.ProcessAssignments(test_assignments_, &p4_pipeline_config_);

  // After ProcessAssignments runs, all field descriptors should have the
  // type specified by the test parameter.
  for (const auto& iter : p4_pipeline_config_.table_map()) {
    EXPECT_EQ(kTestFieldType, iter.second.field_descriptor().type());
  }
}

// This test is the same as TestTypeInferenceToAnnotated, except that the
// initial unspecified field type is P4_FIELD_TYPE_UNKNOWN.
TEST_P(FieldCrossReferenceTest, TestTypeInferenceToUnknown) {
  SetUpFieldAssignments(P4_FIELD_TYPE_UNKNOWN);
  const P4FieldType kTestFieldType = GetParam().second;
  hal::P4TableMapValue field_map_value;
  field_map_value.mutable_field_descriptor()->set_type(kTestFieldType);
  (*p4_pipeline_config_.mutable_table_map())[GetParam().first] =
      field_map_value;
  test_field_xref_.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  for (const auto& iter : p4_pipeline_config_.table_map()) {
    EXPECT_EQ(kTestFieldType, iter.second.field_descriptor().type());
  }
}

// Verifies that ProcessAssignments does not overwrite previously known
// field types.  All fields except the one in the test parameter start with
// the known type P4_FIELD_TYPE_IPV6_DST.  The remaining field gets a different
// type.  After ProcessAssignments runs, all fields should retain their
// original types.
TEST_P(FieldCrossReferenceTest, TestNoKnownFieldTypeOverwrite) {
  const P4FieldType kKnownFieldType = P4_FIELD_TYPE_IPV6_DST;
  SetUpFieldAssignments(kKnownFieldType);
  const P4FieldType kTestFieldType = GetParam().second;
  hal::P4TableMapValue field_map_value;
  field_map_value.mutable_field_descriptor()->set_type(kTestFieldType);
  (*p4_pipeline_config_.mutable_table_map())[GetParam().first] =
      field_map_value;
  test_field_xref_.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  for (const auto& iter : p4_pipeline_config_.table_map()) {
    if (iter.first != GetParam().first) {
      EXPECT_EQ(kKnownFieldType, iter.second.field_descriptor().type());
    } else {
      EXPECT_EQ(kTestFieldType, iter.second.field_descriptor().type());
    }
  }
}

// Verifies that nothing happens when one of the fields has no field descriptor.
// This mimics the case where assignments involve hidden temporary fields,
// constants, and other non-header field expressions.
TEST_P(FieldCrossReferenceTest, TestNoFieldDescriptor) {
  SetUpFieldAssignments(P4_FIELD_TYPE_ANNOTATED);

  // The field descriptor identified by the test parameter is erased.
  auto iter = p4_pipeline_config_.mutable_table_map()->find(GetParam().first);
  ASSERT_TRUE(iter != p4_pipeline_config_.mutable_table_map()->end());
  p4_pipeline_config_.mutable_table_map()->erase(iter);

  // The first remaining field gets the known field type P4_FIELD_TYPE_IPV6_DST.
  std::string remove_field = "field_0";
  if (remove_field == GetParam().first)
    remove_field = "field_1";
  auto first_iter = p4_pipeline_config_.mutable_table_map()->find(remove_field);
  ASSERT_TRUE(first_iter != p4_pipeline_config_.mutable_table_map()->end());
  const P4FieldType kTestFieldType = P4_FIELD_TYPE_IPV6_DST;
  first_iter->second.mutable_field_descriptor()->set_type(kTestFieldType);
  test_field_xref_.ProcessAssignments(test_assignments_, &p4_pipeline_config_);

  // The ability to infer types in the tested assignments depends on which
  // field descriptor was taken away.  The containers below specify the
  // expected output types according to the removed descriptor.  The set of
  // maps named no_fieldN_types indicates the expected values for the remaining
  // fields after removing fieldN.
  const std::map<std::string, P4FieldType> no_field0_types {
    {"field_1", kTestFieldType},
    {"field_2", kTestFieldType},
    {"field_3", kTestFieldType},
  };
  const std::map<std::string, P4FieldType> no_field1_types {
    {"field_0", kTestFieldType},
    {"field_2", P4_FIELD_TYPE_ANNOTATED},
    {"field_3", P4_FIELD_TYPE_ANNOTATED},
  };
  const std::map<std::string, P4FieldType> no_field2_types {
    {"field_0", kTestFieldType},
    {"field_1", kTestFieldType},
    {"field_3", P4_FIELD_TYPE_ANNOTATED},
  };
  const std::map<std::string, P4FieldType> no_field3_types {
    {"field_0", kTestFieldType},
    {"field_1", kTestFieldType},
    {"field_2", kTestFieldType},
  };
  const std::map<std::string, const std::map<std::string, P4FieldType>*>
      no_field_map {
    {"field_0", &no_field0_types},
    {"field_1", &no_field1_types},
    {"field_2", &no_field2_types},
    {"field_3", &no_field3_types},
  };

  const auto& type_iter = no_field_map.find(GetParam().first);
  ASSERT_TRUE(type_iter != no_field_map.end());
  const auto& expected_types = *type_iter->second;
  ASSERT_EQ(p4_pipeline_config_.table_map().size(), expected_types.size());
  for (const auto& table_map_iter : p4_pipeline_config_.table_map()) {
    const auto& find_type_iter = expected_types.find(table_map_iter.first);
    ASSERT_TRUE(find_type_iter != expected_types.end());
    const P4FieldType expected_type = find_type_iter->second;
    EXPECT_EQ(expected_type, table_map_iter.second.field_descriptor().type());
  }
}

// Verifies that nothing happens when field names refer to table map entries
// that exist, but are not field descriptors.
TEST_P(FieldCrossReferenceTest, TestOtherDescriptors) {
  SetUpFieldAssignments(P4_FIELD_TYPE_ANNOTATED);

  // The test replaces the test parameter's field descriptor with an
  // action descriptor.
  hal::P4TableMapValue action_value;
  action_value.mutable_action_descriptor()->set_type(P4_ACTION_TYPE_FUNCTION);
  (*p4_pipeline_config_.mutable_table_map())[GetParam().first] = action_value;
  test_field_xref_.ProcessAssignments(test_assignments_, &p4_pipeline_config_);
  for (const auto& iter : p4_pipeline_config_.table_map()) {
    if (iter.first != GetParam().first) {
      EXPECT_TRUE(iter.second.has_field_descriptor());
      EXPECT_EQ(P4_FIELD_TYPE_ANNOTATED, iter.second.field_descriptor().type());
    } else {
      EXPECT_TRUE(iter.second.has_action_descriptor());
    }
  }
}

// The test parameter values assure that each test runs for every field
// in the three-assignment test statement sequence.
INSTANTIATE_TEST_SUITE_P(
    FieldWithType,
    FieldCrossReferenceTest,
    ::testing::Values(
        std::make_pair("field_0", P4_FIELD_TYPE_ETH_SRC),
        std::make_pair("field_1", P4_FIELD_TYPE_VRF),
        std::make_pair("field_2", P4_FIELD_TYPE_COLOR),
        std::make_pair("field_3", P4_FIELD_TYPE_ETH_DST)));

}  // namespace p4c_backends
}  // namespace stratum
