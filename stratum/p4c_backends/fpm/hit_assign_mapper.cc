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

// This file implements the Stratum p4c backend's HitAssignMapper.

#include "stratum/p4c_backends/fpm/hit_assign_mapper.h"

#include "base/logging.h"
#include "absl/debugging/leak_check.h"
#include "p4lang_p4c/frontends/p4/tableApply.h"

namespace stratum {
namespace p4c_backends {

HitAssignMapper::HitAssignMapper(
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {
}

const IR::P4Control* HitAssignMapper::Apply(const IR::P4Control& control) {
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  auto new_body = control.body->apply(*this);
  if (new_body == control.body)
    return &control;

  // Since the control body has transformed and the input control is
  // immutable, the return value is a new P4Control with the transformed
  // body and clones of all other control attributes.
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           new_body->to<IR::BlockStatement>());
}

// The Stratum backend expects the frontend and midend to transform
// IR IfStatements with table hit conditions of the form:
//  if (table.apply().hit) {
//    ...
//  }
// into:
//  bool hit_tmp = table.apply().hit;
//  if (hit_tmp) {
//    ...
//  }
//
// The preorder below takes the IR::AssignmentStatement for "hit_tmp" and
// transforms it into a TableHitStatement.
const IR::Node* HitAssignMapper::preorder(IR::AssignmentStatement* statement) {
  auto table_hit =
      P4::TableApplySolver::isHit(statement->right, ref_map_, type_map_);
  if (table_hit == nullptr) {
    return statement;  // This is not a table.apply().hit assignment.
  }

  auto hit_var_path = statement->left->to<IR::PathExpression>();
  prune();
  if (hit_var_path == nullptr || !hit_var_path->type->is<IR::Type_Boolean>()) {
    ::error("Backend: Expected PathExpression of Type_Boolean for "
            "assignment to table hit variable %s", statement->left);
    return statement;
  }

  auto transformed_hit = new IR::TableHitStatement(
      statement->srcInfo, hit_var_path->path->name.toString(),
      table_hit->externalName(), table_hit);
  return transformed_hit;
}

// This preorder catches any table apply+hit that appears in an unexpected
// expression.  For example, if an apply+hit appears directly in an IfStatement
// condition (despite the expected frontend transform), then previous passes
// may have run an unexpected transform series.  The Stratum backend doesn't
// want these transformations because they can introduce other temporary
// tables and actions that obscure and complicate the control flow.
const IR::Node* HitAssignMapper::preorder(IR::Expression* expression) {
  auto table_hit = P4::TableApplySolver::isHit(expression, ref_map_, type_map_);
  if (table_hit != nullptr) {
    ::error("Backend: Unexpected table hit condition in expression %s.  Check "
            "for incompatible frontend or midend transformations.", expression);
  }

  return expression;
}

const IR::P4Control* HitAssignMapper::RunPreTestTransform(
    const IR::P4Control& control,
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map) {
  HitAssignMapper hit_mapper(ref_map, type_map);
  return hit_mapper.Apply(control);
}

}  // namespace p4c_backends
}  // namespace stratum
