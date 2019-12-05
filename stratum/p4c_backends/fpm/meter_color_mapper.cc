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

// This file implements the Stratum p4c backend's MeterColorMapper.

#include "stratum/p4c_backends/fpm/meter_color_mapper.h"

#include "absl/debugging/leak_check.h"
#include "external/com_github_p4lang_p4c/frontends/p4/tableApply.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/method_call_decoder.h"
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"

namespace stratum {
namespace p4c_backends {

MeterColorMapper::MeterColorMapper(P4::ReferenceMap* ref_map,
                                   P4::TypeMap* type_map,
                                   TableMapGenerator* table_mapper)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      table_mapper_(ABSL_DIE_IF_NULL(table_mapper)) {
  ClearControlState();
}

const IR::P4Control* MeterColorMapper::Apply(const IR::P4Control& control) {
  ClearControlState();
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  auto new_body = control.body->apply(*this);
  if (new_body == control.body) return &control;

  // Since the control body has transformed and the input control is
  // immutable, the return value is a new P4Control with the transformed
  // body and clones of all other control attributes.
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           new_body->to<IR::BlockStatement>());
}

// IR::BlockStatements are acceptable but not interesting to MeterColorMapper.
// No IR node pruning occurs because deeper statements in the block are useful.
const IR::Node* MeterColorMapper::preorder(IR::BlockStatement* statement) {
  return statement;
}

const IR::Node* MeterColorMapper::preorder(IR::IfStatement* statement) {
  if (transforming_if_) {
    ::error(
        "Backend: Stratum FPM does not support nested %s "
        "within a meter color condition",
        statement);
    transforming_if_ = false;
    prune();
    return statement;
  }

  if (!DecodeCondition(*statement)) {
    DCHECK(!transforming_if_);
    return statement;
  }

  transforming_if_ = true;
  color_actions_.Clear();
  visit(statement->ifTrue);
  if (!transforming_if_) {
    prune();
    return statement;
  }
  if (statement->ifFalse) {
    InvertColorConditions();
    visit(statement->ifFalse);
  }

  // Upon successful transform, the IfStatement becomes a MeterColorStatement
  // with color_actions_ stored in text format.
  if (!transforming_if_) return statement;
  transforming_if_ = false;
  std::string color_actions_text;
  bool string_ok = PrintProtoToString(color_actions_, &color_actions_text).ok();
  DCHECK(string_ok) << "Color actions message did not convert to string";
  return new IR::MeterColorStatement(statement->srcInfo, statement->condition,
                                     statement->ifTrue, statement->ifFalse,
                                     color_actions_text);
}

const IR::Node* MeterColorMapper::preorder(IR::MethodCallStatement* statement) {
  VLOG(1) << "MethodCallStatement " << statement->toString();
  if (!transforming_if_) return statement;
  MethodCallDecoder method_call_decoder(ref_map_, type_map_);
  if (!method_call_decoder.DecodeStatement(*statement)) {
    ::error("Backend: %s %s", method_call_decoder.error_message(), statement);
    transforming_if_ = false;
    prune();
    return statement;
  }

  // The MethodCallDecoder allows more statement types than Stratum allows
  // in switch statements, so MeterColorMapper imposes additional restrictions
  // on the output operations.
  const hal::P4ActionDescriptor::P4ActionInstructions& method_op =
      method_call_decoder.method_op();
  if (method_op.primitives_size() != 1 ||
      (method_op.primitives(0) != P4_ACTION_OP_CLONE &&
       method_op.primitives(0) != P4_ACTION_OP_DROP)) {
    transforming_if_ = false;
    ::error(
        "Backend: Stratum FPM only allows clone and drop externs "
        "in meter actions %s",
        statement);
    prune();
    return statement;
  }

  // The color_action message applies the MethodCallDecoder method_op output
  // to all color conditions currently in effect.
  hal::P4ActionDescriptor::P4MeterColorAction* color_action =
      color_actions_.add_color_actions();
  *(color_action->add_ops()) = method_op;
  if (green_condition_) color_action->add_colors(P4_METER_GREEN);
  if (yellow_condition_) color_action->add_colors(P4_METER_YELLOW);
  if (red_condition_) color_action->add_colors(P4_METER_RED);

  prune();
  return statement;
}

// The general IR::Statement preorder catches any statements that the
// MeterColorMapper does not explictly support in other preorder methods.
const IR::Node* MeterColorMapper::preorder(IR::Statement* statement) {
  if (transforming_if_) {
    ::error("Backend: Unexpected %s statement following meter condition",
            statement);
    transforming_if_ = false;
    prune();
  }
  return statement;
}

const IR::P4Control* MeterColorMapper::RunPreTestTransform(
    const IR::P4Control& control, const std::string& color_field_name,
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map) {
  TableMapGenerator table_mapper;
  table_mapper.AddField(color_field_name);
  table_mapper.SetFieldType(color_field_name, P4_FIELD_TYPE_COLOR);
  MeterColorMapper meter_mapper(ref_map, type_map, &table_mapper);
  return meter_mapper.Apply(control);
}

void MeterColorMapper::ClearControlState() {
  condition_equal_ = false;
  green_condition_ = false;
  yellow_condition_ = false;
  red_condition_ = false;
  transforming_if_ = false;
  color_actions_.Clear();
}

bool MeterColorMapper::DecodeCondition(const IR::IfStatement& statement) {
  IfStatementColorInspector if_transform;
  if (!if_transform.CanTransform(statement)) {
    return false;
  }

  const auto& color_field = if_transform.color_field();
  auto iter = table_mapper_->generated_map().table_map().find(color_field);
  if (iter == table_mapper_->generated_map().table_map().end() ||
      !iter->second.has_field_descriptor()) {
    ::error(
        "Backend: Color field operand %s in meter color condition %s is "
        "not a valid mapped field",
        color_field, statement.condition);
    return false;
  }

  if (iter->second.field_descriptor().type() != P4_FIELD_TYPE_COLOR) {
    ::error(
        "Backend: Color field operand %s in meter color condition %s is "
        "type %s, expected P4_FIELD_TYPE_COLOR",
        color_field, statement.condition,
        P4FieldType_Name(iter->second.field_descriptor().type()));
    return false;
  }

  condition_equal_ = !if_transform.negate();
  SetColorConditions(if_transform.color_value());

  return true;
}

void MeterColorMapper::SetColorConditions(const std::string& color_value) {
  green_condition_ = false;
  yellow_condition_ = false;
  red_condition_ = false;
  const auto& p4_model = GetP4ModelNames();

  if (color_value.find(p4_model.color_enum_green()) != std::string::npos)
    green_condition_ = true;
  else if (color_value.find(p4_model.color_enum_yellow()) != std::string::npos)
    yellow_condition_ = true;
  else if (color_value.find(p4_model.color_enum_red()) != std::string::npos)
    red_condition_ = true;
  if (!condition_equal_) InvertColorConditions();

  VLOG(1) << "Color conditions " << (green_condition_ ? "G" : "-") << "/"
          << (yellow_condition_ ? "Y" : "-") << "/"
          << (red_condition_ ? "R" : "-");
}

void MeterColorMapper::InvertColorConditions() {
  green_condition_ = !green_condition_;
  yellow_condition_ = !yellow_condition_;
  red_condition_ = !red_condition_;
}

// IfStatementColorInspector helper implementation starts here.

IfStatementColorInspector::IfStatementColorInspector()
    : equ_found_(false), negate_(false), relational_operators_(0) {}

bool IfStatementColorInspector::CanTransform(const IR::IfStatement& statement) {
  equ_found_ = false;
  negate_ = false;
  relational_operators_ = 0;
  color_value_.clear();
  color_field_.clear();
  bool can_transform = false;
  bool condition_error = false;

  statement.condition->apply(*this);  // Visits deeper nodes under condition.

  // In order to transform to a MeterColorStatement, the input statement
  // needs to have the color_value_ and color_field_ operands.  It also
  // needs a single relation operator testing for equality or inequality.
  if (!color_value_.empty() && !color_field_.empty()) {
    if (equ_found_ && relational_operators_ == 1) {
      VLOG(1) << "This IfStatement needs a MeterColorStatement transform";
      can_transform = true;
    } else {
      condition_error = true;
    }
  } else if (!color_value_.empty() || !color_field_.empty()) {
    condition_error = true;
  }
  if (condition_error) {
    ::error("Backend: Unsupported conditional expression %s for meter color",
            statement.condition);
  }

  return can_transform;
}

bool IfStatementColorInspector::preorder(const IR::Equ* condition) {
  equ_found_ = true;
  negate_ = false;
  ++relational_operators_;
  visit(condition->left);
  visit(condition->right);
  return condition;
}

bool IfStatementColorInspector::preorder(const IR::Neq* condition) {
  equ_found_ = true;
  negate_ = true;
  ++relational_operators_;
  visit(condition->left);
  visit(condition->right);
  return false;
}

bool IfStatementColorInspector::preorder(
    const IR::Operation_Relation* condition) {
  ++relational_operators_;
  return true;
}

bool IfStatementColorInspector::preorder(const IR::Expression* expression) {
  return true;
}

bool IfStatementColorInspector::preorder(const IR::Member* member) {
  if (IsMemberColorEnum(*member)) {
    if (member->expr->is<IR::PathExpression>()) {
      FieldNameInspector field_inspector;
      field_inspector.ExtractName(*member);
      color_field_ = field_inspector.field_name();
      if (color_field_.empty()) {
        ::error(
            "Backend: Color field operand %s in meter color condition is "
            "not a valid field path expression %s",
            color_field_, member->expr);
        return false;
      }
    } else if (member->expr->is<IR::TypeNameExpression>()) {
      color_value_ = std::string(member->member.name);
    } else {
      ::error("Backend: Unexpected enum expression type %s", member->expr);
      return false;
    }
  }

  return true;
}

bool IfStatementColorInspector::IsMemberColorEnum(const IR::Member& member) {
  bool is_color = false;
  auto enum_type = member.type->to<IR::Type_Enum>();
  if (enum_type != nullptr) {
    if (enum_type->name == GetP4ModelNames().color_enum_type()) {
      is_color = true;
    }
  }
  return is_color;
}

}  // namespace p4c_backends
}  // namespace stratum
