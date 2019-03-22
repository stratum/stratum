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

// This file implements the Stratum p4c backend's TableHitInspector.

#include "stratum/p4c_backends/fpm/table_hit_inspector.h"

#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/p4c_backends/fpm/simple_hit_inspector.h"
#include "absl/debugging/leak_check.h"
#include "external/com_github_p4lang_p4c/frontends/p4/tableApply.h"

namespace stratum {
namespace p4c_backends {

TableHitInspector::TableHitInspector(bool table_hit, bool table_miss,
                                     P4::ReferenceMap* ref_map,
                                     P4::TypeMap* type_map)
    : table_hit_(table_hit),
      table_miss_(table_miss),
      ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      if_depth_(0),
      table_applied_(false) {
  DCHECK(!table_hit_ || !table_miss_);
}

bool TableHitInspector::Inspect(const IR::Statement& statement) {
  SimpleHitInspector simple_hit_inspector;
  absl::LeakCheckDisabler disable_ir_statement_leak_checks;
  if (!simple_hit_inspector.Inspect(statement)) {
    LOG(ERROR) << "P4 program has complex table hit expressions - see "
               << "details in p4c error messages";
    return false;
  }

  return ApplyVisitor(statement);
}

// TableHitInspector is only concerned with AssignmentStatements that
// assign the outcome of a table hit to a temporary variable.
bool TableHitInspector::preorder(const IR::TableHitStatement* statement) {
  UpdateHitVars(statement->hit_var_name.c_str());
  if (IsTableApplyValid()) {
    table_applied_ = true;
  } else {
    ::error("Backend: Stratum FPM does not allow %s to be applied in the "
            "scope of another table hit", statement->p4_table);
  }
  return false;  // TableHitStatement child nodes are not interesting.
}

bool TableHitInspector::preorder(const IR::IfStatement* statement) {
  ++if_depth_;
  VLOG(3) << "TableHitInspector IfStatement depth is up to " << if_depth_;
  auto table_hit = P4::TableApplySolver::isHit(
      statement->condition, ref_map_, type_map_);
  DCHECK(table_hit == nullptr)
      << "Unexpected table.apply().hit in IfStatement condition. "
      << "Check for incompatible frontend or midend transformations.";

  const IR::PathExpression* path_expression = nullptr;
  bool not_operator = false;
  if (statement->condition->is<IR::LNot>()) {
    if (statement->condition->to<IR::LNot>()->expr->is<IR::PathExpression>()) {
      path_expression =
          statement->condition->to<IR::LNot>()->expr->to<IR::PathExpression>();
    }
    not_operator = true;
  } else if (statement->condition->is<IR::PathExpression>()) {
    path_expression = statement->condition->to<IR::PathExpression>();
  }

  // A boolean PathExpression in a condition should refer to a temporary
  // hit variable.
  bool visit_deeper = true;
  if (path_expression && path_expression->type->is<IR::Type_Boolean>()) {
    const std::string tmp_var_name = std::string(
        path_expression->path->name.toString());
    bool local_hit = !not_operator;
    if (tmp_var_name == active_hit_var_) {
      bool applied = RecurseInspect(*statement->ifTrue, local_hit);
      if (statement->ifFalse) {
        applied = RecurseInspect(*statement->ifFalse, !local_hit) || applied;
      }

      // Any table apply by one of the recursive inspects takes the current
      // hit variable out of scope.  Code above has visited the ifTrue and
      // ifFalse blocks, so deeper nodes can be pruned.
      if (applied) UpdateHitVars("");
      table_applied_ = table_applied_ || applied;
      visit_deeper = false;
    } else if (stale_hit_vars_.find(tmp_var_name) != stale_hit_vars_.end()) {
      ::error("Backend: P4 program evaluates temporary hit variable %s "
              "in %s out of order with table apply sequence",
              tmp_var_name.c_str(), statement);
    } else {
      // Unknown temporary variables are OK as long as they don't appear
      // while a hit variable is in scope.  One such situation occurs when
      // the frontend transforms returns into conditions based on temporary
      // "hasReturned" flags to preserve a single point of control exit.
      if (!active_hit_var_.empty()) {
        ::error("Backend: Unexpected temporary variable %s in %s",
                tmp_var_name.c_str(), statement);
      }
    }
  }

  return visit_deeper;
}

// Metering operations must occur within a table hit context, and they cannot
// be within other conditions.
bool TableHitInspector::preorder(const IR::MeterColorStatement* statement) {
  if (table_miss_) {
    ::error("Metering action %s cannot be conditional on a table miss",
            statement->condition);
  } else if (!table_hit_) {
    ::error("Metering action %s must occur following a table hit",
            statement->condition);
  } else if (if_depth_) {
    ::error("Metering action %s cannot depend on any condition except "
            "a table hit", statement->condition);
  }
  return false;  // MeterColorStatement child nodes are not interesting.
}

// Looks for standalone table applies, i.e. those that are not part of
// assignments, switches, and other related conditions.  These are typically
// apply statements that do not care about hit or miss status, such as:
//  table1.apply();
//  table2.apply();
bool TableHitInspector::preorder(const IR::MethodCallExpression* expression) {
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(expression, ref_map_, type_map_);
  if (instance->isApply()) {
    UpdateHitVars("");  // Current hit variable goes out of scope.
    if (IsTableApplyValid()) {
      table_applied_ = true;
    } else {
      ::error("Backend: Stratum FPM does not allow %s to be conditional "
              "on some other table hit", expression);
    }
  }
  return false;
}

bool TableHitInspector::preorder(const IR::SwitchStatement* statement) {
  // SwitchCaseDecoder handles everything under this statement type.
  // P4_16 says the switch expression must apply a table, so no expression
  // decoding is necessary.
  UpdateHitVars("");  // Current hit variable goes out of scope.
  if (IsTableApplyValid()) {
    table_applied_ = true;
  } else {
    ::error("Backend: Stratum FPM does not allow %s to be applied in the "
            "scope of another table hit", statement->expression);
  }
  return false;
}

void TableHitInspector::postorder(const IR::IfStatement* statement) {
  --if_depth_;
  VLOG(3) << "TableHitInspector IfStatement depth is down to " << if_depth_;
}

bool TableHitInspector::RecurseInspect(
    const IR::Statement& statement, bool table_hit) {
  TableHitInspector recurse_inspector(
      table_hit, !table_hit, ref_map_, type_map_);
  return recurse_inspector.ApplyVisitor(statement);
}

bool TableHitInspector::ApplyVisitor(const IR::Statement& statement) {
  statement.apply(*this);
  return table_applied_;
}

void TableHitInspector::UpdateHitVars(const std::string& new_hit_var) {
  if (!active_hit_var_.empty()) {
    stale_hit_vars_.insert(active_hit_var_);
  }
  active_hit_var_ = new_hit_var;
}

bool TableHitInspector::IsTableApplyValid() {
  return !table_hit_;
}

}  // namespace p4c_backends
}  // namespace stratum
