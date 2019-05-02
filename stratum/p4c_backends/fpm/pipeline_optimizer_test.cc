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

// This file contains unit tests for the PipelineOptimizer class.

#include "stratum/p4c_backends/fpm/pipeline_optimizer.h"

#include <memory>

#include "base/commandlineflags.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/p4c_backends/test/test_target_info.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

DECLARE_bool(enable_pipeline_optimization);

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  The individual optimization passes have their own unit
// tests, so these tests focus on the overall decision to optimize or not.
class PipelineOptimizerTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    TestTargetInfo::SetUpTestTargetInfo();
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
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));

    // The reference and type maps from the IR are needed to construct a
    // PipelineOptimizer for tests.
    test_optimizer_ = absl::make_unique<PipelineOptimizer>(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  }

  // The test_optimizer_ is for common test use.
  std::unique_ptr<PipelineOptimizer> test_optimizer_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

// The "egress" control in the test file does not have any tables that can
// be optimized into fixed function pipeline stages.
TEST_F(PipelineOptimizerTest, TestNoOptimize) {
  FLAGS_enable_pipeline_optimization = true;
  SetUpTestIR("pipeline_opt_inspect.ir.json");
  const IR::P4Control* ir_control = ir_helper_->GetP4Control("egress");
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_EQ(ir_control, test_optimizer_->Optimize(*ir_control));
}

// The "ingress" control in the test file has one table that can
// be optimized into a fixed function pipeline stage.
TEST_F(PipelineOptimizerTest, TestOptimize) {
  FLAGS_enable_pipeline_optimization = true;
  SetUpTestIR("pipeline_opt_inspect.ir.json");
  const IR::P4Control* ir_control = ir_helper_->GetP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_NE(ir_control, test_optimizer_->Optimize(*ir_control));
}

// The "ingress" control should not be optimized when the command-line
// flag is disabled.
TEST_F(PipelineOptimizerTest, TestDisableOptimization) {
  FLAGS_enable_pipeline_optimization = false;
  SetUpTestIR("pipeline_opt_inspect.ir.json");
  const IR::P4Control* ir_control = ir_helper_->GetP4Control("ingress");
  ASSERT_TRUE(ir_control != nullptr);
  EXPECT_EQ(ir_control, test_optimizer_->Optimize(*ir_control));
}

}  // namespace p4c_backends
}  // namespace stratum
