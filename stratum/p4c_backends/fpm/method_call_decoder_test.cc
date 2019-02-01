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

// Contains unit tests for MethodCallDecoder.

#include "stratum/p4c_backends/fpm/method_call_decoder.h"

#include <memory>

#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "stratum/public/proto/p4_table_defs.host.pb.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data with IR::MethodCallStatements for test use.
class MethodCallDecoderTest : public testing::Test {
 protected:
  // The SetUpTestIR method uses an IRTestHelperJson to load an IR file in JSON
  // format.  It also creates a MethodCallDecoder test instance.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::MakeUnique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
    SetUpTestP4ModelNames();
    method_call_decoder_ = absl::MakeUnique<MethodCallDecoder>(
        ir_helper_->mid_end_refmap(), ir_helper_->mid_end_typemap());
  }

  // Returns a MethodCallStatement from the given action name.  For these
  // tests, each action in the "ingress" control has a single method call.
  const IR::MethodCallStatement* FindMethodStatement(
      const std::string& action_name) {
    const IR::P4Control* ir_control = ir_helper_->GetP4Control("ingress");
    if (ir_control != nullptr) {
      const IR::P4Action* method_action = nullptr;

      // The p4c visitor.h file provides this Inspector-like template to
      // iterate over specific objects in an IR::Node.  It is used here to
      // search all the actions within ir_control.
      forAllMatching<IR::P4Action>(&ir_control->controlLocals,
                                   [&](const IR::P4Action* action) {
        if (action->externalName() == action_name)
          method_action = action;
      });

      if (method_action == nullptr || method_action->body == nullptr)
        return nullptr;
      if (method_action->body->components.size() != 1)
        return nullptr;
      return method_action->body->components[0]->to<IR::MethodCallStatement>();
    }
    return nullptr;
  }

  // SwitchCaseDecoder instance for test use; created by SetUpTestIR.
  std::unique_ptr<MethodCallDecoder> method_call_decoder_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
};

TEST_F(MethodCallDecoderTest, TestDrop) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.drop_statement");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());

  // The tested statement should produce output in the method_op accessor.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder_->method_op();
  EXPECT_EQ(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            method_op.assigned_value().source_value_case());
  EXPECT_TRUE(method_op.destination_field_name().empty());
  ASSERT_EQ(1, method_op.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_DROP, method_op.primitives(0));
  EXPECT_EQ(P4_HEADER_NOP, method_call_decoder_->tunnel_op().header_op());
  EXPECT_TRUE(method_call_decoder_->tunnel_op().header_name().empty());
}

TEST_F(MethodCallDecoderTest, TestClone3) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.clone3_statement_const_port");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());

  // The tested statement should produce output in the method_op accessor.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder_->method_op();
  EXPECT_EQ(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            method_op.assigned_value().source_value_case());
  EXPECT_TRUE(method_op.destination_field_name().empty());
  ASSERT_EQ(1, method_op.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, method_op.primitives(0));
  EXPECT_EQ(P4_HEADER_NOP, method_call_decoder_->tunnel_op().header_op());
  EXPECT_TRUE(method_call_decoder_->tunnel_op().header_name().empty());
  // TODO: Verify clone3 parameters when implemented.
}

TEST_F(MethodCallDecoderTest, TestClone) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.clone_statement_const_port");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());

  // The tested statement should produce output in the method_op accessor.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder_->method_op();
  EXPECT_EQ(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            method_op.assigned_value().source_value_case());
  EXPECT_TRUE(method_op.destination_field_name().empty());
  ASSERT_EQ(1, method_op.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_CLONE, method_op.primitives(0));
  EXPECT_EQ(P4_HEADER_NOP, method_call_decoder_->tunnel_op().header_op());
  EXPECT_TRUE(method_call_decoder_->tunnel_op().header_name().empty());
  // TODO: Verify clone parameters when implemented.
}

TEST_F(MethodCallDecoderTest, TestDirectCounter) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.count_direct_counter");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());

  // The tested statement should produce a NOP primitive in the
  // method_op accessor.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder_->method_op();
  EXPECT_EQ(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            method_op.assigned_value().source_value_case());
  EXPECT_TRUE(method_op.destination_field_name().empty());
  ASSERT_EQ(1, method_op.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_NOP, method_op.primitives(0));
  EXPECT_EQ(P4_HEADER_NOP, method_call_decoder_->tunnel_op().header_op());
  EXPECT_TRUE(method_call_decoder_->tunnel_op().header_name().empty());
}

TEST_F(MethodCallDecoderTest, TestDirectMeter) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.read_direct_meter");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());

  // The tested statement should produce a NOP primitive in the
  // method_op accessor.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder_->method_op();
  EXPECT_EQ(P4AssignSourceValue::SOURCE_VALUE_NOT_SET,
            method_op.assigned_value().source_value_case());
  EXPECT_TRUE(method_op.destination_field_name().empty());
  ASSERT_EQ(1, method_op.primitives_size());
  EXPECT_EQ(P4_ACTION_OP_NOP, method_op.primitives(0));
  EXPECT_EQ(P4_HEADER_NOP, method_call_decoder_->tunnel_op().header_op());
  EXPECT_TRUE(method_call_decoder_->tunnel_op().header_name().empty());
}

TEST_F(MethodCallDecoderTest, TestCounter) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.count_counter");
  ASSERT_NE(nullptr, test_statement);

  // TODO: Enhance this test after non-direct counter implementation.
  EXPECT_FALSE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_FALSE(method_call_decoder_->error_message().empty());
}

TEST_F(MethodCallDecoderTest, TestExecuteMeter) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.execute_meter_meter");
  ASSERT_NE(nullptr, test_statement);

  // TODO: Enhance this test after non-direct meter implementation.
  EXPECT_FALSE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_FALSE(method_call_decoder_->error_message().empty());
}

TEST_F(MethodCallDecoderTest, TestBuiltinSetInvalid) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.set_header_invalid");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());
  const hal::P4ActionDescriptor::P4TunnelAction& tunnel_op =
      method_call_decoder_->tunnel_op();

  // The tested statement should produce output in the tunnel_op accessor.
  EXPECT_EQ(P4_HEADER_SET_INVALID, tunnel_op.header_op());
  EXPECT_EQ("hdr.ethernet", tunnel_op.header_name());
  EXPECT_TRUE(
      method_call_decoder_->method_op().destination_field_name().empty());
  EXPECT_EQ(0, method_call_decoder_->method_op().primitives_size());
}

TEST_F(MethodCallDecoderTest, TestBuiltinSetValid) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.set_header_valid");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());
  const hal::P4ActionDescriptor::P4TunnelAction& tunnel_op =
      method_call_decoder_->tunnel_op();

  // The tested statement should produce output in the tunnel_op accessor.
  EXPECT_EQ(P4_HEADER_SET_VALID, tunnel_op.header_op());
  EXPECT_EQ("hdr.ethernet", tunnel_op.header_name());
  EXPECT_TRUE(
      method_call_decoder_->method_op().destination_field_name().empty());
  EXPECT_EQ(0, method_call_decoder_->method_op().primitives_size());
}

TEST_F(MethodCallDecoderTest, TestDecodeTwice) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.drop_statement");
  ASSERT_NE(nullptr, test_statement);
  EXPECT_TRUE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_TRUE(method_call_decoder_->error_message().empty());
  EXPECT_FALSE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_FALSE(method_call_decoder_->error_message().empty());
}

TEST_F(MethodCallDecoderTest, TestUnsupportedExternFunction) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.call_unsupported");
  ASSERT_NE(nullptr, test_statement);

  // The extern function in the test statement is unsupported.
  EXPECT_FALSE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_FALSE(method_call_decoder_->error_message().empty());
}

TEST_F(MethodCallDecoderTest, TestIgnoredBuiltIn) {
  SetUpTestIR("method_calls.ir.json");
  const IR::MethodCallStatement* test_statement =
      FindMethodStatement("ingress.call_ignored_builtin");
  ASSERT_NE(nullptr, test_statement);

  // The push_front function in the test statement is ignored.
  EXPECT_FALSE(method_call_decoder_->DecodeStatement(*test_statement));
  EXPECT_FALSE(method_call_decoder_->error_message().empty());
}

}  // namespace p4c_backends
}  // namespace stratum
