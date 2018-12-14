// This file implements the Hercules p4c backend's SimpleHitInspector.

#include "platforms/networking/hercules/p4c_backend/switch/simple_hit_inspector.h"

#include "base/logging.h"

namespace google {
namespace hercules {
namespace p4c_backend {

SimpleHitInspector::SimpleHitInspector() : simple_hits_(true) {
}

bool SimpleHitInspector::Inspect(const IR::Statement& statement) {
  DCHECK(temp_hit_vars_.empty())
      << "SimpleHitInspector can only inspect one statement";
  statement.apply(*this);
  return simple_hits_;
}

// Valid table hit assignments should already be transformed to
// TableHitStatements.  This preorder looks for table hit temporaries on
// the right side of other assignments.
bool SimpleHitInspector::preorder(const IR::AssignmentStatement* assignment) {
  HitVarEnforcer enforcer(temp_hit_vars_);
  if (!enforcer.Inspect(*assignment->right, true)) {
    simple_hits_ = false;
    ::error("Backend: Hercules does not allow temporary hit variables "
            "in expressions on the right side of an assignment %s",
            assignment);
    return false;
  }

  return true;
}

// Enforces limits on IfStatement conditions involving hit variables.
bool SimpleHitInspector::preorder(const IR::IfStatement* statement) {
  HitVarEnforcer enforcer(temp_hit_vars_);
  if (!enforcer.Inspect(*statement->condition, false)) {
    simple_hits_ = false;
    ::error("Backend:  Unsupported hit expression in %s condition.",
            statement);
    return false;
  }

  return true;
}

bool SimpleHitInspector::preorder(const IR::TableHitStatement* statement) {
  temp_hit_vars_.insert(statement->hit_var_name.c_str());
  return false;
}

// The HitVarEnforcer implementation starts here.
SimpleHitInspector::HitVarEnforcer::HitVarEnforcer(
    const std::set<std::string>& temp_hit_vars)
    : unsupported_hit_operators_(false),
      hit_vars_count_(0),
      temp_hit_vars_(temp_hit_vars) {
}

bool SimpleHitInspector::HitVarEnforcer::Inspect(
    const IR::Expression& expression, bool assignment_right) {
  expression.apply(*this);
  if (hit_vars_count_ == 0) return true;
  return !(assignment_right || hit_vars_count_ > 1 ||
           unsupported_hit_operators_);
}

// Hit variables are always PathExpressions of Type_Boolean.  All other
// PathExpressions are irrelevant here.  If an expression has an unsupported
// combination of hit variables with other non-hit booleans, it should be
// caught by the unsupported operators preorder below.
bool SimpleHitInspector::HitVarEnforcer::preorder(
    const IR::PathExpression* path_expression) {
  if (!path_expression->type->is<IR::Type_Boolean>()) return true;
  const std::string hit_var_name =
      std::string(path_expression->path->name.toString());
  if (temp_hit_vars_.find(hit_var_name) != temp_hit_vars_.end()) {
    ++hit_vars_count_;
  }

  return false;
}

// IR::LNot is the only operator allowed in hit variable expressions.
bool SimpleHitInspector::HitVarEnforcer::preorder(
    const IR::LNot* not_operator) {
  return true;
}

// This preorder is a catch-all for all operators except IR::LNot.
bool SimpleHitInspector::HitVarEnforcer::preorder(
    const IR::Operation* bad_operator) {
  unsupported_hit_operators_ = true;
  return true;
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
