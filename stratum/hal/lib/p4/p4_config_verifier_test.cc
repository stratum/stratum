// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


// This file contains unit tests for P4ConfigVerifier.

#include "stratum/hal/lib/p4/p4_config_verifier.h"

#include <memory>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/public/lib/error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/gtl/map_util.h"

// P4ConfigVerifier flags to override for some tests.
DECLARE_string(match_field_error_level);
DECLARE_string(action_field_error_level);

using ::testing::HasSubstr;
using ::gflags::FlagSaver;

namespace stratum {
namespace hal {

// This class is the P4ConfigVerifier test fixture.
class P4ConfigVerifierTest : public testing::Test {
 protected:
  void SetUpP4ConfigFromFiles() {
    const std::string kTestP4InfoFile =
        "stratum/hal/lib/p4/testdata/test_p4_info.pb.txt";
    ASSERT_OK(ReadProtoFromTextFile(kTestP4InfoFile, &test_p4_info_));
    const std::string kTestP4PipelineConfigFile =
        "stratum/hal/lib/p4/testdata/"
        "test_p4_pipeline_config.pb.txt";
    ASSERT_OK(ReadProtoFromTextFile(
        kTestP4PipelineConfigFile, &test_p4_pipeline_config_));

    // P4ConfigVerifier assumes P4InfoManager pre-validation of P4Info.
    p4_info_manager_ = absl::make_unique<P4InfoManager>(test_p4_info_);
    ASSERT_OK(p4_info_manager_->InitializeAndVerify());
  }

  // Verifies that the first table in test_p4_info_ has a valid table
  // descriptor.
  bool FirstTableHasDescriptor() {
    bool descriptor_ok = false;
    CHECK_LT(0, test_p4_info_.tables_size()) << "Test P4Info has no tables";
    const ::p4::config::v1::Table& first_p4_table = test_p4_info_.tables(0);
    P4TableMapValue* value =
        gtl::FindOrNull(*test_p4_pipeline_config_.mutable_table_map(),
                        first_p4_table.preamble().name());
    if (value != nullptr) {
      descriptor_ok = value->has_table_descriptor();
    }
    return descriptor_ok;
  }

  // Verifies that the first match field in the first table in test_p4_info_
  // has a valid field descriptor.
  bool FirstMatchFieldHasDescriptor() {
    bool descriptor_ok = false;
    CHECK_LT(0, test_p4_info_.tables_size()) << "Test P4Info has no tables";
    const ::p4::config::v1::Table& first_p4_table = test_p4_info_.tables(0);
    CHECK_LT(0, first_p4_table.match_fields_size())
        << "First table in test P4Info has no match fields";
    P4TableMapValue* value =
        gtl::FindOrNull(*test_p4_pipeline_config_.mutable_table_map(),
                        first_p4_table.match_fields(0).name());
    if (value != nullptr) {
      descriptor_ok = value->has_field_descriptor();
    }
    return descriptor_ok;
  }

  // Verifies that the first action in test_p4_info_ has a valid action
  // descriptor.
  bool FirstActionHasDescriptor() {
    bool descriptor_ok = false;
    CHECK_LT(0, test_p4_info_.actions_size()) << "Test P4Info has no actions";
    const ::p4::config::v1::Action& first_p4_action = test_p4_info_.actions(0);
    P4TableMapValue* value =
        gtl::FindOrNull(*test_p4_pipeline_config_.mutable_table_map(),
                        first_p4_action.preamble().name());
    if (value != nullptr) {
      descriptor_ok = value->has_action_descriptor();
    }
    return descriptor_ok;
  }

  // Adds a static table entry in test_p4_pipeline_config_.  The added entry
  // has attributes set according to the first table in test_p4_info_.
  void SetUpStaticTableEntry() {
    ASSERT_LE(1, test_p4_info_.tables_size());
    const ::p4::config::v1::Table& p4_table = test_p4_info_.tables(0);
    ::p4::v1::TableEntry static_table_entry;
    static_table_entry.set_table_id(p4_table.preamble().id());

    // For simplicity, each FieldMatch value is empty to use the default.
    // The P4ConfigVerifier currently does not validate any field values.
    for (const auto& match_field : p4_table.match_fields()) {
      ::p4::v1::FieldMatch* static_match = static_table_entry.add_match();
      static_match->set_field_id(match_field.id());
    }

    ::p4::v1::WriteRequest* test_write_request =
        test_p4_pipeline_config_.mutable_static_table_entries();
    ::p4::v1::Update* new_update = test_write_request->add_updates();
    new_update->set_type(::p4::v1::Update::INSERT);
    *(new_update->mutable_entity()->mutable_table_entry()) = static_table_entry;
  }

  // Tests typically create p4_verifier_ after setting test_p4_info_ and
  // test_p4_pipeline_config_ according to their needs.
  std::unique_ptr<P4ConfigVerifier> p4_verifier_;
  ::p4::config::v1::P4Info test_p4_info_;
  P4PipelineConfig test_p4_pipeline_config_;

  // P4InfoManager used for verifying P4Info.
  std::unique_ptr<P4InfoManager> p4_info_manager_;

  // Reverts FLAGS to defaults after test runs.
  FlagSaver flag_saver_;
};

TEST_F(P4ConfigVerifierTest, TestValidP4Config) {
  SetUpP4ConfigFromFiles();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->Verify());
}

TEST_F(P4ConfigVerifierTest, TestValidP4ConfigFirstCompare) {
  SetUpP4ConfigFromFiles();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::p4::config::v1::P4Info empty_p4_info;
  P4PipelineConfig empty_p4_pipeline;
  EXPECT_OK(p4_verifier_->VerifyAndCompare(empty_p4_info, empty_p4_pipeline));
}

TEST_F(P4ConfigVerifierTest, TestEmptyPipelineConfig) {
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("missing object mapping"));
}

TEST_F(P4ConfigVerifierTest, TestMissingTableDescriptor) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstTableHasDescriptor());
  const std::string& first_table_name =
      test_p4_info_.tables(0).preamble().name();
  test_p4_pipeline_config_.mutable_table_map()->erase(first_table_name);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_table_name));
}

TEST_F(P4ConfigVerifierTest, TestWrongTableDescriptorType) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstTableHasDescriptor());
  const std::string& first_table_name =
      test_p4_info_.tables(0).preamble().name();

  // This test replaces the table descriptor with a field descriptor of
  // the same name.
  test_p4_pipeline_config_.mutable_table_map()->erase(first_table_name);
  P4TableMapValue bad_descriptor;
  bad_descriptor.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_VRF);
  (*test_p4_pipeline_config_.mutable_table_map())[first_table_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("not have the expected descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_table_name));
}

TEST_F(P4ConfigVerifierTest, TestMissingTablePipelineStage) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstTableHasDescriptor());
  const std::string& first_table_name =
      test_p4_info_.tables(0).preamble().name();

  // This test replaces the table descriptor with a new one that has
  // no pipeline_stage set.
  test_p4_pipeline_config_.mutable_table_map()->erase(first_table_name);
  P4TableMapValue bad_descriptor;
  bad_descriptor.mutable_table_descriptor();  // Nothing set in descriptor.
  (*test_p4_pipeline_config_.mutable_table_map())[first_table_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("not specify a pipeline stage"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_table_name));
}

TEST_F(P4ConfigVerifierTest, TestMissingFieldDescriptor) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstMatchFieldHasDescriptor());
  const std::string& first_field_name =
      test_p4_info_.tables(0).match_fields(0).name();
  test_p4_pipeline_config_.mutable_table_map()->erase(first_field_name);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_field_name));
  EXPECT_THAT(status.ToString(),
              HasSubstr(test_p4_info_.tables(0).preamble().name()));
}

TEST_F(P4ConfigVerifierTest, TestWrongFieldDescriptorType) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstMatchFieldHasDescriptor());
  const std::string& first_field_name =
      test_p4_info_.tables(0).match_fields(0).name();

  // This test replaces the field descriptor with an action descriptor of
  // the same name.
  test_p4_pipeline_config_.mutable_table_map()->erase(first_field_name);
  P4TableMapValue bad_descriptor;
  bad_descriptor.mutable_action_descriptor()->set_type(P4_ACTION_TYPE_FUNCTION);
  (*test_p4_pipeline_config_.mutable_table_map())[first_field_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("not have the expected descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_field_name));
  EXPECT_THAT(status.ToString(),
              HasSubstr(test_p4_info_.tables(0).preamble().name()));
}

TEST_F(P4ConfigVerifierTest, TestMissingMatchFieldType) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstMatchFieldHasDescriptor());
  const std::string& first_field_name =
      test_p4_info_.tables(0).match_fields(0).name();

  // This test clears all valid converions in the field descriptor so
  // the match type specified by the P4 table won't be found.
  P4TableMapValue* value = gtl::FindOrNull(
      *test_p4_pipeline_config_.mutable_table_map(), first_field_name);
  ASSERT_TRUE(value != nullptr);
  value->mutable_field_descriptor()->clear_valid_conversions();

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("has no conversion entry"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_field_name));
  EXPECT_THAT(status.ToString(),
              HasSubstr(test_p4_info_.tables(0).preamble().name()));
}

TEST_F(P4ConfigVerifierTest, TestUnknownMatchFieldType) {
  FLAGS_match_field_error_level = "error";  // Set strictest level.
  SetUpP4ConfigFromFiles();
  const std::string kTestMatchField = "test-header-field-32";

  // This test clears the field descriptor type value so
  // it will be unknown when referenced as a table match field.
  P4TableMapValue* value = gtl::FindOrNull(
      *test_p4_pipeline_config_.mutable_table_map(), kTestMatchField);
  ASSERT_TRUE(value != nullptr);
  value->mutable_field_descriptor()->clear_type();

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("in table"));
  EXPECT_THAT(status.ToString(), HasSubstr("has an unspecified field type"));
  EXPECT_THAT(status.ToString(), HasSubstr(kTestMatchField));
  EXPECT_THAT(status.ToString(),
              HasSubstr(test_p4_info_.tables(0).preamble().name()));
}

TEST_F(P4ConfigVerifierTest, TestMissingActionDescriptor) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();
  test_p4_pipeline_config_.mutable_table_map()->erase(first_action_name);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestWrongActionDescriptorType) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test replaces the action descriptor with a field descriptor of
  // the same name.
  test_p4_pipeline_config_.mutable_table_map()->erase(first_action_name);
  P4TableMapValue bad_descriptor;
  bad_descriptor.mutable_field_descriptor()->set_type(P4_FIELD_TYPE_VRF);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("not have the expected descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestMissingActionDestinationFieldDescriptor) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts a reference to
  // a non-existent destination header field.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  bad_assignment->mutable_assigned_value()->set_constant_param(1);
  const std::string kMissingFieldName = "unknown-header-field";
  bad_assignment->set_destination_field_name(kMissingFieldName);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(kMissingFieldName));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

// This test is the same as the previous test, but it uses the deprecated
// destination_field_names(0) instead of the new destination_field_name.
// TODO(teverman): Remove this test after deprecation of
// destination_field_names.
TEST_F(P4ConfigVerifierTest, TestMissingActionDestinationFieldDescriptorOld) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts a reference to
  // a non-existent destination header field.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  bad_assignment->mutable_assigned_value()->set_constant_param(1);
  const std::string kMissingFieldName = "unknown-header-field";
  bad_assignment->add_destination_field_names(kMissingFieldName);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(kMissingFieldName));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestMissingActionSourceFieldDescriptor) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts a reference to
  // a non-existent source header field.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  const std::string kMissingFieldName = "unknown-header-field";
  bad_assignment->mutable_assigned_value()->
      set_source_field_name(kMissingFieldName);
  const std::string kTestDestField = "test-header-field-32";
  bad_assignment->set_destination_field_name(kTestDestField);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(kMissingFieldName));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

// This test is the same as the previous test, but it uses the deprecated
// destination_field_names(0) instead of the new destination_field_name.
// TODO(teverman): Remove this test after deprecation of
// destination_field_names.
TEST_F(P4ConfigVerifierTest, TestMissingActionSourceFieldDescriptorOld) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts a reference to
  // a non-existent source header field.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  const std::string kMissingFieldName = "unknown-header-field";
  bad_assignment->mutable_assigned_value()->
      set_source_field_name(kMissingFieldName);
  const std::string kTestDestField = "test-header-field-32";
  bad_assignment->add_destination_field_names(kTestDestField);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor for"));
  EXPECT_THAT(status.ToString(), HasSubstr("referenced by P4 object"));
  EXPECT_THAT(status.ToString(), HasSubstr(kMissingFieldName));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestUnknownActionDestinationFieldType) {
  FLAGS_action_field_error_level = "error";  // Set strictest level.
  SetUpP4ConfigFromFiles();
  const std::string kTestHeaderField = "test-header-field-128";

  // This test clears the destination field descriptor type value so
  // it will be unknown when referenced from an action statement.
  // The Verify should only enforce field types when used as a source field.
  P4TableMapValue* value = gtl::FindOrNull(
      *test_p4_pipeline_config_.mutable_table_map(), kTestHeaderField);
  ASSERT_TRUE(value != nullptr);
  value->mutable_field_descriptor()->clear_type();

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->Verify());
}

TEST_F(P4ConfigVerifierTest, TestUnknownActionSourceFieldType) {
  FLAGS_action_field_error_level = "error";  // Set strictest level.
  SetUpP4ConfigFromFiles();
  const std::string kTestHeaderField = "test-header-field-32";

  // This test clears the field descriptor type value so
  // it will be unknown when referenced from an action statement.
  P4TableMapValue* value = gtl::FindOrNull(
      *test_p4_pipeline_config_.mutable_table_map(), kTestHeaderField);
  ASSERT_TRUE(value != nullptr);
  value->mutable_field_descriptor()->clear_type();

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("in action"));
  EXPECT_THAT(status.ToString(), HasSubstr("has an unspecified field type"));
  EXPECT_THAT(status.ToString(), HasSubstr(kTestHeaderField));
}

// TODO(teverman): This test goes away after full deprecation of the repeated
// destination_field_names.
TEST_F(P4ConfigVerifierTest, TestActionMultipleDestinationFields) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts an unexpected
  // assignment to multiple destination fields.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  const std::string kDestField1 = "test-header-field-32";
  const std::string kDestField2 = "test-header-field-64";
  bad_assignment->mutable_assigned_value()->set_parameter_name("param-64");
  bad_assignment->add_destination_field_names(kDestField1);
  bad_assignment->add_destination_field_names(kDestField2);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unexpected assignment"));
  EXPECT_THAT(status.ToString(), HasSubstr(kDestField1));
  EXPECT_THAT(status.ToString(), HasSubstr(kDestField2));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestActionEmptyAssignment) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test copies the first action descriptor and inserts an unexpected
  // assignment with neither primitives nor destination fields.
  P4TableMapValue bad_descriptor =
      (*test_p4_pipeline_config_.mutable_table_map())[first_action_name];
  P4ActionDescriptor::P4ActionInstructions* bad_assignment =
      bad_descriptor.mutable_action_descriptor()->add_assignments();
  const std::string kTestParam = "param-64";
  bad_assignment->mutable_assigned_value()->set_parameter_name(kTestParam);
  (*test_p4_pipeline_config_.mutable_table_map())[first_action_name] =
      bad_descriptor;

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unexpected assignment"));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
  EXPECT_THAT(status.ToString(), HasSubstr(kTestParam));
}

TEST_F(P4ConfigVerifierTest, TestActionInternalLink) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);
  P4TableMapValue internal_action_value;
  internal_action_value.mutable_internal_action();
  gtl::InsertOrDie(test_p4_pipeline_config_.mutable_table_map(),
                   kInternalAction, internal_action_value);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->Verify());
}

TEST_F(P4ConfigVerifierTest, TestActionBadInternalLink) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action that does not exist.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "unknown-internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("map has no descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr(kInternalAction));
  EXPECT_THAT(status.ToString(), HasSubstr(first_action_name));
}

TEST_F(P4ConfigVerifierTest, TestInternalActionWithRedirects) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action, then populates the internal action with another
  // level of indirection.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);
  P4TableMapValue internal_action_value;
  auto internal_action = internal_action_value.mutable_internal_action();
  internal_action->add_action_redirects();
  gtl::InsertOrDie(test_p4_pipeline_config_.mutable_table_map(),
                   kInternalAction, internal_action_value);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unexpected redirects"));
  EXPECT_THAT(status.ToString(), HasSubstr(kInternalAction));
}

// TODO(teverman): Find another way to cause the bad assignment error after
// full deprecation of the repeated destination_field_names.
TEST_F(P4ConfigVerifierTest, TestInternalActionWithBadAssignment) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action, then populates the internal action with invalid
  // assignment instructions.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);
  P4TableMapValue internal_action_value;
  auto internal_action = internal_action_value.mutable_internal_action();
  auto assignment = internal_action->add_assignments();
  assignment->add_destination_field_names("field1");  // Multiple destination
  assignment->add_destination_field_names("field2");  // fields are invalid.
  gtl::InsertOrDie(test_p4_pipeline_config_.mutable_table_map(),
                   kInternalAction, internal_action_value);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unexpected assignment"));
  EXPECT_THAT(status.ToString(), HasSubstr(kInternalAction));
}

TEST_F(P4ConfigVerifierTest, TestActionInternalLinkWithAppliedTables) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action.  The link is constrained to the first P4 table.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  ASSERT_TRUE(FirstTableHasDescriptor());
  internal_link->add_applied_tables(test_p4_info_.tables(0).preamble().name());
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);
  P4TableMapValue internal_action_value;
  internal_action_value.mutable_internal_action();
  gtl::InsertOrDie(test_p4_pipeline_config_.mutable_table_map(),
                   kInternalAction, internal_action_value);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->Verify());
}

TEST_F(P4ConfigVerifierTest, TestActionInternalLinkWithUnknownAppliedTables) {
  SetUpP4ConfigFromFiles();
  ASSERT_TRUE(FirstActionHasDescriptor());
  const std::string& first_action_name =
      test_p4_info_.actions(0).preamble().name();

  // This test creates a link from the first action descriptor to an
  // internal action.  The link is constrained to a P4 table that doesn't exist.
  P4TableMapValue link_descriptor = gtl::FindOrDie(
      test_p4_pipeline_config_.table_map(), first_action_name);
  P4ActionDescriptor::P4InternalActionLink* internal_link =
      link_descriptor.mutable_action_descriptor()->
      add_action_redirects()->add_internal_links();
  const std::string kInternalAction = "internal-action";
  internal_link->set_internal_action_name(kInternalAction);
  internal_link->add_applied_tables("unknown-applied-table");
  gtl::InsertOrUpdate(test_p4_pipeline_config_.mutable_table_map(),
                      first_action_name, link_descriptor);
  P4TableMapValue internal_action_value;
  internal_action_value.mutable_internal_action();
  gtl::InsertOrDie(test_p4_pipeline_config_.mutable_table_map(),
                   kInternalAction, internal_action_value);

  p4_verifier_ =
      P4ConfigVerifier::CreateInstance(test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("map has no descriptor"));
  EXPECT_THAT(status.ToString(), HasSubstr(kInternalAction));
  EXPECT_THAT(status.ToString(), HasSubstr("unknown-applied-table"));
}

// TODO(unknown): When P4ConfigVerifier supports header-to-header copy
// verification, add a test for an invalid copy, i.e. one where the header
// has no header descriptor.

TEST_F(P4ConfigVerifierTest, TestValidStaticTableEntry) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->Verify());
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryBadUpdateType) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  ::p4::v1::Update* test_update =
      test_p4_pipeline_config_.mutable_static_table_entries()->mutable_updates(
          0);
  test_update->set_type(::p4::v1::Update::DELETE);  // DELETE is unexpected.
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("unexpected type"));
  EXPECT_THAT(status.ToString(), HasSubstr("DELETE"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryNotTableEntry) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  ::p4::v1::Entity* test_entity =
      test_p4_pipeline_config_.mutable_static_table_entries()
          ->mutable_updates(0)
          ->mutable_entity();
  test_entity->clear_table_entry();  // Clears the expected table_entry.
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("no TableEntry"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryBadTableID) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  ::p4::v1::Entity* test_entity =
      test_p4_pipeline_config_.mutable_static_table_entries()
          ->mutable_updates(0)
          ->mutable_entity();
  test_entity->mutable_table_entry()->set_table_id(0xf123f);
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table_id is not in P4Info"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryNoFieldMatches) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  ::p4::v1::Entity* test_entity =
      test_p4_pipeline_config_.mutable_static_table_entries()
          ->mutable_updates(0)
          ->mutable_entity();
  test_entity->mutable_table_entry()->clear_match();  // Clears expected match.
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status = p4_verifier_->Verify();
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("0 match fields"));
  EXPECT_THAT(status.ToString(), HasSubstr("P4Info expects 1"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryCompareNoChange) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  P4PipelineConfig old_p4_pipeline = test_p4_pipeline_config_;
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->VerifyAndCompare(test_p4_info_, old_p4_pipeline));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryCompareAddition) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  P4PipelineConfig old_p4_pipeline = test_p4_pipeline_config_;
  old_p4_pipeline.clear_static_table_entries();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  EXPECT_OK(p4_verifier_->VerifyAndCompare(test_p4_info_, old_p4_pipeline));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryCompareDeletion) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  P4PipelineConfig old_p4_pipeline = test_p4_pipeline_config_;
  test_p4_pipeline_config_.clear_static_table_entries();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status =
      p4_verifier_->VerifyAndCompare(test_p4_info_, old_p4_pipeline);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("deletions that require a reboot"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryCompareModification) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  P4PipelineConfig old_p4_pipeline = test_p4_pipeline_config_;
  auto update =
      old_p4_pipeline.mutable_static_table_entries()->mutable_updates(0);
  update->mutable_entity()->mutable_table_entry()->mutable_action()->
      mutable_action()->set_action_id(1);
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status =
      p4_verifier_->VerifyAndCompare(test_p4_info_, old_p4_pipeline);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.ToString(),
              HasSubstr("modifications that require a reboot"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryModifyAndDelete) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();
  P4PipelineConfig p4_pipeline_one_static = test_p4_pipeline_config_;

  // The sequence below adds a second test_p4_pipeline_config_ static entry.
  // This entry needs to be modified with a different table ID since
  // SetUpStaticTableEntry puts the same ID in all entries.
  SetUpStaticTableEntry();
  auto deleted_table_entry =
      test_p4_pipeline_config_.mutable_static_table_entries()->
      mutable_updates(1)->mutable_entity()->mutable_table_entry();
  ASSERT_LE(2, test_p4_info_.tables_size());
  deleted_table_entry->set_table_id(test_p4_info_.tables(1).preamble().id());
  auto modified_table_entry =
      test_p4_pipeline_config_.mutable_static_table_entries()->
      mutable_updates(0)->mutable_entity()->mutable_table_entry();
  modified_table_entry->set_priority(100);

  // The error string from the ERR_REBOOT_REQUIRED status should report both
  // a modify and a delete.
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, p4_pipeline_one_static);
  ::util::Status status =
      p4_verifier_->VerifyAndCompare(test_p4_info_, test_p4_pipeline_config_);
  EXPECT_EQ(ERR_REBOOT_REQUIRED, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("deletions that require a reboot"));
  EXPECT_THAT(status.ToString(),
              HasSubstr("modifications that require a reboot"));
}

TEST_F(P4ConfigVerifierTest, TestStaticTableEntryVerifyVsRebootPrecedence) {
  SetUpP4ConfigFromFiles();
  SetUpStaticTableEntry();

  // The table descriptor removal below triggers a basic Verify error.
  ASSERT_TRUE(FirstTableHasDescriptor());
  const std::string& first_table_name =
      test_p4_info_.tables(0).preamble().name();
  test_p4_pipeline_config_.mutable_table_map()->erase(first_table_name);

  // The old_p4_pipeline adjustment simulates a reboot-required deletion.
  P4PipelineConfig old_p4_pipeline = test_p4_pipeline_config_;
  test_p4_pipeline_config_.clear_static_table_entries();
  p4_verifier_ = P4ConfigVerifier::CreateInstance(
      test_p4_info_, test_p4_pipeline_config_);
  ::util::Status status =
      p4_verifier_->VerifyAndCompare(test_p4_info_, old_p4_pipeline);

  // ERR_INTERNAL should overrule ERR_REBOOT_REQUIRED.
  EXPECT_EQ(ERR_INTERNAL, status.error_code());
  EXPECT_THAT(status.ToString(), HasSubstr("table map has no descriptor"));
}

// This test sets up an error that is under command-line flag control, then
// verifies that the Verify status is OK for all flag values that do not
// mandate verification errors.  Other tests that use the flag "error" option
// provide additional coverage.
TEST_F(P4ConfigVerifierTest, TestNonErrorLevels) {
  SetUpP4ConfigFromFiles();
  const std::string kTestHeaderField = "test-header-field-128";

  // This test clears the field descriptor type value so
  // it will be unknown when referenced from an action statement.
  P4TableMapValue* value = gtl::FindOrNull(
      *test_p4_pipeline_config_.mutable_table_map(), kTestHeaderField);
  ASSERT_TRUE(value != nullptr);
  value->mutable_field_descriptor()->clear_type();
  const std::vector<std::string> error_test_levels = {
    "warn",
    "vlog",
    "xxxxxx"
  };

  for (const auto& level : error_test_levels) {
    FLAGS_action_field_error_level = level;
    p4_verifier_ = P4ConfigVerifier::CreateInstance(
        test_p4_info_, test_p4_pipeline_config_);
    EXPECT_OK(p4_verifier_->Verify());
  }
}

}  // namespace hal
}  // namespace stratum
