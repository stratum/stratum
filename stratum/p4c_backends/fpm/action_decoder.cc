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

// This file contains the ActionDecoder class implementation.

#include "stratum/p4c_backends/fpm/action_decoder.h"

#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/expression_inspector.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/method_call_decoder.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/methodInstance.h"

namespace stratum {
namespace p4c_backends {

ActionDecoder::ActionDecoder(TableMapGenerator* table_mapper,
                             P4::ReferenceMap* ref_map, P4::TypeMap* type_map)
    : table_mapper_(ABSL_DIE_IF_NULL(table_mapper)),
      ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {}

void ActionDecoder::ConvertActionBody(
    const std::string& p4_action_name,
    const IR::IndexedVector<IR::StatOrDecl>& body) {
  // The compiler outputs multiple instances of some actions.  This code
  // assumes that repeat appearances are duplicates.  Since BlockStatement
  // recursion can occur, processed_actions_ is updated upon return, and
  // table_mapper_ needs to be aware that it may see the same action
  // added more than once.
  if (processed_actions_.find(p4_action_name) != processed_actions_.end()) {
    VLOG(1) << "Skipping duplicate appearance of " << p4_action_name;
    return;
  }
  table_mapper_->AddAction(p4_action_name);
  ConvertActionBlock(p4_action_name, body);
  processed_actions_.insert(p4_action_name);
}

void ActionDecoder::ConvertActionBlock(
    const std::string& p4_action_name,
    const IR::IndexedVector<IR::StatOrDecl>& block) {
  // This method uses has_body_statements to indicate whether the action has
  // enough substance to be a non-nop.
  bool has_body_statements = false;

  // This loop iterates over all the statements in the action block.
  for (auto statement : block) {
    // TODO: This setting of has_body_statements is overly broad.
    // For example, returns and exits should probably not count.
    has_body_statements = true;
    if (!statement->is<IR::Statement>()) {
      LOG(WARNING) << "Action Body member is not a statement in "
                   << p4_action_name;
      continue;
    } else if (statement->is<IR::BlockStatement>()) {
      auto components = statement->to<IR::BlockStatement>()->components;
      VLOG(1) << "Recursing to process block statement in " << p4_action_name;
      ConvertActionBlock(p4_action_name, components);
      continue;
    } else if (statement->is<IR::ReturnStatement>()) {
      VLOG(1) << "Return statement in " << p4_action_name;
      break;
    } else if (statement->is<IR::ExitStatement>()) {
      VLOG(1) << "Exit statement in " << p4_action_name;
      break;
    } else if (statement->is<IR::AssignmentStatement>()) {
      VLOG(1) << "Assignment statement in " << p4_action_name;
      auto assignment = statement->to<IR::AssignmentStatement>();

      // The IR::AssignmentStatement has expressions for the left-hand side and
      // the right-hand side.  The left side is typically a header field, and
      // the right side is normally an action parameter or a constant.
      const IR::Expression* lhs = assignment->left;

      // This code was lifted from p4c's JSON converter to distinguish
      // AssignmentStatements that modify fields from those that copy
      // entire headers.
      const IR::Type* type = type_map_->getType(lhs, true);
      bool modify_field_op = false;
      if (type->is<IR::Type_StructLike>()) {
        VLOG(4) << "AssignmentStatement copies header";
      } else {
        VLOG(4) << "AssignmentStatement modifies field";
        modify_field_op = true;
      }

      // The FieldNameInspector extracts the name of the left side field and
      // assures it conforms to switch limitations, i.e. no arithmetic
      // expressions.
      FieldNameInspector header_inspector;
      header_inspector.ExtractName(*lhs);
      const std::string lhs_name_string = header_inspector.field_name();

      // The table_mapper_ updates below cover assigning the right-side
      // expression to fields, which can mean assigning a parameter to a field,
      // assigning a constant to a field, or copying headers.
      ExpressionInspector rhs_inspector(ref_map_, type_map_);
      if (rhs_inspector.Inspect(*assignment->right)) {
        if (modify_field_op) {
          table_mapper_->AssignActionSourceValueToField(
              p4_action_name, rhs_inspector.value(), lhs_name_string);
        } else {
          // A header copy also includes the header's valid bit, which implies
          // a possible encap.  This approach can pick up false encaps, such
          // as a copy from an inner header to an outer header.  These could
          // be filtered by looking up the header descriptors and checking
          // whether they are inner or outer headers, but that adds extra
          // complexity here.  The current strategy is to provide the hidden
          // table mapper with more information rather than less, and let it
          // sort out what all potential tunnel actions really mean.
          table_mapper_->AssignHeaderToHeader(
              p4_action_name, rhs_inspector.value(), lhs_name_string);
          hal::P4ActionDescriptor::P4TunnelAction tunnel_op;
          tunnel_op.set_header_name(lhs_name_string);
          tunnel_op.set_header_op(P4_HEADER_COPY_VALID);
          table_mapper_->AddTunnelAction(p4_action_name, tunnel_op);
        }
      }
      if (VLOG_IS_ON(2)) ::dump(assignment);
      continue;
    } else if (statement->is<IR::EmptyStatement>()) {
      VLOG(1) << "Empty statement in "  << p4_action_name;
      continue;
    } else if (statement->is<IR::MethodCallStatement>()) {
      VLOG(1) << "Method statement in "  << p4_action_name;
      ConvertMethodCall(
          *statement->to<IR::MethodCallStatement>(), p4_action_name);
      continue;
    } else {
      LOG(WARNING) << "Unsupported statement type "
                   << statement->node_type_name()
                   << " in action " << p4_action_name;
    }
  }

  if (!has_body_statements)
    table_mapper_->AddNopPrimitive(p4_action_name);
}

void ActionDecoder::ConvertMethodCall(
    const IR::MethodCallStatement& method_call,
    const std::string& p4_action_name) {
  MethodCallDecoder method_call_decoder(ref_map_, type_map_);
  if (method_call_decoder.DecodeStatement(method_call)) {
    // TODO: ActionDecoder currently expects MethodCallDecoder output
    // to be limited to tunnel actions, the drop primitive, or a NOP.  This
    // needs to be generalized as more P4 externs are supported.  This may
    // benefit from expanding table_mapper_'s public API to take the raw
    // method_op() output as an action descriptor update parameter.
    if (method_call_decoder.tunnel_op().header_op() != P4_HEADER_NOP) {
      table_mapper_->AddTunnelAction(
          p4_action_name, method_call_decoder.tunnel_op());
    } else {
      const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
          method_call_decoder.method_op();
      bool method_call_ok = false;
      if (method_op.primitives_size() == 1) {
        if (method_op.primitives(0) == P4_ACTION_OP_DROP) {
          table_mapper_->AddDropPrimitive(p4_action_name);
          method_call_ok = true;
        } else if (method_op.primitives(0) == P4_ACTION_OP_NOP) {
          method_call_ok = true;
        }
      }
      if (!method_call_ok) {
        LOG(WARNING) << "Unsupported method call in P4 action "
                     << p4_action_name << ": " << method_op.ShortDebugString();
      }
    }
  } else {
    LOG(WARNING) << method_call_decoder.error_message()
                 << " in action " << p4_action_name;
  }
}

}  // namespace p4c_backends
}  // namespace stratum
