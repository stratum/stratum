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

// This file contains unit tests of the pipeline intra-block optimizing classes.

#include "stratum/p4c_backends/fpm/pipeline_intra_block_passes.h"

#include "stratum/p4c_backends/fpm/pipeline_block_passes.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_inspectors.h"
#include "stratum/p4c_backends/test/test_target_info.h"
#include "stratum/public/proto/p4_annotation.pb.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/visitor.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  The test parameter allows testing with and without
// prior transform passes since the pipeline passes are designed to work
// either way.
class PipelineIntraBlockPassesTest : public testing::TestWithParam<bool> {
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
    ir_helper_->set_color_field_name("meta.enum_color");
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Gets P4Control with the given name, running preliminary transform
  // passes if the test parameter is true.  The MeterColorMapper transform
  // is not used here because it doesn't like some of the input controls
  // for these tests.
  const IR::P4Control* SetUpTestP4Control(const std::string& control_name) {
    if (GetParam()) {
      std::vector<IRTestHelperJson::IRControlTransforms> transform_list(
          {IRTestHelperJson::kHitAssignMapper});
      return ir_helper_->TransformP4Control(control_name, transform_list);
    }
    return ir_helper_->GetP4Control(control_name);
  }

  // Runs all preliminary block optimization passes to set up intra-block tests.
  const IR::P4Control* RunBlockPasses(const IR::P4Control& control) {
    PipelineIfBlockInsertPass block_insert_pass;
    const IR::P4Control* control1 = block_insert_pass.InsertBlocks(control);
    PipelineBlockPass block_pass(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
    const IR::P4Control* control2 = block_pass.OptimizeControl(*control1);
    PipelineIfElsePass ifelse_pass(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
    return ifelse_pass.OptimizeControl(*control2);
  }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// Tests the common ACL table followed by multiple L2 table apply sequence
// found in Stratum P4 programs:
//    acl_table.apply();
//    if (!mac_table_1.apply().hit) {
//      mac_table_2.apply();
//    }
TEST_P(PipelineIntraBlockPassesTest, TestACLMultiL2Sequence) {
  SetUpTestIR("pipeline_intra_block1.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(0, ::errorCount());

  // In the optimized final control, the L2 tables should be combined into
  // a PipelineStageStatement, which also consumes the IfStatement.  The
  // top-level block statement remains.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*final_control);
  EXPECT_EQ(1, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(1, statement_counter.block_statement_count());

  // Both L2 applies should now be in the optimized stage.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_table"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_2"));
}

// Tests a basic block sequence involving multiple apply stages:
//    acl_table.apply();
//    mac_table_1.apply();
//    mac_table_2.apply();
//    l3_table_1.apply();
TEST_P(PipelineIntraBlockPassesTest, TestACLStageL2StageL3StageSequence) {
  SetUpTestIR("pipeline_intra_block2.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(0, ::errorCount());

  // In the optimized final control, the L2 tables and L3 table should be
  // combined into separate PipelineStageStatements.  The top-level block
  // statement remains.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*final_control);
  EXPECT_EQ(2, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(1, statement_counter.block_statement_count());

  // The L2 and L3 tables should now be in the optimized stages.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_table"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_2"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.l3_table_1"));
}

// Verifies AssignmentStatements interleaved between table applies do
// not affect stage consolidation:
//    acl_table.apply();
//    meta.color = 1;
//    mac_table_1.apply();
//    meta.enum_color = meter_color_t.COLOR_YELLOW;
//    mac_table_2.apply();
TEST_P(PipelineIntraBlockPassesTest, TestInterleavedAssignments) {
  SetUpTestIR("pipeline_intra_block3.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(0, ::errorCount());

  // In the optimized final control, the L2 tables should be combined into
  // one PipelineStageStatement.  The top-level block statement remains.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*final_control);
  EXPECT_EQ(1, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(1, statement_counter.block_statement_count());

  // The L2 and tables should now be in the optimized stage.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_table"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_2"));
}

// Verifies that a nested block with no impact on stage assignments does
// not affect stage consolidation:
//    acl_table.apply();
//    mac_table_1.apply();
//    if (hdr.ethernet.isValid()) {
//      meta.color = 1;
//      meta.enum_color = meter_color_t.COLOR_YELLOW;
//    }
//    mac_table_2.apply();
TEST_P(PipelineIntraBlockPassesTest, TestNestedNonStageBlock) {
  SetUpTestIR("pipeline_intra_block4.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(0, ::errorCount());

  // In the optimized final control, the L2 tables should be combined into
  // one PipelineStageStatement.  The top-level block statement remains,
  // but the PipelineStageStatement consumes the block between the L2 tables.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*final_control);
  EXPECT_EQ(1, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(1, statement_counter.block_statement_count());

  // The L2 tables should now be in the optimized stage.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.acl_table"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_2"));
}

// Verifies a more complex statement sequence involving tables in
// nested blocks:
//    vfp_table.apply();
//    bool hit_l2 = mac_table_1.apply().hit || mac_table_2.apply().hit;
//    if (hit_l2) {
//      l3_table_1.apply();
//      l3_table_2.apply();
//      ifp_table.apply();
//    }
TEST_P(PipelineIntraBlockPassesTest, TestNestedBlocks) {
  SetUpTestIR("pipeline_intra_block5.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(0, ::errorCount());

  // In the optimized final control, the L2 tables and L3 tables should be
  // combined into separate PipelineStageStatements.  The top-level block
  // statement and the true-block under the IfStatement remain.
  StatementCounter statement_counter;
  statement_counter.CountStatements(*final_control);
  EXPECT_EQ(2, statement_counter.pipeline_statement_count());
  EXPECT_EQ(1, statement_counter.if_statement_count());
  EXPECT_EQ(2, statement_counter.block_statement_count());

  // The L2 and L3 tables should now be in the optimized stages.  The
  // IFP and VFP ACL tables are not optimized.
  OptimizedTableInspector table_inspector;
  table_inspector.InspectTables(*final_control);
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.vfp_table"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.mac_table_2"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.l3_table_1"));
  EXPECT_TRUE(table_inspector.IsOptimized("ingress.l3_table_2"));
  EXPECT_TRUE(table_inspector.IsUnoptimized("ingress.ifp_table"));
}

// Verifies that PipelineIntraBlockPass doesn't choke on a SwitchStatement.
TEST_P(PipelineIntraBlockPassesTest, TestNoSwitchStatementTransform) {
  SetUpTestIR("switch_case.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("normal_clone_drop");
  ASSERT_TRUE(ir_control != nullptr);
  const IR::P4Control* test_control = RunBlockPasses(*ir_control);

  PipelineIntraBlockPass intra_block_pass(
      ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  const IR::P4Control* final_control =
      intra_block_pass.OptimizeControl(*test_control);
  EXPECT_EQ(test_control, final_control);  // Expects no transformation.
  EXPECT_EQ(0, ::errorCount());
}

// Verifies StatementStageInspector behavior with an unexpected SwitchStatement.
TEST_P(PipelineIntraBlockPassesTest, TestSwitchStatementStageInspect) {
  SetUpTestIR("switch_case.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("normal_clone_drop");
  ASSERT_TRUE(ir_control != nullptr);

  // The switch statement should be the only statement in the control body.
  StatementStageInspector stage_inspector(P4Annotation::L2,
                                          ir_helper_->mid_end_refmap(),
                                          ir_helper_->mid_end_typemap());
  P4Annotation::PipelineStage stage = stage_inspector.Inspect(
      *ir_control->body->components[0]);
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, stage);
  EXPECT_NE(0, ::errorCount());
}

// Verifies StatementStageInspector behavior with an unexpected BlockStatement.
TEST_P(PipelineIntraBlockPassesTest, TestBlockStatementStageInspect) {
  SetUpTestIR("pipeline_intra_block1.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);

  // The control body is a BlockStatement.
  StatementStageInspector stage_inspector(P4Annotation::L2,
                                          ir_helper_->mid_end_refmap(),
                                          ir_helper_->mid_end_typemap());
  P4Annotation::PipelineStage stage = stage_inspector.Inspect(
      *ir_control->body);
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, stage);
  EXPECT_NE(0, ::errorCount());
}

// Verifies StatementStageInspector behavior with an unexpected IfStatement.
TEST_P(PipelineIntraBlockPassesTest, TestIfStatementStageInspect) {
  SetUpTestIR("pipeline_intra_block1.ir.json");
  const IR::P4Control* ir_control = SetUpTestP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);

  // The if statement should be the 3rd statement in the control body.
  StatementStageInspector stage_inspector(P4Annotation::L2,
                                          ir_helper_->mid_end_refmap(),
                                          ir_helper_->mid_end_typemap());
  P4Annotation::PipelineStage stage = stage_inspector.Inspect(
      *ir_control->body->components[2]);
  EXPECT_EQ(P4Annotation::DEFAULT_STAGE, stage);
  EXPECT_NE(0, ::errorCount());
}

INSTANTIATE_TEST_CASE_P(
  WithAndWithoutTransforms,
  PipelineIntraBlockPassesTest,
  ::testing::Bool()
);

}  // namespace p4c_backends
}  // namespace stratum
