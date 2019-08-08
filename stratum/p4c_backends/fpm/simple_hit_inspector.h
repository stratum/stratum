/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// SimpleHitInspector is generally meant for use as a TableHitInspector helper.
// It looks for table hit combinations that are too complex for the Stratum
// switch stack. One example is this statement:
//
//  if (!table1.apply().hit && !table2.apply().hit)
//    do-something-if-neither-table-hits;
//
// The p4c frontend (see sideEffects.h in the third_party code) transforms
// the IfStatement condition above in a way that produces several temporary
// variable assignments, some of which employ the NOT operator in a
// double-negative form.  Stratum rejects these and expects the P4
// programmer to write the above sequence as:
//
//  if (!table1.apply().hit) {
//    if (!table2.apply().hit) {
//      do-something-if-neither-table-hits;
//    }
//  }
//
// SimpleHitInspector rejects any AssignmentStatement sequence that references
// a temporary hit variable on the right-hand side of the statement.  It also
// expects that the p4c frontend has already converted conditional table hits
// into a statement sequence of the form:
//
//  tmp_hit = table.apply().hit;
//  if (tmp_hit)
//    do-something-for-hit;
//
// Note: The fundamental problem with more complex hit conditions and the
// way the p4c frontend transforms them is the ambiguity it creates in the
// meaning of some temporary variables.  The condition "(!tmp_hit_N)" can
// mean that a) the apply of Table N itself was a miss, or b) the outcome
// of at least one table apply prior to table N evaluated to false, so
// table N was never applied.

#ifndef STRATUM_P4C_BACKENDS_FPM_SIMPLE_HIT_INSPECTOR_H_
#define STRATUM_P4C_BACKENDS_FPM_SIMPLE_HIT_INSPECTOR_H_

#include <set>
#include <string>

#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"

namespace stratum {
namespace p4c_backends {

class SimpleHitInspector : public Inspector {
 public:
  SimpleHitInspector();
  ~SimpleHitInspector() override {}

  // The Inspect method inspects all IR nodes under the input statement to
  // enforce Stratum limitations on table-hit expressions.  If Inspect
  // detects an unsupported expression sequence, it reports a P4 program
  // error using p4c's ErrorReporter, and it returns false.  It returns true
  // when no unsupported table-hit expressions exist within the input
  // statement.  Inspect operates on exactly one input statement.  It does
  // not support being called repeatedly with multiple statements.  Inspect
  // typically operates on a P4Control's main "body" statement, but it
  // can also be called to evaluate statements with smaller scope.  (The
  // latter usage is common for unit tests.)  Inspect expects to examine
  // statements in P4 programs that have already undergone the HitAssignMapper
  // transform.
  bool Inspect(const IR::Statement& statement);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::Statement.  Per p4c convention, they return true
  // to visit deeper nodes in the IR, or false if the Inspector does not need
  // to visit any deeper nodes.
  bool preorder(const IR::AssignmentStatement* assignment) override;
  bool preorder(const IR::IfStatement* statement) override;
  bool preorder(const IR::TableHitStatement* statement) override;

  // SimpleHitInspector is neither copyable nor movable.
  SimpleHitInspector(const SimpleHitInspector&) = delete;
  SimpleHitInspector& operator=(const SimpleHitInspector&) = delete;

 private:
  // The HitVarEnforcer is a private helper for SimpleHitInspector.  It
  // inspects expressions for temporary hit variables and enforces Stratum
  // switch stack restrictions.  It rejects expressions with any one of
  // these attributes:
  // - multiple hit variables.
  // - any operator other than LNot when at least one hit variable is present.
  class HitVarEnforcer : public Inspector {
   public:
    // The temp_hit_vars parameter is the set of all temporary hit variables
    // within the caller's P4 program scope.
    explicit HitVarEnforcer(const std::set<std::string>& temp_hit_vars);
    ~HitVarEnforcer() override {}

    // Inspect evaluates the input expression and returns true if it is
    // acceptable to Stratum with respect to any hit variables.  The
    // assignment_right flag distinguishes an expression from the right side
    // of an assignment versus a conditional expression in an IfStatement.
    bool Inspect(const IR::Expression& expression, bool assignment_right);

    // These methods override the IR::Inspector base class to visit the nodes
    // under the inspected IR::Expression.  Per p4c convention, they return true
    // to visit deeper nodes in the IR, or false if the Inspector does not need
    // to visit any deeper nodes.
    bool preorder(const IR::PathExpression* path_expression) override;
    bool preorder(const IR::LNot* not_operator) override;
    bool preorder(const IR::Operation* bad_operator) override;

    // HitVarEnforcer is neither copyable nor movable.
    HitVarEnforcer(const HitVarEnforcer&) = delete;
    HitVarEnforcer& operator=(const HitVarEnforcer&) = delete;

   private:
    // Private members:
    //  unsupported_hit_operators_ - the inspected expression contains
    //      operators other than IR::LNot ("!").
    //  hit_vars_count_ - counts the number of hit variables in the
    //      inspected expression.
    //  temp_hit_vars_ - injected via constructor.
    bool unsupported_hit_operators_;
    int hit_vars_count_;
    const std::set<std::string>& temp_hit_vars_;
  };

  // Injected members.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // The preorder methods use this member to signal the Inspect result.
  bool simple_hits_;

  // This set keeps track of all the temporary variables within the scope
  // ot the input statement.
  std::set<std::string> temp_hit_vars_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_SIMPLE_HIT_INSPECTOR_H_
