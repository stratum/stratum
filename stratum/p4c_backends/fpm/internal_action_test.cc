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

// This file tests the InternalAction class.

#include "stratum/p4c_backends/fpm/internal_action.h"

#include "stratum/hal/lib/p4/p4_pipeline_config.host.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/table_map_generator_mock.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_mock.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace p4c_backends {

using ::testing::_;
using ::testing::DoAll;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SetArgPointee;

MATCHER_P(EqualsProto, proto, "") { return ProtoEqual(arg, proto); }

class InternalActionTest : public testing::Test {
 protected:
  static constexpr const char* kOriginalAction = "original-action";
  static constexpr const char* kNopAction = "nop-action";
  static constexpr const char* kAssignmentAction = "assignment-action";
  static constexpr const char* kLinkedInternalActionName = "internal-action";
  static constexpr const char* kActionToInternalName = "action-to-internal";

  // The SetUp method creates an original_action_ and uses it to construct
  // an InternalAction for testing.  The original_action_ contains a single
  // assignment statement of an action parameter to a field.
  void SetUp() override {
    const std::string test_action_name = kOriginalAction;
    table_map_generator_.AddAction(test_action_name);
    table_map_generator_.AssignActionParameterToField(
        test_action_name, "param0", "field0");
    original_action_ = FindActionDescriptorOrDie(
        test_action_name, table_map_generator_.generated_map());
    test_internal_action_ =
        absl::make_unique<InternalAction>(test_action_name, original_action_,
                                          table_map_generator_.generated_map(),
                                          &tunnel_optimizer_mock_);
  }

  // These functions add various types of assignments into kAssignmentAction.
  void AddParameterAssignment(const std::string& dest_field,
                              const std::string& action_param) {
    table_map_generator_.AddAction(kAssignmentAction);
    table_map_generator_.AssignActionParameterToField(
        kAssignmentAction, action_param, dest_field);
  }

  void AddConstantAssignment(const std::string& dest_field,
                             int64 constant_64) {
    P4AssignSourceValue source_value;
    source_value.set_constant_param(constant_64);
    table_map_generator_.AddAction(kAssignmentAction);
    table_map_generator_.AssignActionSourceValueToField(
        kAssignmentAction, source_value, dest_field);
  }

  void AddFieldAssignment(const std::string& dest_field,
                          const std::string& source_field,
                          const int slice_bit_width = 0) {
    P4AssignSourceValue source_value;
    source_value.set_source_field_name(source_field);
    source_value.set_bit_width(slice_bit_width);
    table_map_generator_.AddAction(kAssignmentAction);
    table_map_generator_.AssignActionSourceValueToField(
        kAssignmentAction, source_value, dest_field);
  }

  void MergeAssignmentAction() {
    test_internal_action_->MergeAction(kAssignmentAction);
  }

  // Replaces the test_internal_action_ provided by the SetUp method with an
  // action that contains only a NOP.
  void ReplaceOriginalActionWithNop() {
    const std::string test_action_name = kNopAction;
    table_map_generator_.AddAction(test_action_name);
    table_map_generator_.AddNopPrimitive(test_action_name);
    original_action_ = FindActionDescriptorOrDie(
        test_action_name, table_map_generator_.generated_map());
    test_internal_action_ = absl::make_unique<InternalAction>(
        test_action_name, original_action_,
        table_map_generator_.generated_map(), &tunnel_optimizer_mock_);
  }

  // Sets up a pair of action descriptors.  The first action is an
  // InternalAction.  The second action represents a P4 program action
  // that has already been linked to the first action.  This setup
  // facilitates testing the merging of actions that have already been
  // merged with another action.
  void SetUpLinkedActions() {
    // The InternalAction contains a drop primitive as the merged content.
    hal::P4ActionDescriptor internal_action;
    internal_action.set_type(P4_ACTION_TYPE_FUNCTION);
    internal_action.add_primitive_ops(P4_ACTION_OP_DROP);
    table_map_generator_.AddInternalAction(
        kLinkedInternalActionName, internal_action);

    // The regular P4 action descriptor contains a clone primitive and a
    // link to the internal action above.
    table_map_generator_.AddAction(kActionToInternalName);
    hal::P4ActionDescriptor action_to_internal = FindActionDescriptorOrDie(
        kActionToInternalName, table_map_generator_.generated_map());
    action_to_internal.add_primitive_ops(P4_ACTION_OP_CLONE);
    auto redirect = action_to_internal.add_action_redirects();
    redirect->add_internal_links()->set_internal_action_name(
        kLinkedInternalActionName);
    table_map_generator_.ReplaceActionDescriptor(
        kActionToInternalName, action_to_internal);
  }

  // Counts the number of times that test_internal_action_ assigns param_name.
  int CountParameterAssignments(const std::string& param_name) {
    int param_count = 0;
    for (const auto& assignment :
         test_internal_action_->internal_descriptor().assignments()) {
      if (assignment.assigned_value().source_value_case() ==
          P4AssignSourceValue::kParameterName) {
        if (param_name == assignment.assigned_value().parameter_name()) {
          ++param_count;
        }
      }
    }

    return param_count;
  }

  // Counts the number of times that assignments in test_internal_action_
  // refer to field_name as either a source or destination.
  int CountFieldReferences(const std::string& field_name) {
    int field_count = 0;
    for (const auto& assignment :
         test_internal_action_->internal_descriptor().assignments()) {
      if (assignment.assigned_value().source_value_case() ==
          P4AssignSourceValue::kSourceFieldName) {
        if (field_name == assignment.assigned_value().source_field_name()) {
          ++field_count;
        }
      }
      if (assignment.destination_field_name() == field_name) {
        ++field_count;
      }
    }

    return field_count;
  }

  // The SetUp method creates this InternalAction instance for test use.
  // This instance always has a TunnelOptimizerMock.
  std::unique_ptr<InternalAction> test_internal_action_;

  // TunnelOptimizerMock for test_internal_action_.
  TunnelOptimizerMock tunnel_optimizer_mock_;

  // The TableMapGenerator facilitates easy test setup of action descriptors.
  TableMapGenerator table_map_generator_;
  hal::P4ActionDescriptor original_action_;  // Populated by SetUp.
};

// Verifies that the constructed InternalAction contains a unique name and
// a copy of the original action descriptor.
TEST_F(InternalActionTest, TestUnmerged) {
  EXPECT_TRUE(ProtoEqual(original_action_,
                         test_internal_action_->internal_descriptor()));
  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find(kOriginalAction));
  EXPECT_LT(strlen(kOriginalAction),
            test_internal_action_->internal_name().size());
}

// Verifies that a merge of an empty action updates the internal name,
// but does not alter the internal descriptor.
TEST_F(InternalActionTest, TestMergeActionNameOnly) {
  const std::string kMergeAction = "merge-action";
  table_map_generator_.AddAction(kMergeAction);  // Added with no content.

  test_internal_action_->MergeAction(kMergeAction);

  EXPECT_TRUE(ProtoEqual(original_action_,
                         test_internal_action_->internal_descriptor()));
  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find(kOriginalAction));
  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find(kMergeAction));
}

// Verifies a merge of a populated hidden action with the original action.
TEST_F(InternalActionTest, TestMergeAction) {
  // The "hidden-action" adds two action parameter-to-field assignments
  // and a drop primitive.
  const std::string kMergeAction = "hidden-action";
  table_map_generator_.AddAction(kMergeAction);
  table_map_generator_.AssignActionParameterToField(
      kMergeAction, "param1", "field1");
  table_map_generator_.AssignActionParameterToField(
      kMergeAction, "param2", "field2");
  table_map_generator_.AddDropPrimitive(kMergeAction);
  const auto& hidden_action = FindActionDescriptorOrDie(
      kMergeAction, table_map_generator_.generated_map());

  test_internal_action_->MergeAction(kMergeAction);

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  ASSERT_EQ(
      hidden_action.assignments_size() + original_action_.assignments_size(),
      internal_descriptor.assignments_size());
  const int original_assignments_size = original_action_.assignments_size();
  for (int i = 0; i < original_assignments_size; ++i) {
    EXPECT_TRUE(ProtoEqual(original_action_.assignments(i),
                           internal_descriptor.assignments(i)));
  }
  for (int i = 0; i < hidden_action.assignments_size(); ++i) {
    EXPECT_TRUE(ProtoEqual(
        hidden_action.assignments(i),
        internal_descriptor.assignments(i + original_assignments_size)));
  }
  ASSERT_EQ(1, internal_descriptor.primitive_ops_size());
  EXPECT_EQ(hidden_action.primitive_ops(0),
            internal_descriptor.primitive_ops(0));
}

// Verifies that for simple action merges, the same output occurs regardless
// of whether a tunnel optimizer is used.
TEST_F(InternalActionTest, TestNoTunnelOptimizerEffect) {
  std::unique_ptr<InternalAction> test_internal_action_no_opt;
  const std::string& original_name = kOriginalAction;
  test_internal_action_no_opt = absl::make_unique<InternalAction>(
      original_name, original_action_, table_map_generator_.generated_map());

  // The "hidden-action" adds an action parameter-to-field assignment
  // and a drop primitive.
  const std::string kMergeAction = "hidden-action";
  table_map_generator_.AddAction(kMergeAction);
  table_map_generator_.AssignActionParameterToField(
      kMergeAction, "param1", "field1");
  table_map_generator_.AddDropPrimitive(kMergeAction);
  EXPECT_CALL(tunnel_optimizer_mock_, Optimize(_, _)).Times(0);
  EXPECT_CALL(tunnel_optimizer_mock_, MergeAndOptimize(_, _, _)).Times(0);

  test_internal_action_->MergeAction(kMergeAction);
  test_internal_action_no_opt->MergeAction(kMergeAction);
  EXPECT_EQ(test_internal_action_->internal_name(),
            test_internal_action_no_opt->internal_name());
  EXPECT_TRUE(ProtoEqual(test_internal_action_->internal_descriptor(),
                         test_internal_action_no_opt->internal_descriptor()));
}

// Verifies multiple action merges with overwritten tunnel properties.
TEST_F(InternalActionTest, TestMergeMultipleTunnelProperties) {
  // The "tunnel1-action" encaps an IPv6 inner header and assigns a field.
  const std::string kTunnel1Action = "tunnel1-action";
  table_map_generator_.AddAction(kTunnel1Action);
  table_map_generator_.AssignActionParameterToField(
      kTunnel1Action, "param1", "field1");
  auto tunnel1_action = FindActionDescriptorOrDie(
      kTunnel1Action, table_map_generator_.generated_map());
  tunnel1_action.mutable_tunnel_properties()->mutable_encap()->
      add_encap_inner_headers(P4_HEADER_IPV6);
  table_map_generator_.ReplaceActionDescriptor(kTunnel1Action, tunnel1_action);

  test_internal_action_->MergeAction(kTunnel1Action);
  EXPECT_TRUE(ProtoEqual(
      tunnel1_action.tunnel_properties(),
      test_internal_action_->internal_descriptor().tunnel_properties()));
  EXPECT_EQ(original_action_.assignments_size() +
            tunnel1_action.assignments_size(),
            test_internal_action_->internal_descriptor().assignments_size());

  // The "tunnel2-action" encaps an IPv4 inner header and assigns a field.
  const std::string kTunnel2Action = "tunnel2-action";
  table_map_generator_.AddAction(kTunnel2Action);
  table_map_generator_.AssignActionParameterToField(
      kTunnel2Action, "param2", "field2");
  auto tunnel2_action = FindActionDescriptorOrDie(
      kTunnel2Action, table_map_generator_.generated_map());
  tunnel2_action.mutable_tunnel_properties()->mutable_encap()->
      add_encap_inner_headers(P4_HEADER_IPV4);
  table_map_generator_.ReplaceActionDescriptor(kTunnel2Action, tunnel2_action);

  // The mock tunnel optimizer output is a basic protobuf merge of the two
  // inputs.
  hal::P4ActionDescriptor expected_internal_action = tunnel1_action;
  expected_internal_action.MergeFrom(tunnel2_action);
  EXPECT_CALL(tunnel_optimizer_mock_, Optimize(_, _)).Times(0);
  EXPECT_CALL(tunnel_optimizer_mock_, MergeAndOptimize(_, _, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<2>(expected_internal_action), Return(true)));

  test_internal_action_->MergeAction(kTunnel2Action);
  const auto& test_descriptor = test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(expected_internal_action, test_descriptor));
}

// Verifies action merge where only the merged action has tunnel properties.
TEST_F(InternalActionTest, TestMergeOneTunnelProperties) {
  // The original_action_ in test_internal_action_ has no tunnel properties.
  // The action below merges an encap tunnel.
  const std::string kEncapAction = "encap-action";
  table_map_generator_.AddAction(kEncapAction);
  table_map_generator_.AssignActionParameterToField(
      kEncapAction, "param2", "field2");
  auto encap_action = FindActionDescriptorOrDie(
      kEncapAction, table_map_generator_.generated_map());
  encap_action.mutable_tunnel_properties()->mutable_encap()->
      add_encap_inner_headers(P4_HEADER_IPV4);
  table_map_generator_.ReplaceActionDescriptor(kEncapAction, encap_action);

  // The mock tunnel optimizer should be used for optimizing only, not to
  // merge the action.
  hal::P4ActionDescriptor expected_optimized_action = original_action_;
  expected_optimized_action.MergeFrom(encap_action);
  EXPECT_CALL(tunnel_optimizer_mock_, MergeAndOptimize(_, _, _)).Times(0);
  EXPECT_CALL(tunnel_optimizer_mock_, Optimize(_, NotNull()))
      .WillOnce(
          DoAll(SetArgPointee<1>(expected_optimized_action), Return(true)));
  test_internal_action_->MergeAction(kEncapAction);
  const auto& test_descriptor = test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(expected_optimized_action, test_descriptor));
}

// Verifies merging a meter color-based condition updates the internal
// descriptor correctly.
TEST_F(InternalActionTest, TestMergeMeterCondition) {
  hal::P4ActionDescriptor::P4MeterColorAction color_action;
  color_action.add_colors(P4_METER_RED);
  color_action.add_ops()->add_primitives(P4_ACTION_OP_CLONE);
  hal::P4ActionDescriptor color_descriptor;
  *(color_descriptor.add_color_actions()) = color_action;
  std::string color_action_text;
  ASSERT_TRUE(PrintProtoToString(color_descriptor, &color_action_text).ok());

  test_internal_action_->MergeMeterCondition(color_action_text);

  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find(kOriginalAction));
  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find("Meter"));
  ASSERT_EQ(1,
            test_internal_action_->internal_descriptor().color_actions_size());
  EXPECT_TRUE(ProtoEqual(
      color_action,
      test_internal_action_->internal_descriptor().color_actions(0)));
  ASSERT_EQ(1, test_internal_action_->internal_descriptor().assignments_size());
  EXPECT_TRUE(ProtoEqual(
      original_action_.assignments(0),
      test_internal_action_->internal_descriptor().assignments(0)));
}

// Verifies a merge of an original action with an internal action that links
// to another internal action.
TEST_F(InternalActionTest, TestMergeActionInternalToInternal) {
  SetUpLinkedActions();
  const auto& iter = table_map_generator_.generated_map().table_map().find(
      kLinkedInternalActionName);
  const hal::P4ActionDescriptor& linked_internal_action =
      iter->second.internal_action();

  // This step merges kActionToInternalName into test_internal_action_.  Since
  // kActionToInternalName already links to another internal action,
  // MergeAction should remove the indirection.
  test_internal_action_->MergeAction(kActionToInternalName);

  // The internal_descriptor in test_internal_action_ should now have
  // the assignment from the original_action_ merged with the primitives
  // from linked_internal_action.
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  ASSERT_EQ(original_action_.assignments_size(),
            internal_descriptor.assignments_size());
  for (int i = 0; i < original_action_.assignments_size(); ++i) {
    EXPECT_TRUE(ProtoEqual(original_action_.assignments(i),
                           internal_descriptor.assignments(i)));
  }
  ASSERT_EQ(linked_internal_action.primitive_ops_size(),
            internal_descriptor.primitive_ops_size());
  for (int i = 0; i < linked_internal_action.primitive_ops_size(); ++i) {
    EXPECT_EQ(linked_internal_action.primitive_ops(i),
              internal_descriptor.primitive_ops(i));
  }

  // The internal_name should be formed from the two merged actions, and
  // internal_descriptor should not redirect.
  EXPECT_NE(std::string::npos,
            test_internal_action_->internal_name().find(kOriginalAction));
  EXPECT_NE(
      std::string::npos,
      test_internal_action_->internal_name().find(kLinkedInternalActionName));
  EXPECT_EQ(0, internal_descriptor.action_redirects_size());
}

// Verifies a merge of an original action with an internal action that redirects
// to multiple internal actions.
TEST_F(InternalActionTest, TestMergeInternalToInternalMultipleRedirects) {
  SetUpLinkedActions();
  hal::P4ActionDescriptor action_to_internal = FindActionDescriptorOrDie(
      kActionToInternalName, table_map_generator_.generated_map());
  auto redirect = action_to_internal.add_action_redirects();
  redirect->add_internal_links()->set_internal_action_name("dummy-action");
  table_map_generator_.ReplaceActionDescriptor(
      kActionToInternalName, action_to_internal);

  // Since InternalAction does not merge multiple action_redirects, the
  // internal_descriptor should be unchanged.
  test_internal_action_->MergeAction(kActionToInternalName);
  EXPECT_TRUE(ProtoEqual(
      original_action_, test_internal_action_->internal_descriptor()));
}

// Verifies a merge of an original action with an internal action that has
// multiple internal links.
TEST_F(InternalActionTest, TestMergeInternalToInternalMultipleLinks) {
  SetUpLinkedActions();
  hal::P4ActionDescriptor action_to_internal = FindActionDescriptorOrDie(
      kActionToInternalName, table_map_generator_.generated_map());
  ASSERT_EQ(1, action_to_internal.action_redirects_size());
  auto redirect = action_to_internal.mutable_action_redirects(0);
  redirect->add_internal_links()->set_internal_action_name("dummy-action");
  table_map_generator_.ReplaceActionDescriptor(
      kActionToInternalName, action_to_internal);

  // Since InternalAction does not merge multiple internal links, the
  // internal_descriptor should be unchanged.
  test_internal_action_->MergeAction(kActionToInternalName);
  EXPECT_TRUE(ProtoEqual(
      original_action_, test_internal_action_->internal_descriptor()));
}

// Tests removal of a single duplicate assignment:
//  field1 = 1;
//  field2 = 1;
//  field1 = 1;  <-- Duplicate to be removed.
TEST_F(InternalActionTest, TestDuplicateAssignment) {
  AddConstantAssignment("field1", 1);
  AddConstantAssignment("field2", 1);
  AddConstantAssignment("field1", 1);
  const hal::P4ActionDescriptor& assignment_action = FindActionDescriptorOrDie(
      kAssignmentAction, table_map_generator_.generated_map());
  MergeAssignmentAction();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  const int original_size = original_action_.assignments_size();
  EXPECT_EQ(original_size + assignment_action.assignments_size() - 1,
            internal_descriptor.assignments_size());
  EXPECT_EQ(1, CountFieldReferences("field1"));
  EXPECT_EQ(1, CountFieldReferences("field2"));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(0),
                         internal_descriptor.assignments(0 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(1),
                         internal_descriptor.assignments(1 + original_size)));
}

// Tests removal of a multiple duplicate assignments:
//  field1 = 1;
//  field2 = 2;
//  field1 = 1;  <-- Duplicate to be removed.
//  field2 = 2;  <-- Duplicate to be removed.
//  field1 = 1;  <-- Duplicate to be removed.
TEST_F(InternalActionTest, TestMultipleDuplicateAssignments) {
  AddConstantAssignment("field1", 1);
  AddConstantAssignment("field2", 2);
  AddConstantAssignment("field1", 1);
  AddConstantAssignment("field2", 2);
  AddConstantAssignment("field1", 1);
  const hal::P4ActionDescriptor& assignment_action = FindActionDescriptorOrDie(
      kAssignmentAction, table_map_generator_.generated_map());
  MergeAssignmentAction();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  const int original_size = original_action_.assignments_size();
  EXPECT_EQ(original_size + assignment_action.assignments_size() - 3,
            internal_descriptor.assignments_size());
  EXPECT_EQ(1, CountFieldReferences("field1"));
  EXPECT_EQ(1, CountFieldReferences("field2"));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(0),
                         internal_descriptor.assignments(0 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(1),
                         internal_descriptor.assignments(1 + original_size)));
}

// Tests removal of duplicate assignments when conflicts are present:
//  field1 = 1;
//  field2 = 2;
//  field1 = 1;  <-- Duplicate to be removed.
//  field1 = 5;  <-- Change in field value conflict - no removal.
//  field3 = 3;
//  field1 = 5;  <-- Duplicate to be removed.
//  field4 = 4;
TEST_F(InternalActionTest, TestDuplicateAssignmentsWithConflict) {
  AddConstantAssignment("field1", 1);
  AddConstantAssignment("field2", 2);
  AddConstantAssignment("field1", 1);
  AddConstantAssignment("field1", 5);
  AddConstantAssignment("field3", 3);
  AddConstantAssignment("field1", 5);
  AddConstantAssignment("field4", 4);
  const hal::P4ActionDescriptor& assignment_action = FindActionDescriptorOrDie(
      kAssignmentAction, table_map_generator_.generated_map());
  MergeAssignmentAction();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  const int original_size = original_action_.assignments_size();
  EXPECT_EQ(original_size + assignment_action.assignments_size() - 2,
            internal_descriptor.assignments_size());
  EXPECT_EQ(2, CountFieldReferences("field1"));
  EXPECT_EQ(1, CountFieldReferences("field2"));
  EXPECT_EQ(1, CountFieldReferences("field3"));
  EXPECT_EQ(1, CountFieldReferences("field4"));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(0),
                         internal_descriptor.assignments(0 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(1),
                         internal_descriptor.assignments(1 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(3),
                         internal_descriptor.assignments(2 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(4),
                         internal_descriptor.assignments(3 + original_size)));
  EXPECT_TRUE(ProtoEqual(assignment_action.assignments(6),
                         internal_descriptor.assignments(4 + original_size)));
}

// Tests a single parameter replacing a metadata field in multiple assignments:
//  meta_field = opt_param;      <-- Optimized away.
//  header_field1 = meta_field;  <-- Becomes header_field1 = opt_param;
//  header_field2 = meta_field;  <-- Becomes header_field2 = opt_param;
TEST_F(InternalActionTest, TestOptimizeOneParam) {
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("header_field1", "meta_field");
  AddFieldAssignment("header_field2", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  EXPECT_FALSE(ProtoEqual(
      unoptimized_action, test_internal_action_->internal_descriptor()));
  EXPECT_EQ(2, CountParameterAssignments("opt_param"));
  EXPECT_EQ(0, CountFieldReferences("meta_field"));
}

// Tests two parameters replacing metadata fields in multiple assignments:
//  meta_field1 = opt_param1;     <-- Optimized away.
//  meta_field2 = opt_param2;     <-- Optimized away.
//  header_field1 = meta_field1;  <-- Becomes header_field1 = opt_param1;
//  header_field2 = meta_field2;  <-- Becomes header_field2 = opt_param2;
TEST_F(InternalActionTest, TestOptimizeMultiParam) {
  AddParameterAssignment("meta_field1", "opt_param1");
  AddParameterAssignment("meta_field2", "opt_param2");
  AddFieldAssignment("header_field1", "meta_field1");
  AddFieldAssignment("header_field2", "meta_field2");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  EXPECT_FALSE(ProtoEqual(
      unoptimized_action, test_internal_action_->internal_descriptor()));
  EXPECT_EQ(1, CountParameterAssignments("opt_param1"));
  EXPECT_EQ(1, CountParameterAssignments("opt_param2"));
  EXPECT_EQ(0, CountFieldReferences("meta_field1"));
  EXPECT_EQ(0, CountFieldReferences("meta_field2"));
}

// Tests a single parameter replacing sliced metadata fields:
//  meta_field = opt_param;      <-- Not optimized away due to slicing.
//  sliced_field1 = meta_field;  <-- Becomes header_field1 = opt_param;
//  sliced_field2 = meta_field;  <-- Becomes header_field2 = opt_param;
TEST_F(InternalActionTest, TestOptimizeParamSliceAssignments) {
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("sliced_field1", "meta_field", 8);   // Slice of 8 bits.
  AddFieldAssignment("sliced_field2", "meta_field", 16);  // Slice of 16 bits.
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  EXPECT_FALSE(ProtoEqual(
      unoptimized_action, test_internal_action_->internal_descriptor()));
  EXPECT_EQ(3, CountParameterAssignments("opt_param"));
  EXPECT_EQ(1, CountFieldReferences("meta_field"));
}

// Tests a single parameter replacing sliced metadata fields and an unsliced
// field:
//  meta_field = opt_param;      <-- Optimized away by one unsliced assignment.
//  sliced_field1 = meta_field;  <-- Becomes header_field1 = opt_param;
//  sliced_field2 = meta_field;  <-- Becomes header_field2 = opt_param;
//  unsliced_field = meta_field;  <-- Becomes unsliced_field = opt_param;
TEST_F(InternalActionTest, TestOptimizeParamSlicedAndUnslicedAssignments) {
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("sliced_field1", "meta_field", 8);   // Slice of 8 bits.
  AddFieldAssignment("sliced_field2", "meta_field", 16);  // Slice of 16 bits.
  AddFieldAssignment("unsliced_field", "meta_field");     // No bit slicing.
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  EXPECT_FALSE(ProtoEqual(
      unoptimized_action, test_internal_action_->internal_descriptor()));
  EXPECT_EQ(3, CountParameterAssignments("opt_param"));
  EXPECT_EQ(0, CountFieldReferences("meta_field"));
}

// Tests limited ability to optimize due to metadata field being reassigned
// a constant value:
//  meta_field = opt_param;
//  header_field1 = meta_field;  <-- Expected to optimize.
//  meta_field = 0;
//  header_field2 = meta_field;  <-- Not expected to optimize.
TEST_F(InternalActionTest, TestOptimizeMetaReassignConstant) {
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("header_field1", "meta_field");
  AddConstantAssignment("meta_field", 0);
  AddFieldAssignment("header_field2", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_EQ(2, CountParameterAssignments("opt_param"));
  EXPECT_EQ(3, CountFieldReferences("meta_field"));
  ASSERT_EQ(unoptimized_action.assignments_size(),
            internal_descriptor.assignments_size());
  for (int i = 0; i < unoptimized_action.assignments_size(); ++i) {
    if (i == 2) continue;  // Assignment 2 was optimized.
    EXPECT_TRUE(ProtoEqual(unoptimized_action.assignments(i),
                           internal_descriptor.assignments(i)));
  }
}

// Tests limited ability to optimize due to metadata field being reassigned
// another source field value:
//  meta_field = opt_param;
//  header_field1 = meta_field;  <-- Expected to optimize.
//  meta_field = meta_field_reassign;
//  header_field2 = meta_field;  <-- Not expected to optimize.
TEST_F(InternalActionTest, TestOptimizeMetaReassignField) {
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("header_field1", "meta_field");
  AddFieldAssignment("meta_field", "meta_field_reassign");
  AddFieldAssignment("header_field2", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_EQ(2, CountParameterAssignments("opt_param"));
  EXPECT_EQ(3, CountFieldReferences("meta_field"));
  ASSERT_EQ(unoptimized_action.assignments_size(),
            internal_descriptor.assignments_size());
  for (int i = 0; i < unoptimized_action.assignments_size(); ++i) {
    if (i == 2) continue;  // Assignment 2 was optimized.
    EXPECT_TRUE(ProtoEqual(unoptimized_action.assignments(i),
                           internal_descriptor.assignments(i)));
  }
}

// Tests limited ability to optimize due to metadata field being assigned a
// parameter later in the assignment sequence:
// another source field value:
//  header_field1 = meta_field;  <-- Not expected to optimize.
//  meta_field = opt_param;      <-- Goes away.
//  header_field2 = meta_field;  <-- Becomes header_field2 = opt_param.
TEST_F(InternalActionTest, TestOptimizeMetaLateAssign) {
  AddFieldAssignment("header_field1", "meta_field");
  AddParameterAssignment("meta_field", "opt_param");
  AddFieldAssignment("header_field2", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_EQ(1, CountParameterAssignments("opt_param"));
  ASSERT_EQ(unoptimized_action.assignments_size() - 1,
            internal_descriptor.assignments_size());
  EXPECT_EQ(1, CountFieldReferences("meta_field"));
  for (int i = 0; i < internal_descriptor.assignments_size() - 1; ++i) {
    EXPECT_TRUE(ProtoEqual(unoptimized_action.assignments(i),
                           internal_descriptor.assignments(i)));
  }
}

// Tests inability to optimize assignments where a field is assigned a
// metadata field that was never set to a parameter value:
//  header_field1 = meta_field;
TEST_F(InternalActionTest, TestOptimizeMetaAssignNone) {
  AddFieldAssignment("header_field1", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(unoptimized_action, internal_descriptor));
}

// Tests inability to optimize assignments where a metadata field is assigned
// a constant instead of an action parameter:
//  meta_field = 0;
//  header_field1 = meta_field;
TEST_F(InternalActionTest, TestOptimizeMetaConstantAssign) {
  AddConstantAssignment("meta_field", 0);
  AddFieldAssignment("header_field1", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(unoptimized_action, internal_descriptor));
}

// Tests inability to optimize assignments where a metadata field is assigned
// another field instead of an action parameter:
//  meta_field = header_field2;
//  header_field1 = meta_field;
TEST_F(InternalActionTest, TestOptimizeMetaFieldAssign) {
  AddFieldAssignment("meta_field", "header_field2");
  AddFieldAssignment("header_field1", "meta_field");
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();
  test_internal_action_->Optimize();

  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(unoptimized_action, internal_descriptor));
}

// Tests removal of NOP when an assignment is present.
TEST_F(InternalActionTest, TestOptimizeNopWithAssignment) {
  table_map_generator_.AddAction(kAssignmentAction);
  table_map_generator_.AddNopPrimitive(kAssignmentAction);
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_FALSE(ProtoEqual(unoptimized_action, internal_descriptor));
  EXPECT_EQ(unoptimized_action.assignments_size(),
            internal_descriptor.assignments_size());
  EXPECT_EQ(0, internal_descriptor.primitive_ops_size());
}

// Tests removal of NOP when a meter action is present.
TEST_F(InternalActionTest, TestOptimizeNopWithMetering) {
  ReplaceOriginalActionWithNop();
  hal::P4ActionDescriptor::P4MeterColorAction color_action;
  color_action.add_colors(P4_METER_RED);
  color_action.add_ops()->add_primitives(P4_ACTION_OP_CLONE);
  hal::P4ActionDescriptor color_descriptor;
  *(color_descriptor.add_color_actions()) = color_action;
  std::string color_action_text;
  ASSERT_TRUE(PrintProtoToString(color_descriptor, &color_action_text).ok());
  test_internal_action_->MergeMeterCondition(color_action_text);
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_FALSE(ProtoEqual(unoptimized_action, internal_descriptor));
  EXPECT_EQ(0, internal_descriptor.primitive_ops_size());
  ASSERT_EQ(1, internal_descriptor.color_actions_size());
  EXPECT_TRUE(ProtoEqual(unoptimized_action.color_actions(0),
                         internal_descriptor.color_actions(0)));
}

// Tests removal of NOP when other action primitives are present.
TEST_F(InternalActionTest, TestOptimizeNopWithDrop) {
  ReplaceOriginalActionWithNop();
  table_map_generator_.AddAction(kAssignmentAction);
  table_map_generator_.AddDropPrimitive(kAssignmentAction);
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_FALSE(ProtoEqual(unoptimized_action, internal_descriptor));
  ASSERT_EQ(1, internal_descriptor.primitive_ops_size());
  EXPECT_EQ(P4_ACTION_OP_DROP, internal_descriptor.primitive_ops(0));
}

// Tests no removal of NOP when no other action primitives, assignments, or
// meter actions are present.
TEST_F(InternalActionTest, TestNoOptimizeNop) {
  ReplaceOriginalActionWithNop();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_TRUE(ProtoEqual(unoptimized_action, internal_descriptor));
}

// Tests removal of all but one NOP when no other action primitives,
// assignments, or meter actions are present.
TEST_F(InternalActionTest, TestOptimizeMultipleNopToOne) {
  ReplaceOriginalActionWithNop();
  table_map_generator_.AddAction(kAssignmentAction);
  table_map_generator_.AddNopPrimitive(kAssignmentAction);
  table_map_generator_.AddNopPrimitive(kAssignmentAction);
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_FALSE(ProtoEqual(unoptimized_action, internal_descriptor));
  EXPECT_EQ(1, internal_descriptor.primitive_ops_size());
}

// Tests removal of all NOPs when assignments are present.
TEST_F(InternalActionTest, TestOptimizeMultipleNopWithAssignments) {
  ReplaceOriginalActionWithNop();
  AddFieldAssignment("meta_field", "header_field2");
  AddFieldAssignment("header_field1", "meta_field");
  table_map_generator_.AddAction(kAssignmentAction);
  table_map_generator_.AddNopPrimitive(kAssignmentAction);
  MergeAssignmentAction();
  const auto unoptimized_action = test_internal_action_->internal_descriptor();

  test_internal_action_->Optimize();
  const auto& internal_descriptor =
      test_internal_action_->internal_descriptor();
  EXPECT_FALSE(ProtoEqual(unoptimized_action, internal_descriptor));
  EXPECT_EQ(unoptimized_action.assignments_size(),
            internal_descriptor.assignments_size());
  EXPECT_EQ(0, internal_descriptor.primitive_ops_size());
}

TEST_F(InternalActionTest, TestWriteToP4PipelineConfig) {
  hal::P4PipelineConfig test_pipeline_config;
  test_internal_action_->WriteToP4PipelineConfig(&test_pipeline_config);
  const auto& iter = test_pipeline_config.table_map().find(
      test_internal_action_->internal_name());
  ASSERT_TRUE(iter != test_pipeline_config.table_map().end());
  EXPECT_TRUE(iter->second.has_internal_action());
  EXPECT_TRUE(ProtoEqual(test_internal_action_->internal_descriptor(),
                         iter->second.internal_action()));
}

TEST_F(InternalActionTest, TestWriteToTableMapGenerator) {
  TableMapGeneratorMock table_map_generator_mock;
  EXPECT_CALL(table_map_generator_mock, AddInternalAction(
      test_internal_action_->internal_name(),
      EqualsProto(test_internal_action_->internal_descriptor())));
  test_internal_action_->WriteToTableMapGenerator(&table_map_generator_mock);
}

}  // namespace p4c_backends
}  // namespace stratum
