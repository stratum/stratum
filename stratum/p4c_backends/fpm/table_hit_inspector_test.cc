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

// Contains unit tests for TableHitInspector.

#include "stratum/p4c_backends/fpm/table_hit_inspector.h"

#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

using ::testing::Values;

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  See INSTANTIATE_TEST_CASE_P near the end if this file
// for the parameter format.
class TableHitInspectorTest
    : public testing::Test,
      public testing::WithParamInterface<
          std::tuple<std::string, std::string, int, bool, bool>> {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a TableHitInspector test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    ir_helper_->set_color_field_name("meta.enum_color");
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    test_inspector_ = absl::make_unique<TableHitInspector>(
        false, false, ir_helper_->mid_end_refmap(),
        ir_helper_->mid_end_typemap());
  }

  // Returns the IR::Statement from the input control name as selected by
  // statement_index.  If statement_index is negative, the entire control
  // body is returned.  Otherwise, statement_index refers to a specific
  // statement within the control body.  The test control is pre-processed
  // with a HitAssignMapper and a MeterColorMapper to do transforms where
  // applicable.
  const IR::Statement* FindTestStatement(
      const std::string& control, int statement_index) {
    std::vector<IRTestHelperJson::IRControlTransforms> transform_list(
        {IRTestHelperJson::kHitAssignMapper,
         IRTestHelperJson::kMeterColorMapper});
    const IR::P4Control* ir_control =
        ir_helper_->TransformP4Control(control, transform_list);
    if (ir_control == nullptr) return nullptr;
    if (statement_index >= 0) {
      if (statement_index >= ir_control->body->components.size())
        return nullptr;
      return ir_control->body->components[statement_index]->to<IR::Statement>();
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
  bool expect_apply() const {
    return ::testing::get<3>(GetParam());
  }
  bool expect_error() const {
    return ::testing::get<4>(GetParam());
  }

  // TableHitInspector instance for test use; created by SetUpTestIR.
  std::unique_ptr<TableHitInspector> test_inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

TEST_P(TableHitInspectorTest, TestInspect) {
  SetUpTestIR(test_ir_file());
  const IR::Statement* test_statement =
      FindTestStatement(control_name(), statement_index());
  ASSERT_TRUE(test_statement != nullptr);
  EXPECT_EQ(expect_apply(), test_inspector_->Inspect(*test_statement));
  EXPECT_EQ(expect_error(), ::errorCount() != 0);
}

// Test parameters:
//  1) Name of JSON file with test P4 IR.
//  2) Name of control in the IR to be tested.
//  3) Statement index within control body (-1 for entire body).
//  4) Flag indicating whether to expect control to apply tables.
//  5) Flag indicating whether to expect errors after Inspect.
INSTANTIATE_TEST_CASE_P(
    NoApplyNoErrorTests,
    TableHitInspectorTest,
    Values(
        std::make_tuple("control_if_test.ir.json",
                        "computeChecksum", -1, false, false),
        std::make_tuple("control_if_test.ir.json", "egress", -1, false, false),
        std::make_tuple("control_if_test.ir.json",
                        "verifyChecksum", -1, false, false),
        std::make_tuple("control_misc_test.ir.json",
                        "computeChecksum", -1, false, false)
    ));

INSTANTIATE_TEST_CASE_P(
    ApplyNoErrorTests,
    TableHitInspectorTest,
    Values(
        std::make_tuple("complex_hits.ir.json", "complex_hits", 7, true, false),
        std::make_tuple("control_apply_hit_miss_test.ir.json",
                        "egress", -1, true, false),
        std::make_tuple("control_apply_hit_miss_test.ir.json",
                        "ingress", -1, true, false),
        std::make_tuple("control_if_test.ir.json", "ingress", -1, true, false),
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
                        "hit_var_valid_statements", 1, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 2, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 3, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 4, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 5, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 6, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 7, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 8, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 9, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 10, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", 11, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_valid_statements", -1, true, false),
        std::make_tuple("control_return.ir.json",
                        "early_return", -1, true, false),
        std::make_tuple("control_return.ir.json",
                        "control_nested_return", -1, true, false),
        std::make_tuple("table_hit_tmp_valid.ir.json",
                        "hit_var_scope_ok", -1, true, false)
    ));

INSTANTIATE_TEST_CASE_P(
    ApplyWithErrorTests,
    TableHitInspectorTest,
    Values(
        std::make_tuple("complex_hits.ir.json", "complex_hits", 2, true, true),
        std::make_tuple("hidden_table1.ir.json", "ingress", -1, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 0, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 1, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 2, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 3, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 4, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 5, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 6, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 7, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 8, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 9, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 10, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 12, true, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", -1, true, true)
    ));

INSTANTIATE_TEST_CASE_P(
    NoApplyWithErrorTests,
    TableHitInspectorTest,
    Values(
        std::make_tuple("complex_hits.ir.json", "complex_hits", 0, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 1, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 3, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 4, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 5, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 6, false, true),
        std::make_tuple("complex_hits.ir.json", "complex_hits", 8, false, true),
        std::make_tuple("meter_color_if_else.ir.json",
                        "meter_if_else", -1, false, true),
        std::make_tuple("meter_colors.ir.json",
                        "meter_if_green", -1, false, true),
        std::make_tuple("meter_colors.ir.json",
                        "meter_if_red", -1, false, true),
        std::make_tuple("meter_colors.ir.json",
                        "meter_if_yellow", -1, false, true),
        std::make_tuple("table_hit_tmp_invalid.ir.json",
                        "hit_var_invalid", 11, false, true)
    ));

}  // namespace p4c_backends
}  // namespace stratum
