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

// Contains unit tests for ControlInspector.

#include "stratum/p4c_backends/fpm/control_inspector.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "stratum/hal/lib/p4/p4_info_manager_mock.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/pipeline_optimizer.h"
#include "stratum/p4c_backends/fpm/switch_case_decoder_mock.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_target_info.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  The test parameter is used by some tests as a hit/miss
// indicator.
class ControlInspectorTest : public testing::TestWithParam<bool> {
 public:
  static void SetUpTestCase() {
    TestTargetInfo::SetUpTestTargetInfo();
    SetUpTestP4ModelNames();
  }
  static void TearDownTestCase() {
    TestTargetInfo::TearDownTestTargetInfo();
  }

 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format and record IR nodes that contain some P4Control methods to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));


    P4ModelNames p4_model_names = GetP4ModelNames();
    p4_model_names.set_ingress_control_name(kIngressName);
    p4_model_names.set_egress_control_name(kEgressName);
    ir_helper_->set_color_field_name("meta.enum_color");
    SetP4ModelNames(p4_model_names);
    control_inspector_ = absl::make_unique<ControlInspector>(
        &mock_p4_info_manager_,
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap(),
        &mock_switch_case_decoder_, &mock_table_map_generator_);

    // These default_mock_table_ values are suitable for most tests that don't
    // care how a P4ControlTableRef gets populated by mock_p4_info_manager_
    // data.
    default_mock_table_.mutable_preamble()->set_name("test-table");
    default_mock_table_.mutable_preamble()->set_id(1);

    pre_test_transforms_ = {IRTestHelperJson::kHitAssignMapper,
                            IRTestHelperJson::kMeterColorMapper};
  }

  // Sets up mock expectations to handle generated meter color actions.
  void SetUpMockMeterActions(const std::string& table_name) {
    ::p4::config::v1::P4Info meter_p4_info;
    ASSERT_TRUE(ir_helper_->GenerateP4Info(&meter_p4_info));
    const ::p4::config::v1::Table* test_table = nullptr;
    for (const auto& table : meter_p4_info.tables()) {
      if (table.preamble().name() == table_name) {
        test_table = &table;
        break;
      }
    }

    ASSERT_TRUE(test_table != nullptr);
    std::set<int> expected_action_ids;
    for (const auto& action_ref : test_table->action_refs()) {
      bool default_action = false;
      for (const auto& annotation : action_ref.annotations()) {
        if (annotation.find("@defaultonly") != std::string::npos) {
          default_action = true;
          break;
        }
      }
      if (!default_action) {
        expected_action_ids.insert(action_ref.id());
      }
    }
    EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
        .WillRepeatedly(Return(*test_table));

    // Every expected action should have a mock_table_map_generator_ call
    // to ReplaceActionDescriptor.  An InternalAction should also be added.
    for (const auto& action : meter_p4_info.actions()) {
      auto action_id = action.preamble().id();
      if (expected_action_ids.find(action_id) != expected_action_ids.end()) {
        EXPECT_CALL(mock_p4_info_manager_, FindActionByID(action_id))
            .WillOnce(Return(action));
        EXPECT_CALL(
            mock_table_map_generator_,
            ReplaceActionDescriptor(action.preamble().name(), _))
            .WillOnce(SaveArg<1>(&saved_action_descriptor_));
        EXPECT_CALL(
            mock_table_map_generator_,
            AddInternalAction(HasSubstr(action.preamble().name()), _))
            .WillOnce(SaveArg<0>(&saved_internal_action_name_));
        real_table_mapper_.AddAction(action.preamble().name());
      } else {
        EXPECT_CALL(mock_p4_info_manager_, FindActionByID(action_id)).Times(0);
      }
    }
    EXPECT_CALL(mock_table_map_generator_, generated_map())
        .Times(AnyNumber())
        .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));
  }

  // Sets up mock expectations to handle header type lookups.  The
  // real_table_mapper_ contains one header descriptor for
  // "hdr.ethernet", which is the only header checked by isValid P4 method
  // calls in the tested P4 programs.  The mock_table_map_generator_ returns
  // the table map generated by real_table_mapper_ as the mock output.
  void SetUpMockHeaderTypes() {
    real_table_mapper_.AddHeader("hdr.ethernet");
    real_table_mapper_.SetHeaderAttributes(
        "hdr.ethernet", P4_HEADER_ETHERNET, 0);
    EXPECT_CALL(mock_table_map_generator_, generated_map())
        .Times(AnyNumber())
        .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));
  }

  // When a ControlInspector needs to create an InternalAction, this method
  // verifies the presence of a link from the action for the original action
  // to the InternalAction.
  void VerifyInternalAction() {
    ASSERT_EQ(1, saved_action_descriptor_.action_redirects_size());
    const auto& redirect = saved_action_descriptor_.action_redirects(0);
    ASSERT_EQ(1, redirect.internal_links_size());
    EXPECT_EQ(redirect.internal_links(0).internal_action_name(),
              saved_internal_action_name_);
  }

  // ControlInspector for common test use.
  std::unique_ptr<ControlInspector> control_inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.

  // The mock P4InfoManager, SwitchCaseDecoderMock, and TableMapGeneratorMock
  // are injected into the tested ControlInspector.  The default_mock_table_
  // member provides tests with a mock return value from
  // P4InfoManager::FindTableByName.
  hal::P4InfoManagerMock mock_p4_info_manager_;
  SwitchCaseDecoderMock mock_switch_case_decoder_;
  TableMapGeneratorMock mock_table_map_generator_;
  ::p4::config::v1::Table default_mock_table_;

  // This TableMapGenerator backs the mock_table_map_generator_ with a real map
  // that has test header and/or action descriptor lookup mappings.
  TableMapGenerator real_table_mapper_;

  // This member is a list of transforms to run while getting a transformed
  // IR control node for tests.
  std::vector<IRTestHelperJson::IRControlTransforms> pre_test_transforms_;

  // These members save the mock TableMapGenerator arguments during
  // InternalAction creation.  For tests that involve multiple InternalActions,
  // only the last one is retained, but this should provide sufficient
  // verification.
  std::string saved_internal_action_name_;
  hal::P4ActionDescriptor saved_action_descriptor_;

  // These are the control names for test case support in the
  // no_table_tmp.p4 file.
  static constexpr const char* kIngressName = "ingress";
  static constexpr const char* kEgressName = "egress_stub";
  static constexpr const char* kDeparserName = "deparser_stub";

  // These are the control names for test case support in the
  // control_if_test.p4 file.
  static constexpr const char* kSimpleIf = "egress";
  static constexpr const char* kBasicIfElse = "ingress";
  static constexpr const char* kUnaryNot = "verifyChecksum";
  static constexpr const char* kNestedIfs = "computeChecksum";

  // These are the control names and other useful strings for test case
  // support in the control_apply_hit_miss_test.p4 file.
  static constexpr const char* kApplyHit = "egress";
  static constexpr const char* kApplyMiss = "ingress";
  static constexpr const char* kApplyHitTableName = "egress.test_table";
  static constexpr const char* kApplyMissTableName = "ingress.test_table";

  // These are the control names for test case support in the
  // control_misc_test.p4 file.
  static constexpr const char* kBuiltInMethod = "egress";
  static constexpr const char* kFieldCompare = "computeChecksum";
  static constexpr const char* kAssignConstant = "verifyChecksum";
  static constexpr const char* kDropNotValid = "DeparserImpl";

  // These are the control names for test case support in the
  // control_misc_test2.p4 file.
  static constexpr const char* kNopIf = "control_nop_if";
  static constexpr const char* kMeterHitHidden = "meter_hit_hidden";

  // These strings are the P4 object names for test case
  // support in the control_hit_meter.p4 file.
  static constexpr const char* kMeterHit = "meter_hit";
  static constexpr const char* kMeterMissElse = "meter_miss_else";

  // These strings are the control names and table names for test case
  // support in the pipeline_opt_block.p4 file.
  static constexpr const char* kOptimizeBlock = "ingress";
  static constexpr const char* kAclTableName = "ingress.acl_v";
  static constexpr const char* kLpmTable1Name = "ingress.lpm_1";
  static constexpr const char* kLpmTable2Name = "ingress.lpm_2";
  static constexpr const char* kLpmTable3Name = "ingress.lpm_3";

  // These strings are the P4 object names for test case
  // support in the switch_case.p4 file.
  static constexpr const char* kPuntSwitch = "normal_clone_drop";
};

// Verifies proper inspector decoding of ingress control name and type.
TEST_F(ControlInspectorTest, TestIngressNameAndType) {
  SetUpTestIR("no_table_tmp.ir.json");
  const std::string ingress_name(kIngressName);
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      ingress_name, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  EXPECT_EQ(ingress_name, control.name());
  EXPECT_EQ(hal::P4Control::P4_CONTROL_INGRESS, control.type());
}

// Verifies proper inspector decoding of egress control name and type.
TEST_F(ControlInspectorTest, TestEgressNameAndType) {
  SetUpTestIR("no_table_tmp.ir.json");
  const std::string egress_name(kEgressName);
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      egress_name, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  EXPECT_EQ(egress_name, control.name());
  EXPECT_EQ(hal::P4Control::P4_CONTROL_EGRESS, control.type());
}

// Any non-ingress or non-egress control is currently treated as unknown.
TEST_F(ControlInspectorTest, TestUnknownType) {
  SetUpTestIR("no_table_tmp.ir.json");
  const std::string deparse_name(kDeparserName);
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      deparse_name, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  EXPECT_EQ(deparse_name, control.name());
  EXPECT_EQ(hal::P4Control::P4_CONTROL_UNKNOWN, control.type());
}

// Verifies that Inspect is a nop if called a second time.
TEST_F(ControlInspectorTest, TestInspectTwice) {
  SetUpTestIR("no_table_tmp.ir.json");
  const std::string ingress_name(kIngressName);
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      ingress_name, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  EXPECT_EQ(ingress_name, control.name());

  // An attempt to inspect another control should leave the output unchanged.
  ir_control = ir_helper_->TransformP4Control(
      kEgressName, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  EXPECT_EQ(ingress_name, control.name());  // Name should not be changed.
}

// The next four tests verify the branching and block flow of the P4Control
// output.  They do not verify any detailed statement content.

// Verifies proper inspector control flow output for a simple P4 control
// if statement.
TEST_F(ControlInspectorTest, TestSimpleIf) {
  SetUpTestIR("control_if_test.ir.json");
  SetUpMockHeaderTypes();
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kSimpleIf, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(0).has_branch());
  const auto& if_statement = control.main().statements(0).branch();
  EXPECT_TRUE(if_statement.has_condition());
  EXPECT_TRUE(if_statement.has_true_block());
  EXPECT_FALSE(if_statement.has_false_block());
}

// Verifies proper inspector control flow output for a basic P4 control
// if-else statement.
TEST_F(ControlInspectorTest, TestBasicIfElse) {
  SetUpTestIR("control_if_test.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kBasicIfElse, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());

  // This control has two statements.  The first applies a table, and the
  // second is the if statement of interest to this test.
  ASSERT_EQ(2, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(1).has_branch());
  const auto& if_statement = control.main().statements(1).branch();
  EXPECT_TRUE(if_statement.has_condition());
  EXPECT_TRUE(if_statement.has_true_block());
  EXPECT_TRUE(if_statement.has_false_block());
}

// Verifies proper inspector control flow output for nested if statements.
TEST_F(ControlInspectorTest, TestNestedIfs) {
  SetUpTestIR("control_if_test.ir.json");
  SetUpMockHeaderTypes();
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kNestedIfs, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(0).has_branch());

  // The outer if should have a true block and a false block.
  const auto& outer_if = control.main().statements(0).branch();
  EXPECT_TRUE(outer_if.has_condition());
  EXPECT_TRUE(outer_if.has_true_block());
  EXPECT_TRUE(outer_if.has_false_block());

  // The inner if within the outer if's true_block also has true and
  // false blocks.
  ASSERT_EQ(1, outer_if.true_block().statements_size());
  ASSERT_TRUE(outer_if.true_block().statements(0).has_branch());
  const auto& true_inner_if = outer_if.true_block().statements(0).branch();
  EXPECT_TRUE(true_inner_if.has_condition());
  EXPECT_TRUE(true_inner_if.has_true_block());
  EXPECT_TRUE(true_inner_if.has_false_block());

  // The inner if within the outer if's false_block has a true block
  // but no false block.
  ASSERT_EQ(1, outer_if.false_block().statements_size());
  ASSERT_TRUE(outer_if.false_block().statements(0).has_branch());
  const auto& false_inner_if = outer_if.false_block().statements(0).branch();
  EXPECT_TRUE(false_inner_if.has_condition());
  EXPECT_TRUE(false_inner_if.has_true_block());
  EXPECT_FALSE(false_inner_if.has_false_block());
}

// Verifies the not_operator value in an "if (!<condition>)" statement.
TEST_F(ControlInspectorTest, TestNotOperator) {
  SetUpTestIR("control_if_test.ir.json");
  SetUpMockHeaderTypes();
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kUnaryNot, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());

  // The test expects a single if statement in the output with the
  // not_operator field set.
  ASSERT_EQ(1, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(0).has_branch());
  const auto& if_statement = control.main().statements(0).branch();
  EXPECT_TRUE(if_statement.has_condition());
  EXPECT_TRUE(if_statement.condition().not_operator());
  EXPECT_TRUE(if_statement.has_true_block());
  EXPECT_FALSE(if_statement.has_false_block());
}

// Verifies proper encoding of a header is_valid condition.
TEST_F(ControlInspectorTest, TestHeaderValid) {
  SetUpTestIR("control_if_test.ir.json");
  SetUpMockHeaderTypes();
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kSimpleIf, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());

  // The test expects an if statement with the condition representing a
  // validity check on the ethernet header, including the header name and type.
  ASSERT_EQ(1, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(0).has_branch());
  const hal::P4IfStatement& if_statement =
      control.main().statements(0).branch();
  EXPECT_TRUE(if_statement.has_condition());
  EXPECT_TRUE(if_statement.condition().has_is_valid());
  EXPECT_EQ("hdr.ethernet", if_statement.condition().is_valid().header_name());
  EXPECT_EQ(P4_HEADER_ETHERNET,
            if_statement.condition().is_valid().header_type());
}

// Verifies expected encoding of a header is_valid condition when no header
// descriptor exists.
TEST_F(ControlInspectorTest, TestHeaderValidNoDescriptor) {
  SetUpTestIR("control_if_test.ir.json");

  // Instead of calling SetUpMockHeaderTypes to populate a table map, this
  // test sets up an expectation to return an empty map.
  EXPECT_CALL(mock_table_map_generator_, generated_map())
      .Times(AnyNumber())
      .WillRepeatedly(ReturnRef(real_table_mapper_.generated_map()));

  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kSimpleIf, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());

  // The test expects an if statement with the condition representing a
  // validity check on the ethernet header, including the header name with
  // an unknown header type.
  ASSERT_EQ(1, control.main().statements_size());
  ASSERT_TRUE(control.main().statements(0).has_branch());
  const hal::P4IfStatement& if_statement =
      control.main().statements(0).branch();
  EXPECT_TRUE(if_statement.has_condition());
  EXPECT_TRUE(if_statement.condition().has_is_valid());
  EXPECT_EQ("hdr.ethernet", if_statement.condition().is_valid().header_name());
  EXPECT_EQ(P4_HEADER_UNKNOWN,
            if_statement.condition().is_valid().header_type());
}

// Tests apply and hit/miss - "if (table.apply().hit)" and
// "if (!table.apply().hit)" as determined by the test parameter.
TEST_P(ControlInspectorTest, TestApplyHitMiss) {
  SetUpTestIR("control_apply_hit_miss_test.ir.json");
  const bool is_miss = !GetParam();
  std::string tested_control = kApplyHit;
  std::string tested_table = kApplyHitTableName;
  if (is_miss) {
    tested_control = kApplyMiss;
    tested_table = kApplyMissTableName;
  }

  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      tested_control, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  ::p4::config::v1::Table test_p4_table;
  const uint32_t kTestTableID = 1234;
  test_p4_table.mutable_preamble()->set_name(tested_table);
  test_p4_table.mutable_preamble()->set_id(kTestTableID);
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(tested_table))
      .WillRepeatedly(Return(test_p4_table));
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());

  // The control output has two statements.  The first applies the table, and
  // the second is the if-statement test for the hit/miss.
  ASSERT_EQ(2, control.main().statements_size());
  const hal::P4ControlStatement& apply_statement = control.main().statements(0);
  ASSERT_TRUE(apply_statement.has_apply());
  const std::string kExpectedTable = tested_table;
  EXPECT_EQ(kExpectedTable, apply_statement.apply().table_name());
  EXPECT_EQ(kTestTableID, apply_statement.apply().table_id());

  const hal::P4ControlStatement& if_statement = control.main().statements(1);
  ASSERT_TRUE(if_statement.has_branch());
  ASSERT_TRUE(if_statement.branch().has_condition());
  EXPECT_EQ(is_miss, if_statement.branch().condition().not_operator());
  ASSERT_TRUE(if_statement.branch().condition().has_hit());
  EXPECT_EQ(kExpectedTable,
            if_statement.branch().condition().hit().table_name());
  EXPECT_EQ(kTestTableID, if_statement.branch().condition().hit().table_id());
  EXPECT_TRUE(if_statement.branch().has_true_block());
  EXPECT_FALSE(if_statement.branch().has_false_block());
}

// The ControlInspector currently has limited built-in method
// support, so the goal of this test is to verify that it produces a sane
// statement without crashing.
TEST_F(ControlInspectorTest, TestBuiltInMethodCall) {
  SetUpTestIR("control_misc_test.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kBuiltInMethod, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  EXPECT_EQ(1, control.main().statements_size());
}

// The ControlInspector currently has limited compare operator
// support, so the goal of this test is to verify that it produces a sane
// statement without crashing.
TEST_F(ControlInspectorTest, TestFieldCompare) {
  SetUpTestIR("control_misc_test.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kFieldCompare, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  EXPECT_EQ(1, control.main().statements_size());
}

// The ControlInspector currently has limited assignment
// support, so the goal of this test is to verify that it produces a sane
// statement without crashing.
TEST_F(ControlInspectorTest, TestAssignConstant) {
  SetUpTestIR("control_misc_test.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kAssignConstant, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  EXPECT_EQ(1, control.main().statements_size());
}

// Tests dropping packet when a header is invalid,
// coded in P4 as: if (!header.isValid()) mark_to_drop();
TEST_F(ControlInspectorTest, TestDropNotValid) {
  SetUpTestIR("control_misc_test.ir.json");
  SetUpMockHeaderTypes();
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kDropNotValid, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());

  const hal::P4ControlStatement& if_statement = control.main().statements(0);
  ASSERT_TRUE(if_statement.has_branch());
  ASSERT_TRUE(if_statement.branch().has_condition());
  EXPECT_TRUE(if_statement.branch().condition().not_operator());
  EXPECT_TRUE(if_statement.branch().has_true_block());
  EXPECT_FALSE(if_statement.branch().has_false_block());

  // The if-statement's true_block should be marked to drop.
  ASSERT_EQ(1, if_statement.branch().true_block().statements_size());
  EXPECT_TRUE(if_statement.branch().true_block().statements(0).drop());
}

// Tests that P4Control output has no superfluous if statements when
// everything under the if moves to a metering action.
TEST_F(ControlInspectorTest, TestNopIf) {
  SetUpTestIR("control_misc_test2.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kNopIf, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  SetUpMockMeterActions("control_nop_if.table1");
  control_inspector_->Inspect(*ir_control);
  const hal::P4Control& control = control_inspector_->control();

  // The only statement should apply the table.
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());
  EXPECT_TRUE(control.main().statements(0).has_apply());
  SCOPED_TRACE("");
  VerifyInternalAction();
}

// Tests output when input is an optimized P4Control.
TEST_F(ControlInspectorTest, TestOptimizedControl) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kOptimizeBlock, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);

  // The PipelineOptimizer turns ir_control into its optimized form.
  PipelineOptimizer optimizer(ir_helper_->mid_end_refmap(),
                              ir_helper_->mid_end_typemap());
  const IR::P4Control* optimized_control = optimizer.Optimize(*ir_control);
  ASSERT_NE(ir_control, optimized_control);

  // The mock_p4_info_manager_ needs to return specific information
  // for each table in the control.
  std::vector<std::string> test_table_names =
      {kAclTableName, kLpmTable1Name, kLpmTable2Name, kLpmTable3Name};
  std::vector<::p4::config::v1::Table> test_tables(test_table_names.size());
  int dummy_table_id = default_mock_table_.preamble().id();
  for (int t = 0; t < test_table_names.size(); ++t) {
    test_tables[t].mutable_preamble()->set_name(test_table_names[t]);
    test_tables[t].mutable_preamble()->set_id(++dummy_table_id);
    EXPECT_CALL(mock_p4_info_manager_, FindTableByName(test_table_names[t]))
        .WillOnce(Return(test_tables[t]));
  }

  control_inspector_->Inspect(*optimized_control);
  const hal::P4Control& control = control_inspector_->control();

  // The test expects 2 statements in the output control, an apply for the
  // ACL table and a fixed_pipeline for the 3 LPM tables.
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(2, control.main().statements_size());

  const hal::P4ControlStatement& apply_statement = control.main().statements(0);
  ASSERT_TRUE(apply_statement.has_apply());
  const ::p4::config::v1::Preamble& acl_preamble = test_tables[0].preamble();
  EXPECT_EQ(acl_preamble.name(), apply_statement.apply().table_name());
  EXPECT_EQ(acl_preamble.id(), apply_statement.apply().table_id());

  const hal::P4ControlStatement& pipeline_statement =
      control.main().statements(1);
  ASSERT_TRUE(pipeline_statement.has_fixed_pipeline());
  const hal::FixedPipelineTables& fixed_tables =
      pipeline_statement.fixed_pipeline();
  EXPECT_EQ(P4Annotation::L3_LPM, fixed_tables.pipeline_stage());
  ASSERT_EQ(test_tables.size() - 1, fixed_tables.tables_size());
  for (int t = 0; t < fixed_tables.tables_size(); ++t) {
    const ::p4::config::v1::Preamble& table_info =
        test_tables[t + 1].preamble();
    SCOPED_TRACE(absl::StrFormat("Expected table %s does not appear in expected"
                              " order in pipeline stage, found table %s",
                              table_info.name().c_str(),
                              fixed_tables.tables(t).table_name().c_str()));
    EXPECT_EQ(table_info.name(), fixed_tables.tables(t).table_name());
    EXPECT_EQ(table_info.id(), fixed_tables.tables(t).table_id());
    EXPECT_EQ(P4Annotation::L3_LPM, fixed_tables.tables(t).pipeline_stage());
  }
}

// Verifies that control_inspector_ hands off IR::SwitchStatement processing
// to the SwitchCaseDecoder.
TEST_F(ControlInspectorTest, TestSwitchStatement) {
  SetUpTestIR("switch_case.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kPuntSwitch, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);

  // The mock_switch_case_decoder_ needs to return this dummy applied table.
  IR::Annotations empty_annotations_;
  IR::TableProperties empty_properties_;
  const cstring tblName = IR::ID(default_mock_table_.preamble().name());
  std::unique_ptr<IR::P4Table> dummy_table(
      new IR::P4Table(tblName, &empty_annotations_, &empty_properties_));
  EXPECT_CALL(mock_switch_case_decoder_, Decode(_));
  EXPECT_CALL(mock_switch_case_decoder_, applied_table())
      .WillRepeatedly(Return(dummy_table.get()));
  EXPECT_CALL(mock_p4_info_manager_, FindTableByName(_))
      .WillRepeatedly(Return(default_mock_table_));  // Default table is OK.
  control_inspector_->Inspect(*ir_control);

  // The test expects 1 statement in the output control, an apply for the
  // mocked table in the switch statement expression.
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());

  const hal::P4ControlStatement& apply_statement = control.main().statements(0);
  ASSERT_TRUE(apply_statement.has_apply());
  const ::p4::config::v1::Preamble& apply_preamble =
      default_mock_table_.preamble();
  EXPECT_EQ(apply_preamble.name(), apply_statement.apply().table_name());
  EXPECT_EQ(apply_preamble.id(), apply_statement.apply().table_id());
}

// Tests output for meter action after table hit.
TEST_F(ControlInspectorTest, TestTableHitMeterAction) {
  SetUpTestIR("control_hit_meter.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kMeterHit, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  SetUpMockMeterActions("meter_hit.hit_table");

  control_inspector_->Inspect(*ir_control);
  SCOPED_TRACE("");
  VerifyInternalAction();
}

// Tests output for meter action in else clause after table miss.
TEST_F(ControlInspectorTest, TestTableMissElseMeterAction) {
  SetUpTestIR("control_hit_meter.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kMeterMissElse, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  SetUpMockMeterActions("meter_miss_else.miss_else_table");

  control_inspector_->Inspect(*ir_control);
  SCOPED_TRACE("");
  VerifyInternalAction();
}

// Tests output for hidden table hit with a metering condition.
TEST_F(ControlInspectorTest, TestHiddenTableMeterAction) {
  SetUpTestIR("control_misc_test2.ir.json");
  const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
      kMeterHitHidden, pre_test_transforms_);
  ASSERT_TRUE(ir_control != nullptr);
  const std::string kHiddenTable = "meter_hit_hidden.hidden_table";
  SetUpMockMeterActions(kHiddenTable);
  control_inspector_->Inspect(*ir_control);

  // The control output has one statement, which applies the table.  Since
  // the applied table is hidden, the meter logic migrates to the actions,
  // and the output P4Control only records the hidden table apply.
  const hal::P4Control& control = control_inspector_->control();
  ASSERT_TRUE(control.has_main());
  ASSERT_EQ(1, control.main().statements_size());
  const hal::P4ControlStatement& apply_statement = control.main().statements(0);
  ASSERT_TRUE(apply_statement.has_apply());
  EXPECT_EQ(kHiddenTable, apply_statement.apply().table_name());
  SCOPED_TRACE("");
  VerifyInternalAction();
}

INSTANTIATE_TEST_SUITE_P(
  HitMissFlag,
  ControlInspectorTest,
  ::testing::Bool()
);

}  // namespace p4c_backends
}  // namespace stratum
