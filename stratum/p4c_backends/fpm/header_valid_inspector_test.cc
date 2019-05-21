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

// Contains unit tests for HeaderValidInspector.

#include "stratum/p4c_backends/fpm/header_valid_inspector.h"

#include <set>
#include <string>

#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace p4c_backends {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Range;

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.  The test parameter is used by some tests to select a
// specific statement for testing.
class HeaderValidInspectorTest : public testing::TestWithParam<int> {
 public:
  static void SetUpTestCase() {
    SetUpTestP4ModelNames();
  }

 protected:
  HeaderValidInspectorTest() {
    expected_single_header_set_ = {kHeader1Name};
    expected_multi_header_set_ = {kHeader1Name, kHeader2Name};
  }

  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also prepares a HeaderValidInspector test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    ir_helper_->set_color_field_name("meta.enum_color");
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    test_inspector_ = absl::make_unique<HeaderValidInspector>(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
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

  // HeaderValidInspector instance for test use; created by SetUpTestIR.
  std::unique_ptr<HeaderValidInspector> test_inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  TableMapGeneratorMock mock_table_mapper_;  // Common TableMapGenerator mock.

  // These sets define the expected headers to be added via mock_table_mapper_.
  std::set<std::string> expected_single_header_set_;
  std::set<std::string> expected_multi_header_set_;

  // Table and header names from the test P4 program.
  static constexpr const char* kTable1Name = "good_statements.table1";
  static constexpr const char* kTable2Name = "good_statements.table2";
  static constexpr const char* kHeader1Name = "hdr.ethernet";
  static constexpr const char* kHeader2Name = "hdr.ethernet2";
};

// These additional type definitions select different sets of test parameters
typedef HeaderValidInspectorTest HeaderValidInspectorTestTable1;
typedef HeaderValidInspectorTest HeaderValidInspectorTestTable1And2;
typedef HeaderValidInspectorTest HeaderValidInspectorTestMultiHeader;
typedef HeaderValidInspectorTest HeaderValidInspectorTestErrors;

// The parameter for this test gives an index to a statement that
// HeaderValidInspector supports, but determines that no table updates
// are necessary.
TEST_P(HeaderValidInspectorTest, TestGoodStatementNoTableUpdates) {
  SetUpTestIR("header_valid_tests.ir.json");
  const IR::Statement* statement =
      FindTestStatement("good_statements", GetParam());
  ASSERT_TRUE(statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, SetTableValidHeaders(_, _)).Times(0);
  test_inspector_->Inspect(*statement, &mock_table_mapper_);
  EXPECT_EQ(0, ::errorCount());
}

// This test's parameter is an index to a statement with a supported
// header valid condition that applies to one table, kTable1Name.
TEST_P(HeaderValidInspectorTestTable1, TestGoodStatementTable1Update) {
  SetUpTestIR("header_valid_tests.ir.json");
  const IR::Statement* statement =
      FindTestStatement("good_statements", GetParam());
  ASSERT_TRUE(statement != nullptr);
  EXPECT_CALL(mock_table_mapper_,
              SetTableValidHeaders(kTable1Name, expected_single_header_set_))
      .Times(1);
  test_inspector_->Inspect(*statement, &mock_table_mapper_);
  EXPECT_EQ(0, ::errorCount());
}

// This test's parameter is an index to a statement with a supported
// header valid condition that applies to two tables, kTable1Name and
// kTable2Name.
TEST_P(HeaderValidInspectorTestTable1And2, TestGoodStatementTable1And2Update) {
  SetUpTestIR("header_valid_tests.ir.json");
  const IR::Statement* statement =
      FindTestStatement("good_statements", GetParam());
  ASSERT_TRUE(statement != nullptr);
  EXPECT_CALL(mock_table_mapper_,
              SetTableValidHeaders(kTable1Name, expected_single_header_set_))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_,
              SetTableValidHeaders(kTable2Name, expected_single_header_set_))
      .Times(1);
  test_inspector_->Inspect(*statement, &mock_table_mapper_);
  EXPECT_EQ(0, ::errorCount());
}

// This test's parameter is an index to a statement with a multiple
// header valid conditions applied to multiple tables.
TEST_P(HeaderValidInspectorTestMultiHeader, TestGoodStatementMultiHeader) {
  SetUpTestIR("header_valid_tests.ir.json");
  const IR::Statement* statement =
      FindTestStatement("good_statements", GetParam());
  ASSERT_TRUE(statement != nullptr);
  EXPECT_CALL(mock_table_mapper_,
              SetTableValidHeaders(kTable1Name, expected_single_header_set_))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_,
              SetTableValidHeaders(kTable2Name, expected_multi_header_set_))
      .Times(1);
  test_inspector_->Inspect(*statement, &mock_table_mapper_);
  EXPECT_EQ(0, ::errorCount());
}

// This test's parameter is an index to a statement with unsupported header
// valid conditions that HeaderValidInspector should report as errors.
TEST_P(HeaderValidInspectorTestErrors, TestErrorStatements) {
  SetUpTestIR("header_valid_tests.ir.json");
  const IR::Statement* statement =
      FindTestStatement("bad_statements", GetParam());
  ASSERT_TRUE(statement != nullptr);
  EXPECT_CALL(mock_table_mapper_, SetTableValidHeaders(_, _))
      .Times(AnyNumber());
  test_inspector_->Inspect(*statement, &mock_table_mapper_);
  EXPECT_NE(0, ::errorCount());
}

INSTANTIATE_TEST_SUITE_P(
    NoTableUpdates,
    HeaderValidInspectorTest,
    Range(0, 5));

INSTANTIATE_TEST_SUITE_P(
    Table1Updates,
    HeaderValidInspectorTestTable1,
    Range(5, 11));

INSTANTIATE_TEST_SUITE_P(
    Table1And2Updates,
    HeaderValidInspectorTestTable1And2,
    Range(11, 15));

INSTANTIATE_TEST_SUITE_P(
    MultiHeader,
    HeaderValidInspectorTestMultiHeader,
    Range(15, 19));

INSTANTIATE_TEST_SUITE_P(
    Errors,
    HeaderValidInspectorTestErrors,
    Range(0, 9));

}  // namespace p4c_backends
}  // namespace stratum
