// This file contains unit tests for the test Inspector classes.

#include "platforms/networking/hercules/p4c_backend/test/test_inspectors.h"

#include <string>
#include <vector>

#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "platforms/networking/hercules/p4c_backend/test/ir_test_helpers.h"
#include "platforms/networking/hercules/public/proto/p4_annotation.host.pb.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/ir/visitor.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class TestInspectorTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  // The SetUpTestIR method loads an IR file in JSON format, then applies a
  // ProgramInspector to record IR nodes that contain some P4Control methods
  // to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "platforms/networking/hercules/p4c_backend/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // These methods set up P4Control instances for testing.  SetUpControl
  // takes the control directly from the ir_helper_.  SetUpOptimizedControl
  // wraps the ir_helper_ control body in an IR::PipelineStageStatement.
  // Both methods assume that SetUpTestIR has run successfully.  The parameter
  // indicates whether to run a HitAssignMapper transform on the control.
  void SetUpControl(bool do_hit_transform) {
    std::vector<IRTestHelperJson::IRControlTransforms> transform_list;
    if (do_hit_transform)
      transform_list.push_back(IRTestHelperJson::kHitAssignMapper);
    const IR::P4Control* ir_control =
        ir_helper_->TransformP4Control("ingress", transform_list);
    ASSERT_TRUE(ir_control != nullptr);
    original_control_.reset(ir_control);
  }

  void SetUpOptimizedControl(bool do_hit_transform) {
    SetUpControl(do_hit_transform);
    optimized_block_ = absl::make_unique<IR::PipelineStageStatement>(
        original_control_->body->annotations,
        original_control_->body->components, P4Annotation::L3_LPM);
    optimized_control_ = absl::make_unique<IR::P4Control>(
        original_control_->srcInfo, original_control_->name,
        original_control_->type, original_control_->constructorParams,
        original_control_->controlLocals, optimized_block_.get());
  }

  // The p4c visitor.h file provides an Inspector-like template to
  // iterate over specific objects in an IR::Node.  It is used here to
  // record all the tables within the input control.
  void GetAllP4Tables(const IR::P4Control& control) {
    tables_in_control_.clear();
    forAllMatching<IR::P4Table>(&control.controlLocals,
                                [&](const IR::P4Table* table) {
      tables_in_control_.push_back(std::string(table->externalName()));
    });
  }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.

  // These pointers are initialized by SetUpControl and SetUpOptimizedControl.
  std::unique_ptr<const IR::PipelineStageStatement> optimized_block_;
  std::unique_ptr<const IR::P4Control> original_control_;
  std::unique_ptr<const IR::P4Control> optimized_control_;

  // Populated by GetAllP4Tables.
  std::vector<std::string> tables_in_control_;
};

TEST_F(TestInspectorTest, TestStatementCounterNoPipelineOptimization) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpControl(false);
  StatementCounter statement_counter;
  statement_counter.CountStatements(*original_control_);

  // No control optimization has been done, so there are no pipeline statements.
  EXPECT_EQ(0, statement_counter.pipeline_statement_count());
  EXPECT_NE(0, statement_counter.if_statement_count());
  EXPECT_NE(0, statement_counter.block_statement_count());
  EXPECT_EQ(0, statement_counter.hit_statement_count());
}

TEST_F(TestInspectorTest, TestStatementCounterWithPipelineOptimization) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpOptimizedControl(false);
  StatementCounter statement_counter;
  statement_counter.CountStatements(*optimized_control_);

  // The control is completely optimized into a pipeline statement.
  EXPECT_EQ(1, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(0, statement_counter.block_statement_count());
  EXPECT_EQ(0, statement_counter.hit_statement_count());
}

TEST_F(TestInspectorTest, TestStatementCounterHitTransformNoPipelineOpt) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpControl(true);
  StatementCounter statement_counter;
  statement_counter.CountStatements(*original_control_);

  // No control optimization has been done, so there are no pipeline statements.
  EXPECT_EQ(0, statement_counter.pipeline_statement_count());
  EXPECT_NE(0, statement_counter.if_statement_count());
  EXPECT_NE(0, statement_counter.block_statement_count());
  EXPECT_EQ(2, statement_counter.hit_statement_count());
}

TEST_F(TestInspectorTest, TestStatementCounterHitTransformAndPipelineOpt) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpOptimizedControl(true);
  StatementCounter statement_counter;
  statement_counter.CountStatements(*optimized_control_);

  // The control is completely optimized into a pipeline statement, including
  // the TableHitStatements.
  EXPECT_EQ(1, statement_counter.pipeline_statement_count());
  EXPECT_EQ(0, statement_counter.if_statement_count());
  EXPECT_EQ(0, statement_counter.block_statement_count());
  EXPECT_EQ(0, statement_counter.hit_statement_count());
}

TEST_F(TestInspectorTest, TestOptimizedTableInspectorNoPipelineOptimization) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpControl(false);
  GetAllP4Tables(*original_control_);
  OptimizedTableInspector inspector;
  inspector.InspectTables(*original_control_);

  // No control optimization has been done, so all tables are unoptimized.
  for (const auto& table_name : tables_in_control_) {
    EXPECT_FALSE(inspector.IsOptimized(table_name));
    EXPECT_TRUE(inspector.IsUnoptimized(table_name));
  }
}

TEST_F(TestInspectorTest, TestOptimizedTableInspectorWithPipelineOptimization) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpOptimizedControl(false);
  GetAllP4Tables(*optimized_control_);
  OptimizedTableInspector inspector;
  inspector.InspectTables(*optimized_control_);

  // The control is completely optimized, so all tables are optimized.
  for (const auto& table_name : tables_in_control_) {
    EXPECT_TRUE(inspector.IsOptimized(table_name));
    EXPECT_FALSE(inspector.IsUnoptimized(table_name));
  }
}

TEST_F(TestInspectorTest,
       TestOptimizedTableInspectorWithHitTransformAndPipelineOptimization) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpOptimizedControl(true);
  GetAllP4Tables(*optimized_control_);
  OptimizedTableInspector inspector;
  inspector.InspectTables(*optimized_control_);

  // The control is completely optimized, so all tables are optimized.
  for (const auto& table_name : tables_in_control_) {
    EXPECT_TRUE(inspector.IsOptimized(table_name));
    EXPECT_FALSE(inspector.IsUnoptimized(table_name));
  }
}

TEST_F(TestInspectorTest, TestOptimizedTableInspectorUnknownTable) {
  SetUpTestIR("switch/testdata/pipeline_opt_block.ir.json");
  SetUpControl(false);
  OptimizedTableInspector inspector;
  inspector.InspectTables(*original_control_);
  EXPECT_FALSE(inspector.IsOptimized("unknown-table"));
  EXPECT_FALSE(inspector.IsUnoptimized("unknown-table"));
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
