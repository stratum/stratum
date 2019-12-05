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

// Contains unit tests for the Stratum-specific p4c MidEnd.

#include "stratum/p4c_backends/fpm/midend.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "gtest/gtest.h"
#include "stratum/p4c_backends/common/program_inspector.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  Since IRTestHelperJson runs the same midend during
// IR generation, in reality these tests are looking at the output of two
// MidEnd passes.  If the IRTestHelperJson midend changes for some reason, it
// may adversely affect these tests.
class MidEndTest : public testing::Test {
 public:
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4IRFile =
        "stratum/"
        "p4c_backends/fpm/testdata/no_table_tmp.ir.json";
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
  EXPECT_EQ(1, prog_inspector.actions().size());
  for (const auto& action_iter : prog_inspector.actions()) {
    const std::string name = std::string(action_iter.first->name.toString());
    EXPECT_EQ(std::string::npos, name.find("tmp"));
  }
  EXPECT_EQ(1, prog_inspector.tables().size());
  for (const auto& table_iter : prog_inspector.tables()) {
    const std::string name = std::string(table_iter->name.toString());
    EXPECT_EQ(std::string::npos, name.find("tmp"));
  }
}

}  // namespace p4c_backends
}  // namespace stratum
