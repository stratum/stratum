// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file implements the Stratum p4c backend's HeaderValidInspector.

#include <utility>

#include "stratum/p4c_backends/fpm/header_valid_inspector.h"

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"

namespace stratum {
namespace p4c_backends {

HeaderValidInspector::HeaderValidInspector(
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      reject_is_valid_(false),
      reject_table_apply_(false),
      valid_headers_in_scope_(nullptr),
      table_header_map_(nullptr) {
}

// Private constructor for recursing.
HeaderValidInspector::HeaderValidInspector(
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
    bool reject_is_valid, bool reject_table_apply,
    ValidHeaderSet* valid_headers_in_scope,
    TableHeaderMap* table_header_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      reject_is_valid_(reject_is_valid),
      reject_table_apply_(reject_table_apply),
      valid_headers_in_scope_(ABSL_DIE_IF_NULL(valid_headers_in_scope)),
      table_header_map_(ABSL_DIE_IF_NULL(table_header_map)) {
}

void HeaderValidInspector::Inspect(
    const IR::Statement& statement, TableMapGenerator* table_mapper) {
  DCHECK(table_mapper != nullptr);
  DCHECK(valid_headers_in_scope_ == nullptr && table_header_map_ == nullptr)
      << "Unexpected HeaderValidInspector recursion via public Inspect";
  auto owned_valid_headers_in_scope = absl::make_unique<ValidHeaderSet>();
  valid_headers_in_scope_ = owned_valid_headers_in_scope.get();
  auto owned_table_header_map = absl::make_unique<TableHeaderMap>();
  table_header_map_ = owned_table_header_map.get();
  {
    absl::LeakCheckDisabler disable_ir_statement_leak_checks;
    statement.apply(*this);  // Inspects statement's child nodes.
  }
  for (const auto& iter : *table_header_map_) {
    if (iter.second.empty()) continue;
    table_mapper->SetTableValidHeaders(iter.first, iter.second);
  }
}

bool HeaderValidInspector::preorder(const IR::IfStatement* statement) {
  const std::string valid_header_name = FindValidHeaderCheck(
      statement->condition->to<IR::MethodCallExpression>());

  // If the IfStatement condition is not a simple header validity check,
  // visit deeper nodes to make sure a validity check is not part of a more
  // complex conditional expression.
  if (valid_header_name.empty()) {
    bool old_reject = reject_is_valid_;
    reject_is_valid_ = true;
    visit(statement->condition);
    reject_is_valid_ = old_reject;
    return true;
  }

  auto set_ret = valid_headers_in_scope_->insert(valid_header_name);
  if (!set_ret.second) {
    ::error("Backend: a valid header condition is already in effect for "
            "%s in %s", valid_header_name.c_str(), statement);
    return false;
  }

  // The first Recurse parameter indicates whether to accept table.apply().
  // A table apply is always OK in the true block because the input statement
  // has a header-valid condition.  Table apply usage in the false block
  // depends on whether the input statement is nested inside any header-valid
  // conditions from ancestor nodes.
  Recurse(false, statement->ifTrue);
  valid_headers_in_scope_->erase(set_ret.first);
  Recurse(valid_headers_in_scope_->empty(), statement->ifFalse);

  return false;
}

// A MethodCallExpression seen here could be one of two things:
//  1) An unconditional table apply.
//  2) A header valid condition somewhere other than a simple IfStatement
//     condition.
bool HeaderValidInspector::preorder(
    const IR::MethodCallExpression* expression) {
  const P4::MethodInstance* instance =
      P4::MethodInstance::resolve(expression, ref_map_, type_map_);
  if (instance->isApply()) {
    const auto apply = instance->to<P4::ApplyMethod>();
    if (apply->isTableApply()) {
      ProcessValidTableHeaders(*apply->object->to<IR::P4Table>());
      return false;
    }
  }

  if (!reject_is_valid_) return true;
  const std::string valid_header_name = FindValidHeaderCheck(expression);
  if (!valid_header_name.empty()) {
    ::error("Backend: Unsupported use of %s", expression);
    return false;
  }

  return true;
}

// This preorder rejects any header.isValid() expressions on the right side
// of an assignment.
bool HeaderValidInspector::preorder(const IR::AssignmentStatement* statement) {
  bool old_reject = reject_is_valid_;
  reject_is_valid_ = true;
  visit(statement->right);
  reject_is_valid_ = old_reject;

  return false;
}

// The TableHitStatement has already done the work to figure out the
// applied table.
bool HeaderValidInspector::preorder(const IR::TableHitStatement* statement) {
  ProcessValidTableHeaders(*statement->p4_table);
  return false;
}

void HeaderValidInspector::Recurse(
    bool reject_table_apply, const IR::Statement* statement) {
  if (statement == nullptr) return;
  HeaderValidInspector recurse_inspector(
      ref_map_, type_map_, reject_is_valid_, reject_table_apply,
      valid_headers_in_scope_, table_header_map_);
  statement->apply(recurse_inspector);
}

void HeaderValidInspector::ProcessValidTableHeaders(
    const IR::P4Table& p4_table) {
  const std::string table_name = p4_table.externalName().c_str();

  if (reject_table_apply_) {
    ::error("Backend: Apply of table %s must follow a valid header "
            "condition", p4_table);
    return;
  }

  auto map_ret = table_header_map_->insert(
      std::make_pair(table_name, *valid_headers_in_scope_));
  if (!map_ret.second) {
    // TODO(unknown): The header set found in the map should probably be
    // compared to valid_headers_in_scope_.  If they are equivalent, it
    // could be that the P4 program developer wants to do some variation
    // of this, which should be OK:
    //  if (hdr1.isValid()) {
    //    if (t1.hit) {
    //      t2.apply();
    //      t3.apply();
    //      t4.apply();
    //    } else {
    //      t5.apply();
    //      t3.apply();
    //      t6.apply();
    //    }
    //  }
    ::error("Backend: table %s is reused, possibly with different sets "
            "of valid header conditions", table_name.c_str());
    return;
  }
}

std::string HeaderValidInspector::FindValidHeaderCheck(
    const IR::MethodCallExpression* method_call) {
  if (method_call == nullptr) return "";
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(method_call, ref_map_, type_map_);
  auto built_in = instance->to<P4::BuiltInMethod>();
  if (built_in == nullptr) return "";
  if (!(built_in->name == IR::Type_Header::isValid)) return "";
  FieldNameInspector field_inspector;
  field_inspector.ExtractName(*built_in->appliedTo);

  return field_inspector.field_name();
}

}  // namespace p4c_backends
}  // namespace stratum
