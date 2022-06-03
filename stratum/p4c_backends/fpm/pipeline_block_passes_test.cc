// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains unit tests for the pipeline block optimization classes.

#include "stratum/p4c_backends/fpm/pipeline_block_passes.h"

#include <set>
#include <string>
#include <memory>

#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_inspectors.h"
#include "stratum/p4c_backends/test/test_target_info.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/visitor.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  The test parameter allows testing with and without
// prior transform passes since the pipeline passes are designed to work
// either way.
class PipelinePassesTest : public testing::TestWithParam<bool> {
 public:
  static void SetUpTestCase() {
    TestTargetInfo::SetUpTestTargetInfo();
    SetUpTestP4ModelNames();
  }
  static void TearDownTestCase() {
    TestTargetInfo::TearDownTestTargetInfo();
  }

 protected:
  // The SetUpTestIR method loads an IR file in JSON format, then applies a
  // ProgramInspector to record IR nodes that contain some P4Control methods
  // to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    ir_helper_->set_color_field_name("mmeta.enum_color");
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Gets P4Control with the given name, running preliminary transform
  // passes if the test parameter is true.
  const IR::P4Control* SetUpTestP4Control(const std::string& control_name) {
    if (GetParam()) {
      std::vector<IRTestHelperJson::IRControlTransforms> transform_list(
          {IRTestHelperJson::kHitAssignMapper,
           IRTestHelperJson::kMeterColorMapper});
      return ir_helper_->TransformP4Control(control_name, transform_list);
    }
    return ir_helper_->GetP4Control(control_name);
  }

  // Runs a PipelineIfBlockInsertPass on the input control to set up testing
  // of subsequent transform passes.
  const IR::P4Control* RunBlockInsertPass(const IR::P4Control& control) {
    PipelineIfBlockInsertPass block_insert_pass;
    return block_insert_pass.InsertBlocks(control);
  }

  // Runs a PipelineBlockPass on the input control to set up testing
  // of subsequent transform passes; expects SetUpTestIR to run first.
  const IR::P4Control* RunPipelineBlockPass(const IR::P4Control& control) {
    PipelineBlockPass block_pass(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
    return block_pass.OptimizeControl(control);
  }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// The "egress" control in the test file does not have any tables that can
// be optimized into fixed function pipeline stages.
TEST_P(PipelinePassesTest, TestInspectNoFixedTables) {
  SetUpTestIR("pipeline_opt_inspect.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("egress");
  ASSERT_TRUE(ir_control != nullptr);
  FixedTableInspector test_inspector;
  EXPECT_FALSE(test_inspector.FindFixedTables(*ir_control));
}

// The "ingress" control in the test file has one table that can
// be optimized into a fixed function pipeline stage.
TEST_P(PipelinePassesTest, TestInspectFixedTables) {
  SetUpTestIR("pipeline_opt_inspect.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  FixedTableInspector test_inspector;
  EXPECT_TRUE(test_inspector.FindFixedTables(*ir_control));
}

// Tests whether PipelineIfBlockInsertPass inserts BlockStatements.  The
// "ingress" control has two standalone apply statements.
TEST_P(PipelinePassesTest, TestBlockInsert) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  StatementCounter statement_counter_before;
  statement_counter_before.CountStatements(*ir_control);
  int blocks_before = statement_counter_before.block_statement_count();

  PipelineIfBlockInsertPass block_insert_pass;
  const IR::P4Control* new_control =
      block_insert_pass.InsertBlocks(*ir_control);
  EXPECT_NE(ir_control, new_control);
  StatementCounter statement_counter_after;
  statement_counter_after.CountStatements(*new_control);
  EXPECT_EQ(blocks_before + 2, statement_counter_after.block_statement_count());
}

// Tests PipelineIfBlockInsertPass on a P4Control with no if-else clauses that
// need block replacement.
TEST_P(PipelinePassesTest, TestNoBlockInsert) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("egress_no_block");
  ASSERT_TRUE(ir_control != nullptr);
  PipelineIfBlockInsertPass block_insert_pass;
  const IR::P4Control* new_control =
      block_insert_pass.InsertBlocks(*ir_control);
  EXPECT_EQ(ir_control, new_control);  // Expects ir_control to be unchanged.
}

// Tests PipelineBlockPass transformation of BlockStatements into
// PipelineStageStatements.
TEST_P(PipelinePassesTest, DISABLED_TestBlockOptimization) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);

  // This preliminary PipelineIfBlockInsertPass surrounds any apply of the form
  // if (...) apply() else apply() into a BlockStatement.
  const IR::P4Control* ir_control_with_blocks = RunBlockInsertPass(*ir_control);

  // The tested PipelineBlockPass forms PipelineStageStatements from
  // BlockStatements wherever an entire block refers to a fixed pipeline stage.
  PipelineBlockPass block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* block_pass_control =
      block_pass.OptimizeControl(*ir_control_with_blocks);
  EXPECT_NE(ir_control_with_blocks, block_pass_control);

  // The PipelineBlockPass should introduce two PipelineStageStatements.  It
  // should leave the IfStatement at the control's top level alone.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*block_pass_control);
  EXPECT_EQ(2, statement_counter.pipeline_statement_count());
  EXPECT_EQ(1, statement_counter.if_statement_count());

  // The PipelineBlockPass should optimize the LPM tables (lpm_N) and leave the
  // ACL VLAN stage alone.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*block_pass_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_v"));
  EXPECT_FALSE(table_inspector.IsOptimized("ingress.acl_v"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_1"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_2"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_2"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_3"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_3"));
}

// Tests PipelineIfElsePass transformation of IfStatements into
// PipelineStageStatements.
TEST_P(PipelinePassesTest, DISABLED_TestIfElseOptimization) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);

  // Two preliminary passes prepare the tested control for
  // the PipelineIfElsePass.  After these two passes, the tested control
  // should have one IfStatement with two PipelineStageStatements remaining.
  const IR::P4Control* pass1_control = RunBlockInsertPass(*ir_control);
  const IR::P4Control* pass2_control = RunPipelineBlockPass(*pass1_control);
  StatementCounter pass2_counter;
  pass2_counter.CountStatements(*pass2_control);
  ASSERT_EQ(1, pass2_counter.if_statement_count());
  ASSERT_EQ(2, pass2_counter.pipeline_statement_count());

  // The tested PipelineIfElsePass forms a PipelineStageStatement from the
  // original IfStatement.
  PipelineIfElsePass ifelse_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      ifelse_pass.OptimizeControl(*pass2_control);
  EXPECT_NE(pass2_control, final_control);
  StatementCounter final_counter;
  final_counter.CountStatements(*final_control);
  EXPECT_EQ(0, final_counter.if_statement_count());
  EXPECT_EQ(1, final_counter.pipeline_statement_count());

  // The PipelineIfElsePass should optimize the LPM tables (lpm_N) and leave the
  // ACL VLAN stage alone.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_v"));
  EXPECT_FALSE(table_inspector.IsOptimized("ingress.acl_v"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_1"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_2"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_2"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.lpm_3"));
  EXPECT_FALSE(table_inspector.IsUnoptimized("ingress.lpm_3"));
}

// Tests PipelineIfElsePass doesn't transform IfStatement with no applies.
TEST_P(PipelinePassesTest, TestIfElseNoOptimization) {
  SetUpTestIR("pipeline_opt_block.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("egress_no_block");
  ASSERT_TRUE(ir_control != nullptr);

  // The two preliminary passes need to run first.
  const IR::P4Control* pass1_control = RunBlockInsertPass(*ir_control);
  ASSERT_EQ(ir_control, pass1_control);
  const IR::P4Control* pass2_control = RunPipelineBlockPass(*pass1_control);
  ASSERT_EQ(pass1_control, pass2_control);

  // The tested PipelineIfElsePass should not transform the control.
  PipelineIfElsePass ifelse_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      ifelse_pass.OptimizeControl(*pass2_control);
  EXPECT_EQ(pass2_control, final_control);
}

// TODO(unknown): Add more PipelineIfElsePass tests to:
// - Test true-block applies LPM stage but false-block is empty.
// - Test true-block applies LPM stage but false-block is non-apply statement.
// - Test true-block applies LPM stage but false-block applies ACL stage.
// - Test if { apply LPM1 } else if { apply LPM2 } else { apply LPM3 }
// - Test as above, replacing LPM3 with ACL table.

INSTANTIATE_TEST_SUITE_P(
  WithAndWithoutTransforms,
  PipelinePassesTest,
  ::testing::Bool()
);

}  // namespace p4c_backends
}  // namespace stratum
