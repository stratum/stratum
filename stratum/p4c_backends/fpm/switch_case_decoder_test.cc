// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for SwitchCaseDecoder.

#include "stratum/p4c_backends/fpm/switch_case_decoder.h"

#include <map>
#include <memory>
#include <string>

#include "google/protobuf/util/message_differencer.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::ReturnRef;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class SwitchCaseDecoderTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  void SetUp() override {
    // SetUp initializes action_name_map_ with internal to external action
    // name mappings for all test files.
    action_name_map_["case1_clone_green"] = "case1_clone_green";
    action_name_map_["case2_drop_not_green"] = "case2_drop_not_green";
    action_name_map_["case1_drop_not_green"] = "case1_drop_not_green";
    action_name_map_["case2_clone_not_not_green"] =
        "case2_clone_not_not_green";
    action_name_map_["case1_nested_if"] = "case1_nested_if";
    action_name_map_["case2_action"] = "case2_action";
    action_name_map_["case1_assign"] = "case1_assign";
    action_name_map_["case1_condition_or"] = "case1_condition_or";
    action_name_map_["case1_fallthru"] = "case1_fallthru";
    action_name_map_["case2_drop_red"] = "case2_drop_red";
    action_name_map_["case1_color_not_enum"] = "case1_color_not_enum_";
    action_name_map_["case1_call_unsupported"] = "case1_call_unsupported";
  }

  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a SwitchCaseDecoder test instance.  The caller
  // should be sure action_name_map_ contains the desired entries before this
  // function runs.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    switch_case_decoder_ = absl::make_unique<SwitchCaseDecoder>(
        action_name_map_, ir_helper_->mid_end_refmap(),
        ir_helper_->mid_end_typemap(), &mock_table_mapper_);

    // This is the metadata field normally used to represent the meter color.
    meta_color_field_ = "meta.enum_color";
  }

  // Returns what should be the first and only SwitchStatement in the input
  // control.  The test control is pre-processed with a MeterColorMapper
  // via TransformP4Control to do MeterColorStatement transforms where
  // applicable.
  const IR::SwitchStatement* FindSwitchStatement(const std::string& control) {
    ir_helper_->set_color_field_name(meta_color_field_);
    const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
        control, {IRTestHelperJson::kMeterColorMapper});
    if (ir_control != nullptr) {
      for (auto statement : ir_control->body->components) {
        if (statement->is<IR::SwitchStatement>())
          return statement->to<IR::SwitchStatement>();
      }
    }
    return nullptr;
  }

  // Populates expected_green_ and expected_red_yellow_ with common message
  // content for matching in mock_table_mapper_ expectations.
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

  // SwitchCaseDecoder instance for test use; created by SetUpTestIR.
  std::unique_ptr<SwitchCaseDecoder> switch_case_decoder_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  TableMapGeneratorMock mock_table_mapper_;  // Mock for testing output.

  // This string is the local metadata field name that tests choose to
  // represent the meter color in IRTestHelperJson::TransformP4Control.
  std::string meta_color_field_;

  // This map contains the internal-to-external action name translations for
  // the test IR.  Tests should set it as needed before calling SetUpTestIR.
  std::map<std::string, std::string> action_name_map_;

  // These messages are populated by SetUpExpectedColorActions.
  hal::P4ActionDescriptor::P4MeterColorAction expected_green_;
  hal::P4ActionDescriptor::P4MeterColorAction expected_red_yellow_;
};

// Matcher for mock_table_mapper_.AddMeterColorAction P4MeterColorAction.
MATCHER_P(MatchColorAction, expected_message, "") {
  hal::P4ActionDescriptor parsed_descriptor;
  bool parsed_ok = ParseProtoFromString(arg, &parsed_descriptor).ok();
  if (!parsed_ok) return false;
  google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      google::protobuf::util::MessageDifferencer::AS_SET);
  return msg_differencer.Compare(
      expected_message, parsed_descriptor.color_actions(0));
}

// Verifies behavior for clone-on-green, drop-on-non-green actions.
TEST_F(SwitchCaseDecoderTest, TestGreenCases) {
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("normal_clone_drop");
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_EQ(nullptr, switch_case_decoder_->applied_table());

  // The mock expectations below verify that switch_case_decoder_->Decode calls
  // the mock_table_mapper_ with the correct AddMeterColorAction input.
  SetUpExpectedColorActions();
  EXPECT_CALL(mock_table_mapper_, AddMeterColorActionsFromString(
      "case1_clone_green", MatchColorAction(expected_green_))).Times(1);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorActionsFromString(
      "case2_drop_not_green", MatchColorAction(expected_red_yellow_))).Times(1);

  switch_case_decoder_->Decode(*test_statement);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_NE(nullptr, switch_case_decoder_->applied_table());
}

// Verifies behavior for inverted conditions.
TEST_F(SwitchCaseDecoderTest, TestInvertedConditions) {
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("inverted_conditions");
  ASSERT_TRUE(test_statement != nullptr);

  // The mock expectations below verify that switch_case_decoder_->Decode calls
  // the mock_table_mapper_ with the correct AddMeterColorAction input.
  SetUpExpectedColorActions();
  EXPECT_CALL(mock_table_mapper_, AddMeterColorActionsFromString(
      "case1_drop_not_green", MatchColorAction(expected_red_yellow_))).Times(1);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorActionsFromString(
      "case2_clone_not_not_green", MatchColorAction(expected_green_)))
      .Times(1);

  switch_case_decoder_->Decode(*test_statement);
  EXPECT_EQ(0, ::errorCount());
  EXPECT_NE(nullptr, switch_case_decoder_->applied_table());
}

// Verifies behavior when action_name_map_ does not contain the necessary
// entries.
TEST_F(SwitchCaseDecoderTest, TestNoExternalNameMap) {
  action_name_map_.clear();  // Clears all internal->external name mapping.
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("normal_clone_drop");
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior when a switch case contains an unsupported statement.
TEST_F(SwitchCaseDecoderTest, TestUnsupportedStatement) {
  SetUpTestIR("switch_case_errors.ir.json");
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("egress_assign");
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior with unexpected case label fall-through.
TEST_F(SwitchCaseDecoderTest, TestUnexpectedFallThrough) {
  SetUpTestIR("switch_case_errors.ir.json");
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("fall_through_case");
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior with unexpected switch expression.
TEST_F(SwitchCaseDecoderTest, TestUnexpectedSwitchExpression) {
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* setup_statement =
      FindSwitchStatement("normal_clone_drop");
  ASSERT_TRUE(setup_statement != nullptr);

  // This test constructs a new SwitchStatement for testing, using the cases
  // from the IR setup with a replacement expression that doesn't satisfy
  // the P4 table.apply().action_run requirement.
  std::unique_ptr<IR::PathExpression> test_expression(
      new IR::PathExpression("tmp_var"));
  std::unique_ptr<IR::SwitchStatement> test_statement(
      new IR::SwitchStatement(test_expression.get(), setup_statement->cases));
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
  EXPECT_EQ(nullptr, switch_case_decoder_->applied_table());
}

// Verifies behavior with unexpected default case.
TEST_F(SwitchCaseDecoderTest, TestUnexpectedDefaultCase) {
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* setup_statement =
      FindSwitchStatement("normal_clone_drop");
  ASSERT_TRUE(setup_statement != nullptr);

  // This test constructs a new SwitchStatement for testing, using the original
  // expression from the IR setup with all cases replaced by a default.
  IR::DefaultExpression default_label;
  IR::BlockStatement default_statement;
  IR::SwitchCase default_case(&default_label, &default_statement);
  IR::Vector<IR::SwitchCase> new_cases;
  new_cases.push_back(&default_case);
  std::unique_ptr<IR::SwitchStatement> test_statement(new IR::SwitchStatement(
      setup_statement->expression, new_cases));
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior with unexpected case label path expression type.
TEST_F(SwitchCaseDecoderTest, TestUnexpectedCaseExpressionType) {
  SetUpTestIR("switch_case.ir.json");
  const IR::SwitchStatement* setup_statement =
      FindSwitchStatement("normal_clone_drop");
  ASSERT_TRUE(setup_statement != nullptr);

  // This test constructs a new SwitchStatement for testing, using the original
  // expression from the IR setup with a replacement case label that is a
  // PathExpression type for a parser state instead of an action type.
  std::unique_ptr<IR::PathExpression> test_label(
      new IR::PathExpression(IR::ID(IR::ParserState::reject)));
  IR::BlockStatement case_block;
  IR::SwitchCase test_case(test_label.get(), &case_block);
  IR::Vector<IR::SwitchCase> new_cases;
  new_cases.push_back(&test_case);
  std::unique_ptr<IR::SwitchStatement> test_statement(new IR::SwitchStatement(
      setup_statement->expression, new_cases));
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

// Verifies behavior with non-enum color field.
TEST_F(SwitchCaseDecoderTest, TestColorNotEnum) {
  // The "meta.color" in this test is a bit field type, not an enum.
  SetUpTestIR("switch_case_errors2.ir.json");
  meta_color_field_ = "meta.color";
  const IR::SwitchStatement* test_statement =
      FindSwitchStatement("color_not_enum");
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, AddMeterColorAction(_, _)).Times(0);
  switch_case_decoder_->Decode(*test_statement);
  EXPECT_NE(0, ::errorCount());
}

}  // namespace p4c_backends
}  // namespace stratum
