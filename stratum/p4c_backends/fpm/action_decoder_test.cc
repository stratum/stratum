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

// Contains ActionDecoder unit tests.
// Significant ActionDecoder test coverage currently occurs
// indirectly via its use while running SwitchP4cBackendTest.

#include "stratum/p4c_backends/fpm/action_decoder.h"

#include <memory>
#include <set>
#include <string>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"

using ::testing::_;

namespace stratum {
namespace p4c_backends {

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

// This class is the ActionDecoder test fixture.
class ActionDecoderTest : public testing::Test {
 protected:
  // The SetUpTestIR uses ir_helper_ to load an IR file in JSON format and
  // applies ProgramInspector to record IR nodes that contain some P4Action
  // nodes to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Searches the P4 test program for an action with the given name.
  const IR::P4Action* GetTestAction(const std::string& action_name) {
    for (auto action_map_iter : ir_helper_->program_inspector().actions()) {
      const IR::P4Action* action = action_map_iter.first;
      if (std::string(action->externalName()) == action_name) return action;
    }
    return nullptr;
  }

  TableMapGeneratorMock mock_table_mapper_;  // Mock to test table map changes.
  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.
  P4AssignSourceValue expected_value_;           // Expected field assign match.
};

// Verifies table map action descriptor setup when an action assigns a
// constant to a field.
TEST_F(ActionDecoderTest, TestAssignConstant) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_constant";
  expected_value_.set_constant_param(1);
  expected_value_.set_bit_width(32);
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(
                  kTestActionName, EqualsProto(expected_value_), "meta.color"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, AddTunnelAction(_, _)).Times(0);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// Verifies table map action descriptor setup when an action assigns the
// same constant to fields with different widths.
TEST_F(ActionDecoderTest, TestAssignMultiWidthConstant) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_constant_multi_width";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  expected_value_.set_constant_param(123);
  expected_value_.set_bit_width(32);
  EXPECT_CALL(
      mock_table_mapper_,
      AssignActionSourceValueToField(
          kTestActionName, EqualsProto(expected_value_), "meta.other_metadata"))
      .Times(1);
  P4AssignSourceValue expected_value_2;
  expected_value_2.set_constant_param(123);
  expected_value_2.set_bit_width(16);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_2),
                                             "meta.smaller_metadata"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, AddTunnelAction(_, _)).Times(0);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// Verifies table map action descriptor setup when an action assigns one
// of its parameters to a field.
TEST_F(ActionDecoderTest, TestAssignParam) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_param";
  expected_value_.set_parameter_name("dmac");
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_),
                                             "hdr.ethernet.dstAddr"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, AddTunnelAction(_, _)).Times(0);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// Verifies table map action descriptor setup when an action assigns the
// same parameter to multiple fields.
TEST_F(ActionDecoderTest, TestAssignParamMultiple) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_param_multiple";
  expected_value_.set_parameter_name("mac");
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_),
                                             "hdr.ethernet.dstAddr"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_),
                                             "hdr.ethernet.srcAddr"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, AddTunnelAction(_, _)).Times(0);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// Verifies table map action descriptor setup when an action assigns one
// header field to another.
TEST_F(ActionDecoderTest, TestAssignFieldToField) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_field_to_field";
  expected_value_.set_source_field_name("hdr.ethernet.srcAddr");
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_),
                                             "hdr.ethernet.dstAddr"))
      .Times(1);
  EXPECT_CALL(mock_table_mapper_, AddTunnelAction(_, _)).Times(0);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// Verifies table map action descriptor setup when an action does a
// header-to-header copy assignment.
TEST_F(ActionDecoderTest, TestAssignHeaderCopy) {
  SetUpTestIR("action_assignments.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.assign_header_copy";
  expected_value_.set_source_header_name("hdr.ethernet");
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(
      mock_table_mapper_,
      AssignHeaderToHeader(kTestActionName, EqualsProto(expected_value_),
                           "hdr.ethernet2"))
      .Times(1);
  hal::P4ActionDescriptor::P4TunnelAction expected_encap;
  expected_encap.set_header_name("hdr.ethernet2");
  expected_encap.set_header_op(P4_HEADER_COPY_VALID);
  EXPECT_CALL(mock_table_mapper_,
              AddTunnelAction(kTestActionName, EqualsProto(expected_encap)))
      .Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for an empty action body should be an action
// descriptor with a NOP primitive.
TEST_F(ActionDecoderTest, TestEmptyStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.empty_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_, AddNopPrimitive(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for an exit statement should be an empty
// action descriptor.
TEST_F(ActionDecoderTest, TestExitStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.exit_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for a return statement should be an empty
// action descriptor.
TEST_F(ActionDecoderTest, TestReturnStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.return_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  EXPECT_CALL(mock_table_mapper_, AddNopPrimitive(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for a built-in setValid statement should be an action
// descriptor with a tunnel encap action added.
TEST_F(ActionDecoderTest, TestHeaderEncapStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.set_valid_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  hal::P4ActionDescriptor::P4TunnelAction expected_tunnel_action;
  expected_tunnel_action.set_header_name("hdr.ethernet2");
  expected_tunnel_action.set_header_op(P4_HEADER_SET_VALID);
  EXPECT_CALL(
      mock_table_mapper_,
      AddTunnelAction(kTestActionName, EqualsProto(expected_tunnel_action)))
      .Times(1);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for a built-in setInvalid statement should be an action
// descriptor with a tunnel decap action added.
TEST_F(ActionDecoderTest, TestHeaderDecapStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.set_invalid_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  hal::P4ActionDescriptor::P4TunnelAction expected_tunnel_action;
  expected_tunnel_action.set_header_name("hdr.ethernet2");
  expected_tunnel_action.set_header_op(P4_HEADER_SET_INVALID);
  EXPECT_CALL(
      mock_table_mapper_,
      AddTunnelAction(kTestActionName, EqualsProto(expected_tunnel_action)))
      .Times(1);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// The table map output for a built-in push_front statement should be an
// empty action descriptor.
TEST_F(ActionDecoderTest, TestHeaderStackPushStatement) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.push_statement";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

// This test makes sure an ActionDecoder can handle a sequence of nested
// BlockStatements.  The table map output should contain the statements for
// both nested blocks in the test action.
TEST_F(ActionDecoderTest, TestTwoNestedBlocks) {
  SetUpTestIR("action_misc.ir.json");
  ActionDecoder test_decoder(&mock_table_mapper_, ir_helper_->mid_end_refmap(),
                             ir_helper_->mid_end_typemap());
  const std::string kTestActionName = "ingress.two_nested_blocks";
  EXPECT_CALL(mock_table_mapper_, AddAction(kTestActionName)).Times(1);
  const IR::P4Action* action = GetTestAction(kTestActionName);
  ASSERT_NE(nullptr, action);
  expected_value_.set_constant_param(1);
  expected_value_.set_bit_width(16);
  EXPECT_CALL(mock_table_mapper_,
              AssignActionSourceValueToField(kTestActionName,
                                             EqualsProto(expected_value_),
                                             "meta.smaller_metadata"))
      .Times(1);
  expected_value_.set_constant_param(2);
  expected_value_.set_bit_width(32);
  EXPECT_CALL(
      mock_table_mapper_,
      AssignActionSourceValueToField(
          kTestActionName, EqualsProto(expected_value_), "meta.other_metadata"))
      .Times(1);
  test_decoder.ConvertActionBody(kTestActionName, action->body->components);
}

}  // namespace p4c_backends
}  // namespace stratum
