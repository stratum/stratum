// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// Contains unit tests for ProgramInspector.

#include "stratum/p4c_backends/common/program_inspector.h"

#include <memory>

#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class ProgramInspectorTest : public testing::Test {
 public:
  void SetUp() override {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4IRFile =
              "stratum/p4c_backends/fpm/testdata/field_inspect_test.ir.json";
    ASSERT_TRUE(ir_helper_->GenerateTestIR(kTestP4IRFile));
  }

 protected:
  ProgramInspector inspector_;        // The tested ProgramInspector.
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

TEST_F(ProgramInspectorTest, TestAction) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.actions().size());
}

TEST_F(ProgramInspectorTest, TestStructLike) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.struct_likes().size());
}

TEST_F(ProgramInspectorTest, TestHeaderTypes) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.header_types().size());
}

TEST_F(ProgramInspectorTest, TestTypedefs) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.p4_typedefs().size());
}

TEST_F(ProgramInspectorTest, TestEnums) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.p4_enums().size());
}

TEST_F(ProgramInspectorTest, TestPathStructs) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.struct_paths().size());
}

TEST_F(ProgramInspectorTest, TestMatchKeys) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.match_keys().size());
}

TEST_F(ProgramInspectorTest, TestTables) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.tables().size());
}

TEST_F(ProgramInspectorTest, TestParsers) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.parsers().size());
}

TEST_F(ProgramInspectorTest, TestControls) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.controls().size());
}

TEST_F(ProgramInspectorTest, TestAssignments) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.assignments().size());
}

TEST_F(ProgramInspectorTest, TestActionAssignments) {
  ir_helper_->ir_top_level()->getProgram()->apply(inspector_);
  EXPECT_NE(0, inspector_.action_assignments().size());
  EXPECT_GE(inspector_.assignments().size(),
            inspector_.action_assignments().size());
}

}  // namespace p4c_backends
}  // namespace stratum
