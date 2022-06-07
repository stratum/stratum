// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file implements the Stratum p4c backend's HitAssignMapper.

#include "stratum/p4c_backends/fpm/hit_assign_mapper.h"

#include "absl/debugging/leak_check.h"
#include "external/com_github_p4lang_p4c/frontends/p4/tableApply.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace p4c_backends {

HitAssignMapper::HitAssignMapper(P4::ReferenceMap* ref_map,
                                 P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {}

const IR::P4Control* HitAssignMapper::Apply(const IR::P4Control& control) {
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
// TODO(max): the above transformation is no longer performed in newer p4c
// versions. Check if this function can be removed.
const IR::Node* HitAssignMapper::preorder(IR::AssignmentStatement* statement) {
  auto table_hit =
      P4::TableApplySolver::isHit(statement->right, ref_map_, type_map_);
  if (table_hit == nullptr) {
    return statement;  // This is not a table.apply().hit assignment.
  }

  auto hit_var_path = statement->left->to<IR::PathExpression>();
  prune();
  if (hit_var_path == nullptr || !hit_var_path->type->is<IR::Type_Boolean>()) {
    ::error(
        "Backend: Expected PathExpression of Type_Boolean for "
        "assignment to table hit variable %s",
        statement->left);
    return statement;
  }

  auto transformed_hit = new IR::TableHitStatement(
      statement->srcInfo, hit_var_path->path->name.toString(),
      table_hit->externalName(), table_hit);
  return transformed_hit;
}

// This preorder transform checks for nested table hits inside block statements.
// See the IR::AssignmentStatement transform for details.
const IR::Node* HitAssignMapper::preorder(IR::BlockStatement* statement) {
  auto tmp = statement->components.clone();
  tmp->clear();
  bool block_modified = false;

  for (const auto& component : statement->components) {
    // We're only interested in table hits inside if statements.
    if (!component->to<IR::IfStatement>()) {
      tmp->push_back(component);
      continue;
    }
    auto* if_statement = component->to<IR::IfStatement>();

    auto* rv = TransformTableHitIf(if_statement);
    if (rv) {
      tmp->push_back(rv->components[0]);
      tmp->push_back(rv->components[1]);
      block_modified = true;
    } else {
      tmp->push_back(if_statement);
    }
  }

  if (block_modified) statement->components = *tmp;

  return statement;
}

const IR::BlockStatement* HitAssignMapper::TransformTableHitIf(
    const IR::IfStatement* statement) {
  const IR::P4Table* table_hit = nullptr;
  table_hit =
      P4::TableApplySolver::isHit(statement->condition, ref_map_, type_map_);
  if (statement->condition->is<IR::LNot>()) {
    table_hit = P4::TableApplySolver::isHit(
        statement->condition->as<IR::LNot>().expr, ref_map_, type_map_);
  }

  if (!table_hit) {
    return nullptr;
  }

  auto* rv = statement->clone();

  cstring tmp_var_name = ref_map_->newName(table_hit->getName() + "_hit_tmp");
  auto* fake_hit = new IR::TableHitStatement(
      rv->srcInfo, tmp_var_name, table_hit->externalName(), table_hit);
  auto* path = new IR::PathExpression(rv->srcInfo, new IR::Type_Boolean(),
                                      new IR::Path(tmp_var_name));
  if (rv->condition->is<IR::LNot>()) {
    rv->condition = new IR::LNot(path);
  } else {
    rv->condition = path;
  }
  auto* new_block = new IR::BlockStatement(rv->srcInfo);
  new_block->components.push_back(fake_hit);
  new_block->components.push_back(rv);

  return new_block;
}

// This preorder transform checks for nested table hits inside the branches of
// if statements. See the IR::AssignmentStatement transform for details.
const IR::Node* HitAssignMapper::preorder(IR::IfStatement* statement) {
  // Check for table hits in single (non-block) nested if statements.
  if (statement->ifTrue->is<IR::IfStatement>()) {
    auto* if_true =
        TransformTableHitIf(statement->ifTrue->to<IR::IfStatement>());
    if (if_true) statement->ifTrue = if_true;
  }

  if (statement->ifFalse->is<IR::IfStatement>()) {
    auto* if_false =
        TransformTableHitIf(statement->ifFalse->to<IR::IfStatement>());
    if (if_false) statement->ifFalse = if_false;
  }

  return statement;
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
    ::error(
        "Backend: Unexpected table hit condition in expression %s.  Check "
        "for incompatible frontend or midend transformations.",
        expression);
  }

  return expression;
}

const IR::P4Control* HitAssignMapper::RunPreTestTransform(
    const IR::P4Control& control, P4::ReferenceMap* ref_map,
    P4::TypeMap* type_map) {
  HitAssignMapper hit_mapper(ref_map, type_map);
  return hit_mapper.Apply(control);
}

}  // namespace p4c_backends
}  // namespace stratum
