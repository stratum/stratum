// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for HitAssignMapper.

#include <memory>
#include <tuple>

#include "stratum/p4c_backends/fpm/hit_assign_mapper.h"

#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_inspectors.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

using ::testing::Values;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  See INSTANTIATE_TEST_SUITE_P near the end if this file
// for the parameter format.
class HitAssignMapperTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, int>> {
 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a HitAssignMapper test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    test_inspector_ = absl::make_unique<HitAssignMapper>(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  }

  // Test parameter accessors.
  const std::string& test_ir_file() const {
    return ::testing::get<0>(GetParam());
  }
  const std::string& control_name() const {
    return ::testing::get<1>(GetParam());
  }
  int expect_transform_count() const {
    return ::testing::get<2>(GetParam());
  }

  // HitAssignMapper instance for test use; created by SetUpTestIR.
  std::unique_ptr<HitAssignMapper> test_inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// This parameterized test covers all the non-error cases with and without
// transforms.
TEST_P(HitAssignMapperTest, TestApplyNoErrors) {
  SetUpTestIR(test_ir_file());
  const IR::P4Control* ir_control = ir_helper_->GetP4Control(control_name());
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* new_control = test_inspector_->Apply(*ir_control);
  const bool expect_transform = (expect_transform_count() != 0);
  EXPECT_EQ(expect_transform, ir_control != new_control);
  EXPECT_EQ(0, ::errorCount());
  StatementCounter hit_counter;
  hit_counter.CountStatements(*new_control);
  EXPECT_EQ(expect_transform_count(), hit_counter.hit_statement_count());
  forAllMatching<IR::TableHitStatement>(new_control->body,
                                        [&](const IR::TableHitStatement* hit) {
    EXPECT_FALSE(hit->hit_var_name.isNullOrEmpty());
    EXPECT_FALSE(hit->table_name.isNullOrEmpty());
    ASSERT_NE(nullptr, hit->p4_table);
    EXPECT_EQ(hit->table_name, hit->p4_table->externalName());
  });
}

// Tests a table.apply().hit expression in an unexpected place.
TEST_F(HitAssignMapperTest, TestApplyUnexpectedHit) {
  const std::string kTestP4File = "hit_assign.ir.json";
  SetUpTestIR(kTestP4File);
  const IR::P4Control* ir_control = ir_helper_->GetP4Control("basic_hit");
  ASSERT_TRUE(ir_control != nullptr);

  // The first statement in ir_control should be an assignment, and the
  // second should be an IfStatement based on the frontend's normal transform
  // to temporary hit variables.  The code below reassembles parts of these
  // statements into a new P4Control with the logic in its original form, i.e.
  // with the hit embedded in "if (!test_table.apply().hit)".
  auto assignment =
      ir_control->body->components[0]->to<IR::AssignmentStatement>();
  ASSERT_TRUE(assignment != nullptr);
  auto if_statement =
      ir_control->body->components[1]->to<IR::IfStatement>();
  ASSERT_TRUE(if_statement != nullptr);
  auto hit_condition = assignment->right;
  auto new_if = absl::make_unique<IR::IfStatement>(
      hit_condition, if_statement->ifTrue, if_statement->ifFalse);
  auto new_body = absl::make_unique<IR::BlockStatement>();
  new_body->push_back(new_if.get());
  auto test_control = absl::make_unique<IR::P4Control>(
      ir_control->name, ir_control->type, ir_control->constructorParams,
      ir_control->controlLocals, new_body.get());

  const IR::P4Control* transformed = test_inspector_->Apply(*test_control);
  EXPECT_EQ(test_control.get(), transformed);
  EXPECT_EQ(1, ::errorCount());
}

// Tests assignment of table hit status to an unexpected value type.
TEST_F(HitAssignMapperTest, TestApplyUnknownHitVarType) {
  const std::string kTestP4File = "hit_assign.ir.json";
  SetUpTestIR(kTestP4File);
  const IR::P4Control* ir_control = ir_helper_->GetP4Control("basic_hit");
  ASSERT_TRUE(ir_control != nullptr);

  // The first statement in ir_control should be the temporary variable
  // assignment with the table hit status.  The code below replaces the
  // assigned temporary variable with a dummy BoolLiteral.  This would not
  // be valid P4 syntax, but it is a simple way to produce an IR to prove
  // that HitAssignMapper can handle something unexpected in the assignment's
  // left-hand side.
  auto assignment =
      ir_control->body->components[0]->to<IR::AssignmentStatement>();
  ASSERT_TRUE(assignment != nullptr);
  auto new_left = absl::make_unique<IR::BoolLiteral>(true);
  auto new_assignment = absl::make_unique<IR::AssignmentStatement>(
      new_left.get(), assignment->right);
  auto new_body = absl::make_unique<IR::BlockStatement>();
  new_body->push_back(new_assignment.get());
  auto test_control = absl::make_unique<IR::P4Control>(
      ir_control->name, ir_control->type, ir_control->constructorParams,
      ir_control->controlLocals, new_body.get());

  const IR::P4Control* transformed = test_inspector_->Apply(*test_control);
  EXPECT_EQ(test_control.get(), transformed);
  EXPECT_EQ(1, ::errorCount());
}

// Verifies HitAssignMapper::RunPreTestTransform produces transformed output.
TEST_F(HitAssignMapperTest, TestRunPreTestTransform) {
  SetUpTestIR("hit_assign.ir.json");
  const IR::P4Control* test_control = ir_helper_->GetP4Control("basic_hit");
  ASSERT_TRUE(test_control != nullptr);
  auto transformed_control = HitAssignMapper::RunPreTestTransform(
      *test_control, ir_helper_->mid_end_refmap(),
      ir_helper_->mid_end_typemap());
  EXPECT_NE(test_control, transformed_control);
  EXPECT_NE(nullptr, transformed_control);
  EXPECT_EQ(0, ::errorCount());
}

// Test parameters:
//  1) Name of JSON file with test P4 IR.
//  2) Name of control in the IR to be tested.
//  3) Count of expected number of TableHitStatement transforms.
// HitAssignMapperTest borrows most .p4 source files from other unit tests.
// It gets full coverage of non-error cases without the need for several
// additional test files specific to these tests.
INSTANTIATE_TEST_SUITE_P(
    ApplyNoErrorTests,
    HitAssignMapperTest,
    Values(
        std::make_tuple("control_apply_hit_miss_test.ir.json", "egress", 1),
        std::make_tuple("control_apply_hit_miss_test.ir.json", "ingress", 1),
        std::make_tuple("control_if_test.ir.json", "computeChecksum", 0),
        std::make_tuple("control_if_test.ir.json", "egress", 0),
        std::make_tuple("control_if_test.ir.json", "ingress", 1),
        std::make_tuple("control_misc_test.ir.json", "computeChecksum", 0),
        std::make_tuple("control_misc_test.ir.json", "egress", 0),
        std::make_tuple("control_misc_test.ir.json", "ingress", 0),
        std::make_tuple("control_misc_test.ir.json", "verifyChecksum", 0),
        std::make_tuple("hidden_table1.ir.json", "ingress", 2),
        std::make_tuple("hit_assign.ir.json", "basic_hit", 1),
        std::make_tuple("if_color_test.ir.json", "ifs_with_no_transforms", 2),
        std::make_tuple("if_color_test.ir.json", "ifs_with_transforms", 0),
        std::make_tuple("switch_case.ir.json", "inverted_conditions", 0),
        std::make_tuple("switch_case.ir.json", "normal_clone_drop", 0)));

}  // namespace p4c_backends
}  // namespace stratum
