// Contains ExpressionInspector unit tests.

#include "platforms/networking/hercules/p4c_backend/switch/expression_inspector.h"

#include "platforms/networking/hercules/p4c_backend/switch/utils.h"
#include "platforms/networking/hercules/p4c_backend/test/ir_test_helpers.h"
#include "platforms/networking/hercules/public/proto/p4_table_defs.host.pb.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace google {
namespace hercules {
namespace p4c_backend {

// This class is the ExpressionInspector test fixture.
class ExpressionInspectorTest : public testing::Test {
 protected:
  // SetUpTestIR uses ir_helper_ to load an IR file in JSON format and
  // applies ProgramInspector to record IR nodes that contain some P4Action
  // nodes with expressions to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "platforms/networking/hercules/p4c_backend/switch/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    SetUpTestP4ModelNames();
  }

  // Searches the P4 test program for an action with the given name, then
  // returns the right-side expression from the first statement in the
  // action body, which is expected to be an IR::AssignmentStatement.
  const IR::Expression* GetTestExpression(const std::string& action_name) {
    for (auto action_map_iter : ir_helper_->program_inspector().actions()) {
      const IR::P4Action* action = action_map_iter.first;
      if (std::string(action->externalName()) != action_name) continue;
      if (action->body == nullptr) break;
      if (action->body->components.empty()) break;
      auto assign = action->body->components[0]->to<IR::AssignmentStatement>();
      if (assign == nullptr) break;
      return assign->right;
    }
    return nullptr;
  }

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  P4AssignSourceValue expected_value_;  // Expected output value per test.
};

// Tests constant expression with value 1.
TEST_F(ExpressionInspectorTest, TestConstant) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_constant");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kConstantParam, value.source_value_case());
  EXPECT_EQ(1, value.constant_param());
  EXPECT_EQ(32, value.bit_width());
}

// Tests constant expression with enum value.  The ExpressionInspector should
// see the COLOR_YELLOW value as the literal value 2.
TEST_F(ExpressionInspectorTest, TestEnum) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_enum");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  // TODO(teverman): Enum support requires implementation in the Member
  // preorder.  Adjust these expectations when the implementation is ready.
  EXPECT_FALSE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
}

// Tests action parameter assignment expression with parameter "dmac".
TEST_F(ExpressionInspectorTest, TestParameterName) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_param");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kParameterName, value.source_value_case());
  EXPECT_EQ("dmac", value.parameter_name());
}

// Tests header field expression.
TEST_F(ExpressionInspectorTest, TestHeaderField) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_field_to_field");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kSourceFieldName, value.source_value_case());
  EXPECT_EQ("hdr.ethernet.srcAddr", value.source_field_name());
}

// Tests an expression that copies an entire header.
TEST_F(ExpressionInspectorTest, TestHeaderCopy) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_header_copy");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kSourceHeaderName, value.source_value_case());
  EXPECT_EQ("hdr.ethernet", value.source_header_name());
}

// Tests an expression that slices an action parameter into bit fields.
TEST_F(ExpressionInspectorTest, TestParameterSlice) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_parameter_slice");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kParameterName, value.source_value_case());
  EXPECT_EQ("sliced_flags", value.parameter_name());
  EXPECT_EQ(2, value.bit_width());
  EXPECT_EQ(4, value.high_bit());
}

// Tests an expression that slices a metadata field into bit fields.
TEST_F(ExpressionInspectorTest, TestMetadataSlice) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_metadata_slice");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kSourceFieldName, value.source_value_case());
  EXPECT_EQ("meta.other_metadata", value.source_field_name());
  EXPECT_EQ(16, value.bit_width());
  EXPECT_EQ(31, value.high_bit());
}

// Tests an expression that adds a constant to a header field.
TEST_F(ExpressionInspectorTest, TestAdd) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_add");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  // Expects false - currently not implemented.
  EXPECT_FALSE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
}

// Tests an expression that copies header stack elements.
TEST_F(ExpressionInspectorTest, TestHeaderStackCopy) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_header_stack");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_TRUE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(0, ::errorCount());
  const P4AssignSourceValue& value = test_inspector.value();
  EXPECT_EQ(P4AssignSourceValue::kSourceHeaderName, value.source_value_case());
  EXPECT_EQ("hdr.ethernet_stack[0]", value.source_header_name());
}

// Tests an expression with an array index that is not a constant.
TEST_F(ExpressionInspectorTest, TestIndexNotConstant) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_non_const_array_index");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_FALSE(test_inspector.Inspect(*test_expression));
  EXPECT_NE(0, ::errorCount());
}

// Tests an expression with a temporary variable array.
TEST_F(ExpressionInspectorTest, TestTempHeaderStack) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_temp_array");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_FALSE(test_inspector.Inspect(*test_expression));
  EXPECT_NE(0, ::errorCount());
}

// Tests an unsupported expression.
TEST_F(ExpressionInspectorTest, TestUnsupported) {
  SetUpTestIR("action_assignments.ir.json");
  const IR::Expression* test_expression =
      GetTestExpression("ingress.assign_unsupported");
  ASSERT_NE(nullptr, test_expression);
  ExpressionInspector test_inspector(ir_helper_->mid_end_refmap(),
                                     ir_helper_->mid_end_typemap());
  EXPECT_FALSE(test_inspector.Inspect(*test_expression));
  EXPECT_EQ(1, ::errorCount());  // Unsupported expressions are program errors.
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
