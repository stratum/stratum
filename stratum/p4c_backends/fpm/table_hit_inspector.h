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

// The TableHitInspector is a p4c Inspector subclass that visits the IR node
// hierarchy surrounding a table hit or miss.  It looks for statement sequences
// that the Stratum switch stack is unable to support.  These include:
//  - A table apply can be conditional on the outcome of a previous table
//    apply if and only if the previous apply was a miss.  TableHitInspector
//    allows this sequence:
//      if (!a.apply().hit) {
//        b.apply();
//      }
//    It prohibits this sequence:
//      if (a.apply().hit) {
//        b.apply();
//      }
//    It also forbids out-of-order evaluation of table hit status, such as:
//      bool a_hit = a.apply().hit;
//      bool b_hit = b.apply().hit;
//      if (!a_hit) c.apply();
//    Note that due to p4c frontend transformations of the P4 program logic,
//    this statement:
//      if (!a.apply().hit && !b.apply().hit) do-something;
//    may transform into something that resembles the sequence above to the
//    Hercles backend.
//  - Meter-based conditions can only appear following a table hit, and they
//    must not be subject to any other conditions.  In other words,
//    TableHitInspector must be able to unambiguously associate the meter
//    condition with a specific table hit or action_run.  TableHitInspector
//    allows this sequence:
//      if (a.apply().hit) {
//        if (color == RED) drop();
//      }
//    And it allows this:
//      switch (a.apply().action_run) {
//        action_name: {
//          if (color == RED) drop();
//        }
//      }
//    It does not allow:
//      if (a.apply().hit) {
//        if (some-other-condition) {
//          // Meter condition subject to other conditions.
//          if (color == RED) drop();
//        }
//      }
//    It also does not allow:
//      if (!a.apply().hit) {
//        if (color == RED) drop();  // Meter condition after table miss.
//      }
//    Nor does it allow:
//      a.apply();
//      if (color == RED) drop();  // Ambiguous table hit or miss.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_HIT_INSPECTOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_HIT_INSPECTOR_H_

#include <set>
#include <string>

#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"
#include "p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"

namespace stratum {
namespace p4c_backends {

// A TableHitInspector inspects one IR::BlockStatement.  It usually begins
// an inspection with the "body" statement at the top level of a P4Control.
// As it inspects the IR nodes in a given block, TableHitInspector may
// create recursive instances of itself to inspect deeper blocks in the IR.
class TableHitInspector : public Inspector {
 public:
  // The constructor supports the following input flag combinations:
  //  table_hit  table_miss  Description
  //    false       false    The BlockStatement to be inspected is not
  //                         subject to any prior hit or miss outcomes.
  //    false       true     The BlockStatement to be inspected is acting
  //                         on a prior table miss condition.
  //    true        false    The BlockStatement to be inspected is acting
  //                         on a prior table hit condition.
  //    true        true     Invalid state.
  // The ref_map and type_map parameters come from the p4c midend output.
  TableHitInspector(bool table_hit, bool table_miss,
                    P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  ~TableHitInspector() override {}

  // The Inspect method inspects all IR nodes under the input statement to
  // enforce the limitations as described by the file header comments.  If
  // it detects an unsupported apply sequence, it reports a P4 program error
  // using p4c's ErrorReporter.  Callers can detect whether a statement is
  // invalid by querying the ErrorReporter for a non-zero error count.  Inspect
  // operates on exactly one input statement.  It does not support being called
  // repeatedly with multiple statements.  Inspect returns true if at least one
  // table was successfully applied by the input statement.
  bool Inspect(const IR::Statement& statement);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::Statement.  Per p4c convention, the preorder
  // methods return true to visit deeper nodes in the IR, or false if the
  // Inspector does not need to visit any deeper nodes.
  bool preorder(const IR::TableHitStatement* statement) override;
  bool preorder(const IR::IfStatement* statement) override;
  bool preorder(const IR::MeterColorStatement* statement) override;
  bool preorder(const IR::MethodCallExpression* expression) override;
  bool preorder(const IR::SwitchStatement* statement) override;
  void postorder(const IR::IfStatement* statement) override;

  // TableHitInspector is neither copyable nor movable.
  TableHitInspector(const TableHitInspector&) = delete;
  TableHitInspector& operator=(const TableHitInspector&) = delete;

 private:
  // Creates a TableHitInspector to recursively process the input statement,
  // returning true if the statement or any of its underlying nodes applies a
  // P4 table.  The table_hit flag is true if the statement to be processed
  // is in the scope of a table hit, or false if in the scope of a table miss.
  // Recursion only occurs after a hit/miss decision.
  bool RecurseInspect(const IR::Statement& statement, bool table_hit);

  // Runs the Inspector::apply method to visit the given statement, returning
  // true if the statement or any of its underlying nodes applies a P4 table.
  bool ApplyVisitor(const IR::Statement& statement);

  // Updates the active_hit_var_ and stale_hit_vars_ members when a temporary
  // hit variable goes in or out of scope.  The new_hit_var input names
  // a hit variable entering scope.  It may be empty to take the current
  // active_hit_var_ out of scope.
  void UpdateHitVars(const std::string& new_hit_var);

  // Evaluates whether a statement that applies a table is valid in
  // the current context.
  bool IsTableApplyValid();

  // Copies of constructor inputs.
  const bool table_hit_;
  const bool table_miss_;

  // Additional injected members.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  int if_depth_;        // Depth of nested IfStatements.
  bool table_applied_;  // True when this instance sees at least one apply.

  // When the compiler front/mid ends encounter a table hit inside an if
  // statement condition, they deconstruct it into a temporary variable
  // assignment.  The active_hit_var_ remembers the most recent table hit
  // status recorded to a temporary variable.  The stale_hit_vars_ set
  // remembers previous hit variables that have gone out of scope.
  std::string active_hit_var_;
  std::set<std::string> stale_hit_vars_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TABLE_HIT_INSPECTOR_H_
