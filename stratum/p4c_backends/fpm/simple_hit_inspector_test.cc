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

// Contains unit tests for SimpleHitInspector.

#include "stratum/p4c_backends/fpm/simple_hit_inspector.h"

#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

using ::testing::Values;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  See INSTANTIATE_TEST_CASE_P near the end if this file
// for the parameter format.
class SimpleHitInspectorTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, int, bool, bool>> {
 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a SimpleHitInspector test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    test_inspector_ = absl::make_unique<SimpleHitInspector>();
  }

  // Returns the IR::Statement from the input control name as selected by
  // statement_index.  If statement_index is negative, the entire control
  // body is returned.  Otherwise, statement_index refers to a specific
  // statement within the control body.  A HitAssignMapper transform is
  // applied for valid returned statements.
  const IR::Statement* FindTestStatement(
      const std::string& control, int statement_index) {
    const IR::P4Control* ir_control = ir_helper_->TransformP4Control(
        control, {IRTestHelperJson::kHitAssignMapper});
    if (ir_control == nullptr) return nullptr;
    if (statement_index >= 0) {
      if (statement_index >= ir_control->body->components.size())
        return nullptr;
      return ir_control->body->components[statement_index]->
          to<IR::Statement>();
    }
    return ir_control->body;
  }

  // Test parameter accessors.
  const std::string& test_ir_file() const {
    return ::testing::get<0>(GetParam());
  }
  const std::string& control_name() const {
    return ::testing::get<1>(GetParam());
  }
  int statement_index() const {
    return ::testing::get<2>(GetParam());
  }
  bool expect_simple() const {
    return ::testing::get<3>(GetParam());
  }
  bool expect_error() const {
    return ::testing::get<4>(GetParam());
  }

  // SimpleHitInspector instance for test use; created by SetUpTestIR.
  std::unique_ptr<SimpleHitInspector> test_inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

TEST_P(SimpleHitInspectorTest, TestInspect) {
  SetUpTestIR(test_ir_file());
  const IR::Statement* test_statement =
      FindTestStatement(control_name(), statement_index());
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_EQ(expect_simple(), test_inspector_->Inspect(*test_statement));
  EXPECT_EQ(expect_error(), ::errorCount() != 0);
}

// Test parameters:
//  1) Name of JSON file with test P4 IR.
//  2) Name of control in the IR to be tested.
//  3) Statement index within control body.
//  4) Flag indicating whether to expect control to have all simple table hits.
//  5) Flag indicating whether to expect errors after Inspect.
INSTANTIATE_TEST_CASE_P(
    VerifyFilesForOtherTestsWithSimpleHits,
    SimpleHitInspectorTest,
    Values(
        std::make_tuple("control_apply_hit_miss_test.ir.json",
                        "egress", -1, true, false),
        std::make_tuple("control_apply_hit_miss_test.ir.json",
                        "ingress", -1, true, false),
        std::make_tuple("control_if_test.ir.json",
                        "computeChecksum", -1, true, false),
        std::make_tuple("control_if_test.ir.json", "egress", -1, true, false),
        std::make_tuple("control_if_test.ir.json", "ingress", -1, true, false),
        std::make_tuple("control_if_test.ir.json",
                        "verifyChecksum", -1, true, false),
        std::make_tuple("control_misc_test.ir.json",
                        "computeChecksum", -1, true, false),
        std::make_tuple("meter_color_nested_ifs.ir.json",
                        "meter_if_in_if", -1, true, false),
        std::make_tuple("no_table_tmp.ir.json", "ingress", -1, true, false),
        std::make_tuple("switch_case.ir.json",
                        "inverted_conditions", -1, true, false),
        std::make_tuple("switch_case.ir.json",
                        "normal_clone_drop", -1, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 0, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_scope_ok", -1, true, false)
    ));

INSTANTIATE_TEST_CASE_P(
    ComplexHitTests,
    SimpleHitInspectorTest,
    Values(
        std::make_tuple("complex_hits.ir.json", "complex_hits", 0, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 1, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 3, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 4, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 5, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 6, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 7, true, false),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 8, false, true)
    ));

}  // namespace p4c_backends
}  // namespace stratum
