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

// Contains unit tests for MidEndP4cOpen.

#include "stratum/p4c_backends/common/midend_p4c_open.h"

#include <string>

#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"
#include "gtest/gtest.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class MidEndP4cOpenTest : public testing::Test {
 public:
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4IRFile =
        "stratum/"
        "p4c_backends/test/testdata/simple_vlan_stack_16.ir.json";
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestP4IRFile));
  }

 protected:
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  CompilerOptions dummy_p4c_options_;            // Dummy options for test use.
};

// Tests a basic midend pass.
TEST_F(MidEndP4cOpenTest, TestRun) {
  MidEndP4cOpen mid_end(&dummy_p4c_options_);
  auto p4_program = ir_helper_->ir_top_level()->getProgram();
  IR::ToplevelBlock* top_level = mid_end.RunMidEndPass(*p4_program);
  EXPECT_NE(nullptr, top_level);
  EXPECT_EQ(top_level, mid_end.top_level());
  EXPECT_NE(nullptr, mid_end.reference_map());
  EXPECT_NE(nullptr, mid_end.type_map());
}

// Tests failure upon multiple midend pass runs.
TEST_F(MidEndP4cOpenTest, TestRunTwice) {
  MidEndP4cOpen mid_end(&dummy_p4c_options_);
  auto p4_program = ir_helper_->ir_top_level()->getProgram();
  EXPECT_NE(nullptr, mid_end.RunMidEndPass(*p4_program));
  EXPECT_EQ(nullptr, mid_end.RunMidEndPass(*p4_program));
}

}  // namespace p4c_backends
}  // namespace stratum
