// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for MidEndP4cOpen.

#include <string>

#include "stratum/p4c_backends/common/midend_p4c_open.h"

#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class MidEndP4cOpenTest : public testing::Test {
 public:
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4IRFile = "stratum/"
        "p4c_backends/test/testdata/simple_vlan_stack_16.ir.json";
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestP4IRFile));
  }

 protected:
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  CompilerOptions dummy_p4c_options_;  // Dummy options for test use.
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
