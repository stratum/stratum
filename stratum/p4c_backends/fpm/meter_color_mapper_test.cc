// Copyright 2019 Google LLC
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

// Contains unit tests for MeterColorMapper.

#include "stratum/p4c_backends/fpm/meter_color_mapper.h"

#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/util/message_differencer.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

using ::testing::AnyNumber;
using ::testing::Combine;
using ::testing::Range;
using ::testing::ReturnRef;
using ::testing::Values;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class MeterColorMapperTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a MeterColorMapper test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));

    meter_color_mapper_ = absl::make_unique<MeterColorMapper>(
        ir_helper_->mid_end_refmap(),
        ir_helper_->mid_end_typemap(), &mock_table_mapper_);

    // The lookup_table_mapper_ is set up to be able to find a field
    // descriptor for the metadata color field.  Its generated_map will be
    // returned via gmock expectations to satisfy any generated_map queries
    // to mock_table_mapper_.
    lookup_table_mapper_.AddField("meta.enum_color");
    lookup_table_mapper_.SetFieldType("meta.enum_color", P4_FIELD_TYPE_COLOR);
    EXPECT_CALL(mock_table_mapper_, generated_map())
        .Times(AnyNumber())
        .WillRepeatedly(ReturnRef(lookup_table_mapper_.generated_map()));
  }

  // Populates expected_green_ and expected_red_yellow_ with common message
  // content for matching meter_color_actions from the transformed
  // MeterColorStatement output.
  void SetUpExpectedColorActions() {
    expected_green_.Clear();
    expected_green_.add_colors(P4_METER_GREEN);
    hal::P4ActionDescriptor::P4ActionInstructions* green_ops =
        expected_green_.add_ops();
    green_ops->add_primitives(P4_ACTION_OP_CLONE);

    expected_red_yellow_.Clear();
    expected_red_yellow_.add_colors(P4_METER_RED);
    expected_red_yellow_.add_colors(P4_METER_YELLOW);
    hal::P4ActionDescriptor::P4ActionInstructions* red_yellow_ops =
        expected_red_yellow_.add_ops();
    red_yellow_ops->add_primitives(P4_ACTION_OP_DROP);
  }

  // Populates the output statement_actions with data from the
  // MeterColorStatements in the input control.  The output vector has
  // one entry for each MeterColorStatement in the input control.
  void GetMeterColorStatementActions(
      const IR::P4Control& control,
      std::vector<hal::P4ActionDescriptor>* statement_actions) {
    statement_actions->clear();
    forAllMatching<IR::MeterColorStatement>(
        control.body, [&](const IR::MeterColorStatement* statement) {
      hal::P4ActionDescriptor parsed_descriptor;
      std::string meter_actions_string(statement->meter_color_actions);
      bool parsed_ok =
          ParseProtoFromString(meter_actions_string, &parsed_descriptor).ok();
      ASSERT_TRUE(parsed_ok);
      statement_actions->push_back(parsed_descriptor);
    });
  }

  // MeterColorMapper instance for test use; created by SetUpTestIR.
  std::unique_ptr<MeterColorMapper> meter_color_mapper_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  TableMapGeneratorMock mock_table_mapper_;  // Mock for testing output.

  // This TableMapGenerator backs the mock_table_mapper_ with a real map that
  // has test field lookup mappings.
  TableMapGenerator lookup_table_mapper_;

  // These messages are populated by SetUpExpectedColorActions.
  hal::P4ActionDescriptor::P4MeterColorAction expected_green_;
  hal::P4ActionDescriptor::P4MeterColorAction expected_red_yellow_;
};

// MeterColorMapperNoTransformTest is a parameterized subclass of
// MeterColorMapperTest.  It tests P4 control blocks that MeterColorMapper
// should not transform.  It covers tests where a control has no metering
// logic as well as tests where the metering logic has an error.  See
// INSTANTIATE_TEST_CASE_P near the end if this file for the parameter formats.
class MeterColorMapperNoTransformTest
  : public MeterColorMapperTest,
    public testing::WithParamInterface<
        std::tuple<std::string, std::string>> {
 protected:
  // Test parameter accessors.
  const std::string& test_ir_file() const {
    return ::testing::get<0>(GetParam());
  }
  const std::string& control_name() const {
    return ::testing::get<1>(GetParam());
  }
};

// MeterColorMapperTransformErrorTest is a redefined type for
// MeterColorMapperNoTransformTest.  It is specifically for testing
// transforms that fail due to some metering statement error.
typedef MeterColorMapperNoTransformTest MeterColorMapperTransformErrorTest;

// InspectIfColorTest is a parameterized subclass of MeterColorMapperTest.
// See INSTANTIATE_TEST_CASE_P at the end if this file for the parameter
// formats.
class InspectIfColorTest
  : public MeterColorMapperTest,
    public testing::WithParamInterface<
        std::tuple<std::string, std::string, int>> {
 protected:
  // Finds the Nth statement in control_name, where N is statement_index,
  // assures that it is an IR::IfStatement, and returns a reference to the
  // statement.
  const IR::IfStatement& SetUpIfStatement(
      const std::string& control_name, int statement_index) {
    const IR::P4Control* test_control = ir_helper_->GetP4Control(control_name);
    if (test_control == nullptr) {
      LOG(FATAL) << "Unable to find test control " << control_name;
    }
    auto ir_statement = test_control->body->
        components[statement_index]->to<IR::IfStatement>();
    if (ir_statement == nullptr) {
      LOG(FATAL) << "Test statement at index " << statement_index
                 << " is not an IfStatement";
    }
    return *ir_statement;
  }

  // Test parameter accessors.
  const std::string& test_ir_file() const {
    return ::testing::get<0>(GetParam());
  }
  const std::string& control_name() const {
    return ::testing::get<1>(GetParam());
  }
  int statement_index() const { return ::testing::get<2>(GetParam()); }
};

// These types use the InspectIfColorTest fixture with different
// parameter sets.
typedef InspectIfColorTest InspectValidColorsTest;
typedef InspectIfColorTest InspectUnsupportedColorsTest;
typedef InspectIfColorTest NoColorInspectTest;

// Matcher for mock_table_mapper_.AddMeterColorAction P4MeterColorAction.
MATCHER_P(MatchColorAction, expected_message, "") {
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  return msg_differencer.Compare(expected_message, arg);
}

// Tests basic clone-on-green condition.
TEST_F(MeterColorMapperTest, TestGreen) {
  SetUpTestIR("meter_colors.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_if_green");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[0].color_actions(0)));
}

// Tests basic drop-on-yellow condition.
TEST_F(MeterColorMapperTest, TestYellow) {
  SetUpTestIR("meter_colors.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_if_yellow");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  expected_red_yellow_.clear_colors();
  expected_red_yellow_.add_colors(P4_METER_YELLOW);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_red_yellow_, statement_actions[0].color_actions(0)));
}

// Tests basic drop-on-red condition.
TEST_F(MeterColorMapperTest, TestRed) {
  SetUpTestIR("meter_colors.ir.json");
  const IR::P4Control* test_control = ir_helper_->GetP4Control("meter_if_red");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  expected_red_yellow_.clear_colors();
  expected_red_yellow_.add_colors(P4_METER_RED);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_red_yellow_, statement_actions[0].color_actions(0)));
}

// Verifies behavior for clone-on-green, drop-on-non-green actions within
// switch statement cases.
TEST_F(MeterColorMapperTest, TestGreenCases) {
  SetUpTestIR("switch_case.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("normal_clone_drop");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(2, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[0].color_actions(0)));
  ASSERT_EQ(1, statement_actions[1].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_red_yellow_, statement_actions[1].color_actions(0)));
}

// Verifies behavior for inverted conditions within switch statement cases.
TEST_F(MeterColorMapperTest, TestInvertedConditions) {
  SetUpTestIR("switch_case.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("inverted_conditions");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(2, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_red_yellow_, statement_actions[0].color_actions(0)));
  ASSERT_EQ(1, statement_actions[1].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[1].color_actions(0)));
}

// Verifies behavior for meter condition nested in another IfStatement.
TEST_F(MeterColorMapperTest, TestNestedIf) {
  SetUpTestIR("meter_color_nested_ifs.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_if_in_if");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[0].color_actions(0)));
}

// Verifies behavior for a valid meter statement after one that is unsupported.
TEST_F(MeterColorMapperTest, TestValidMeterAfterUnsupported) {
  SetUpTestIR("meter_color_errors1.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_valid_after_unsupported");
  ASSERT_TRUE(test_control != nullptr);

  // The valid meter statement should be transformed, but the unsupported
  // statement should report an error.
  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_NE(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(1, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[0].color_actions(0)));
}

// Verifies behavior for meter condition in an if-else statement.
TEST_F(MeterColorMapperTest, TestMeterIfElse) {
  SetUpTestIR("meter_color_if_else.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_if_else");
  ASSERT_TRUE(test_control != nullptr);

  auto transformed_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_NE(test_control, transformed_control);
  EXPECT_EQ(0, ::errorCount());
  std::vector<hal::P4ActionDescriptor> statement_actions;
  GetMeterColorStatementActions(*transformed_control, &statement_actions);
  ASSERT_EQ(1, statement_actions.size());
  SetUpExpectedColorActions();
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  ASSERT_EQ(2, statement_actions[0].color_actions_size());
  EXPECT_TRUE(msg_differencer.Compare(
      expected_green_, statement_actions[0].color_actions(0)));
  EXPECT_TRUE(msg_differencer.Compare(
      expected_red_yellow_, statement_actions[0].color_actions(1)));
}

// Verifies behavior when the metadata color has no field descriptor.
TEST_F(MeterColorMapperTest, TestNoColorFieldDescriptor) {
  SetUpTestIR("switch_case.ir.json");
  TableMapGenerator empty_table_mapper;
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(empty_table_mapper.generated_map()));
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("normal_clone_drop");
  ASSERT_TRUE(test_control != nullptr);
  auto out_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_EQ(test_control, out_control);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior when the metadata color has a table map entry without
// a field descriptor.
TEST_F(MeterColorMapperTest, TestWrongDescriptorContent) {
  SetUpTestIR("switch_case.ir.json");

  // The color metadata field lookup will find an action descriptor.
  TableMapGenerator test_table_mapper;
  test_table_mapper.AddAction("meta.enum_color");
  EXPECT_CALL(mock_table_mapper_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(test_table_mapper.generated_map()));

  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("normal_clone_drop");
  ASSERT_TRUE(test_control != nullptr);
  auto out_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_EQ(test_control, out_control);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior when the metadata color's field descriptor has an
// unexpected field type.
TEST_F(MeterColorMapperTest, TestWrongFieldDescriptorType) {
  SetUpTestIR("switch_case.ir.json");
  lookup_table_mapper_.SetFieldType("meta.enum_color", P4_FIELD_TYPE_VRF);
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("normal_clone_drop");
  ASSERT_TRUE(test_control != nullptr);
  auto out_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_EQ(test_control, out_control);
  EXPECT_NE(0, ::errorCount());
}

// Verifies MeterColorMapper::RunPreTestTransform produces transformed output.
TEST_F(MeterColorMapperTest, TestRunPreTestTransform) {
  SetUpTestIR("meter_colors.ir.json");
  const IR::P4Control* test_control =
      ir_helper_->GetP4Control("meter_if_green");
  ASSERT_TRUE(test_control != nullptr);
  auto transformed_control = MeterColorMapper::RunPreTestTransform(
      *test_control, "meta.enum_color", ir_helper_->mid_end_refmap(),
      ir_helper_->mid_end_typemap());
  EXPECT_NE(test_control, transformed_control);
  EXPECT_NE(nullptr, transformed_control);
  EXPECT_EQ(0, ::errorCount());
}

// Tests controls that have no metering logic.  No transform should occur,
// and no errors should be recorded.
TEST_P(MeterColorMapperNoTransformTest, TestNoTransform) {
  SetUpTestIR(test_ir_file());
  const IR::P4Control* test_control = ir_helper_->GetP4Control(control_name());
  ASSERT_TRUE(test_control != nullptr);
  auto out_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_EQ(test_control, out_control);
  EXPECT_EQ(0, ::errorCount());
}

// Tests for unsupported or invalid transforms that have a common setup
// sequence and expect a single error to abort the transform.
TEST_P(MeterColorMapperTransformErrorTest, TestTransformError) {
  SetUpTestIR(test_ir_file());
  const IR::P4Control* test_control = ir_helper_->GetP4Control(control_name());
  ASSERT_TRUE(test_control != nullptr);
  auto out_control = meter_color_mapper_->Apply(*test_control);
  EXPECT_EQ(test_control, out_control);
  EXPECT_NE(0, ::errorCount());
}

// Tests IfStatements with various valid color comparisons.
TEST_P(InspectValidColorsTest, TestValidColorConditions) {
  SetUpTestIR(test_ir_file());
  IfStatementColorInspector test_inspector;
  EXPECT_TRUE(test_inspector.CanTransform(
      SetUpIfStatement(control_name(), statement_index())));
  EXPECT_EQ(0, ::errorCount());
}

// Tests IfStatements with various unsupported color comparisons.
TEST_P(InspectUnsupportedColorsTest, TestUnsupportedColorConditions) {
  SetUpTestIR(test_ir_file());
  IfStatementColorInspector test_inspector;
  EXPECT_FALSE(test_inspector.CanTransform(
      SetUpIfStatement(control_name(), statement_index())));
  EXPECT_LT(0, ::errorCount());
}

// Tests IfStatements with conditions that are not color comparisons.
TEST_P(NoColorInspectTest, TestNoColorMeterConditions) {
  SetUpTestIR(test_ir_file());
  IfStatementColorInspector test_inspector;
  EXPECT_FALSE(test_inspector.CanTransform(
      SetUpIfStatement(control_name(), statement_index())));
  EXPECT_EQ(0, ::errorCount());
}

// This set of test parameters is for transform attempts that have no
// metering logic.  Their purpose is to make sure MeterColorMapper doesn't
// report unexpected errors for normal control logic.  The first member of
// the parameter tuple is the name of the JSON IR file to load for the test,
// and the second member is the name of the control for test input.
INSTANTIATE_TEST_CASE_P(
    TransformErrorTests,
    MeterColorMapperNoTransformTest,
    Values(
        std::make_tuple("control_apply_hit_miss_test.ir.json", "egress"),
        std::make_tuple("control_apply_hit_miss_test.ir.json", "ingress"),
        std::make_tuple("control_if_test.ir.json", "egress"),
        std::make_tuple("control_if_test.ir.json", "ingress"),
        std::make_tuple("control_if_test.ir.json", "verifyChecksum"),
        std::make_tuple("control_if_test.ir.json", "computeChecksum"),
        std::make_tuple("control_misc_test.ir.json", "egress"),
        std::make_tuple("control_misc_test.ir.json", "ingress"),
        std::make_tuple("control_misc_test.ir.json", "verifyChecksum"),
        std::make_tuple("control_misc_test.ir.json", "computeChecksum")
    ));

// This set of test parameters is for transform attempts that produce errors.
// The first member of the tuple is the name of the JSON IR file to load for
// the test, and the second member is the name of the control for test input.
INSTANTIATE_TEST_CASE_P(
    TransformErrorTests,
    MeterColorMapperTransformErrorTest,
    Values(
        std::make_tuple("meter_color_errors1.ir.json", "meter_and_apply"),
        std::make_tuple("meter_color_errors1.ir.json", "meter_assign"),
        std::make_tuple("meter_color_if_else.ir.json",
                        "meter_if_else_false_bad"),
        std::make_tuple("meter_color_if_else.ir.json",
                        "meter_if_else_true_bad"),
        std::make_tuple("meter_color_if_else.ir.json", "meter_if_elseif_else"),
        std::make_tuple("meter_color_nested_ifs.ir.json", "if_in_meter_if"),
        std::make_tuple("switch_case_errors.ir.json", "bad_condition"),
        std::make_tuple("switch_case_errors.ir.json", "ingress_nested_if"),
        std::make_tuple("switch_case_errors2.ir.json",
                        "unsupported_function_test")
    ));

// This set of test parameters tests IR::IfStatements with valid meter
// color conditions.  The test parameter is a tuple, with these members:
//  1) String containing the IR JSON file to load for testing.
//  2) String naming the control that contains a series of IR::IfStatements
//     for testing.
//  3) Index within the control body of the IR::IfStatement to be tested.
INSTANTIATE_TEST_CASE_P(
    ValidColorParams,
    InspectValidColorsTest,
    // Tests the first 7 IfStatements from control "ifs_with_transforms"
    // in the file "if_color_test.ir.json".
    Combine(
        Values("if_color_test.ir.json"),
        Values("ifs_with_transforms"),
        Range(0, 7)));

// This set of test parameters tests IR::IfStatements with unsupported metering
// conditions.  The tuple format is the same as ValidColorParams above.
INSTANTIATE_TEST_CASE_P(
    UnsupportedColorParams,
    InspectUnsupportedColorsTest,
    // Tests the IfStatements from control "ifs_with_errors"
    // in the file "if_color_test_errors.ir.json".
    Combine(
        Values("if_color_test_errors.ir.json"),
        Values("ifs_with_errors"),
        Range(1, 4)));

// This set of test parameters tests IR::IfStatements with non-metering
// conditions.  The tuple format is the same as ValidColorParams above.
INSTANTIATE_TEST_CASE_P(
    NoInspectParams,
    NoColorInspectTest,
    // Tests the first 10 IfStatements from control "ifs_with_no_transforms"
    // in the file "if_color_test.ir.json".
    Combine(
        Values("if_color_test.ir.json"),
        Values("ifs_with_no_transforms"),
        Range(0, 10)));

// As above, accounting for discontinuities in the IfStatement index range
// due to p4c's insertion of temporary variables.
INSTANTIATE_TEST_CASE_P(
    NoInspectParamsWithTemporaries,
    NoColorInspectTest,
    // These values account for skips due to p4c's insertion of temporary
    // values for evaluating table hits.
    Values(
        std::make_tuple("if_color_test.ir.json", "ifs_with_no_transforms", 11),
        std::make_tuple("if_color_test.ir.json", "ifs_with_no_transforms", 13)
    ));

}  // namespace p4c_backends
}  // namespace stratum
