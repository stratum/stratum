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

// An ExpressionInspector is a p4c Inspector subclass that visits the node
// hierarchy under various types of IR Expressions.  The primary use is
// to decode expressions on the right-hand side of IR::AssignmentStatements
// that appear within P4Action and P4Control bodies.  It may also be useful
// for decoding expressions in IR::IfStatement conditions.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_EXPRESSION_INSPECTOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_EXPRESSION_INSPECTOR_H_

#include "stratum/hal/lib/p4/p4_table_map.host.pb.h"
#include "stratum/public/proto/p4_table_defs.host.pb.h"
#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"
#include "p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// An ExpressionInspector processes IR::Expressions supported by the p4c
// backend.  It translates them into useful values for P4PipelineConfig output.
// Typical usage is to construct an ExpressionInspector, then call its Inspect
// method to process expressions of interest.  Inspect resets its internal
// state each time it is called, so it can process as many expressions as the
// caller encounters during the ExpressionInspector's scope.  For more complex
// expressions consisting of multiple sub-expressions, an ExpressionInspector
// can create additional "child" instances of itself to process each of the
// sub-expressions.
class ExpressionInspector : public Inspector {
 public:
  // The constructor requires p4c's TypeMap and ReferenceMap.  It does not
  // transfer any ownership.
  ExpressionInspector(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  ~ExpressionInspector() override {}

  // The Inspect method processes expressions that the caller finds on the
  // right side of assignment statements.  If it can successfully process
  // the expression, Inspect returns true, and the caller can access the
  // decoded expression value via the value() accessor.  Inspect returns
  // false to indicate an unsupported or unimplemented expression.  Inspect
  // also reports unsupported expressions as P4 program errors via p4c's
  // ErrorReporter.
  bool Inspect(const IR::Expression& expression);

  // These methods override the IR Inspector base class to visit the nodes
  // under the inspected IR::Expression.  Per p4c convention, the preorder
  // functions return true to visit deeper nodes in the IR, or false if the
  // Inspector does not need to visit any deeper nodes.  The first six
  // preorders inspect expression types that the p4c backend accepts in a
  // P4 program.  The last preorder catches all unsupported types.
  bool preorder(const IR::Member* member) override;
  bool preorder(const IR::PathExpression* path) override;
  bool preorder(const IR::Constant* constant) override;
  bool preorder(const IR::Slice* slice) override;
  bool preorder(const IR::Add* add) override;
  bool preorder(const IR::ArrayIndex* array_index) override;
  bool preorder(const IR::Expression* unsupported) override;

  // Accessor to Inspect method output.
  const P4AssignSourceValue& value() const { return value_; }

  // ExpressionInspector is neither copyable nor movable.
  ExpressionInspector(const ExpressionInspector&) = delete;
  ExpressionInspector& operator=(const ExpressionInspector&) = delete;

 private:
  // These members are injected via the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // The inspected expression's value is decoded into this message.
  P4AssignSourceValue value_;

  // The preorder methods use this flag to indicate whether they successfully
  // decoded the inspected expression value.
  bool value_valid_;

  // Points to the Inspect method input expression for better LOG output.
  const IR::Expression* inspect_expression_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_EXPRESSION_INSPECTOR_H_
