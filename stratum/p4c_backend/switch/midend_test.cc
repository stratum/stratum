// Contains unit tests for the Hercules-specific p4c MidEnd.

#include "platforms/networking/hercules/p4c_backend/switch/midend.h"

#include <memory>

#include "platforms/networking/hercules/p4c_backend/common/program_inspector.h"
#include "platforms/networking/hercules/p4c_backend/test/ir_test_helpers.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"
#include "p4lang_p4c/frontends/common/options.h"
#include "p4lang_p4c/ir/ir.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  Since IRTestHelperJson runs the same midend during
// IR generation, in reality these tests are looking at the output of two
// MidEnd passes.  If the IRTestHelperJson midend changes for some reason, it
// may adversely affect these tests.
class MidEndTest : public testing::Test {
 public:
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4IRFile = "platforms/networking/hercules/"
        "p4c_backend/switch/testdata/no_table_tmp.ir.json";
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestP4IRFile));
  }

 protected:
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  std::unique_ptr<MidEndInterface> mid_end_;     // Tested MidEnd.
  CompilerOptions dummy_p4c_options_;            // Dummy options for test use.
};

// Tests a basic midend pass.
TEST_F(MidEndTest, TestRun) {
  mid_end_ = MidEnd::CreateInstance(&dummy_p4c_options_);
  auto p4_program = ir_helper_->ir_top_level()->getProgram();
  IR::ToplevelBlock* top_level = mid_end_->RunMidEndPass(*p4_program);
  EXPECT_NE(nullptr, top_level);
  EXPECT_EQ(top_level, mid_end_->top_level());

  // These expectations do simple operations to make sure the reference and
  // type maps are sane.
  ASSERT_NE(nullptr, mid_end_->reference_map());
  mid_end_->reference_map()->setIsV1(true);
  EXPECT_TRUE(mid_end_->reference_map()->isV1());
  mid_end_->reference_map()->setIsV1(false);
  EXPECT_FALSE(mid_end_->reference_map()->isV1());
  ASSERT_NE(nullptr, mid_end_->type_map());
  EXPECT_LT(0, mid_end_->type_map()->size());
}

// Tests failure upon multiple midend pass runs.
TEST_F(MidEndTest, TestRunTwice) {
  mid_end_ = MidEnd::CreateInstance(&dummy_p4c_options_);
  auto p4_program = ir_helper_->ir_top_level()->getProgram();
  EXPECT_NE(nullptr, mid_end_->RunMidEndPass(*p4_program));
  EXPECT_EQ(nullptr, mid_end_->RunMidEndPass(*p4_program));
}

// Verifies that no temporary actions or tables appear in the midend output.
TEST_F(MidEndTest, TestNoActionTableTmp) {
  mid_end_ = MidEnd::CreateInstance(&dummy_p4c_options_);
  auto p4_program = ir_helper_->ir_top_level()->getProgram();
  IR::ToplevelBlock* top_level = mid_end_->RunMidEndPass(*p4_program);
  ASSERT_NE(nullptr, top_level);

  // This test traverses all the actions and tables in the program to
  // make sure there are no temporaries.  The tested P4 program should have
  // exactly one table and action.
  ProgramInspector prog_inspector;
  top_level->getProgram()->apply(prog_inspector);
  EXPECT_EQ(1,  prog_inspector.actions().size());
  for (const auto& action_iter : prog_inspector.actions()) {
    const std::string name = std::string(action_iter.first->name.toString());
    EXPECT_EQ(std::string::npos, name.find("tmp"));
  }
  EXPECT_EQ(1,  prog_inspector.tables().size());
  for (const auto& table_iter : prog_inspector.tables()) {
    const std::string name = std::string(table_iter->name.toString());
    EXPECT_EQ(std::string::npos, name.find("tmp"));
  }
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
