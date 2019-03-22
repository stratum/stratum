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

// The HeaderValidInspector is an IR::Inspector subclass that looks for tables
// that are applied conditionally based on the validity of one or more header
// types.  It updates table descriptors in the P4TableMap to indicate any
// headers that must be valid when the table is applied.
//
// The HeaderValidInspector also enforces these rules regarding valid header
// checks in Stratum P4 programs:
//  - All header.isValid() expressions must be in IfStatement conditions.
//    They cannot appear in other statement types, such as assignments to
//    temporary variables.
//  - A header.isValid() test can only be in a simple expression.  Expressions
//    involving logical operators and multiple validity checks are forbidden.
//  - The same table cannot be applied in multiple places with different
//    sets of valid headers.
//  - In an IfStatement with a valid header condition, the false block must
//    also depend on a valid header condition.  This IfStatement produces
//    a compilation error:
//      if (header1.isValid())
//        table1.apply();
//      else
//        table2.apply();
//    HeaderValidInspector rejects the apply for table2 because it doesn't
//    know how to handle "apply table2 only when header1 is invalid".  Note
//    that HeaderValidInspector accepts:
//      if (header1.isValid())
//        table1.apply();
//      else if (header2.isValid())
//        table2.apply();

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_VALID_INSPECTOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_VALID_INSPECTOR_H_

#include <map>
#include <set>
#include <string>

#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// A HeaderValidInspector inspects one IR::Statement.  It usually begins
// an inspection with the "body" statement at the top level of a P4Control.
// As it inspects the IR nodes in the input statement, HeaderValidInspector may
// create recursive instances of itself to inspect deeper nodes in the IR.
class HeaderValidInspector : public Inspector {
 public:
  // The ref_map and type_map parameters come from the p4c midend output.
  HeaderValidInspector(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  ~HeaderValidInspector() override {}

  // The Inspect method inspects IR nodes under the input statement.  For
  // any IR::IfStatements with a header.isValid() condition, it looks for
  // child nodes that apply tables, and it calls the table_mapper to update
  // table descriptors in the P4PipelineConfig as needed.  If Inspect detects
  // an unsupported combination of header validity checks, as described by
  // the file header comments, it reports a P4 program error using p4c's
  // ErrorReporter.
  virtual void Inspect(const IR::Statement& statement,
                       TableMapGenerator* table_mapper);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::Statement.  Per p4c convention, they return
  // true to visit deeper nodes in the IR, or false if the Inspector does
  // not need to visit any deeper nodes.
  bool preorder(const IR::IfStatement* statement) override;
  bool preorder(const IR::MethodCallExpression* expression) override;
  bool preorder(const IR::AssignmentStatement* statement) override;
  bool preorder(const IR::TableHitStatement* statement) override;

  // HeaderValidInspector is neither copyable nor movable.
  HeaderValidInspector(const HeaderValidInspector&) = delete;
  HeaderValidInspector& operator=(const HeaderValidInspector&) = delete;

 private:
  // These types maintain the internal state of the Inspector.  A ValidHeaderSet
  // contains the names of all P4 headers with valid conditions in the current
  // scope.  For example, in the statement sequence:
  //  table1.apply();
  //  if (header2.isValid()) {
  //    table2.apply();
  //    if (header3.isValid()) {
  //      table3.apply();
  //    }
  //  }
  // The ValidHeaderSet is empty when table1 is applied.  It contains
  // {"header2"} when table2 is applied.  It expands to {"header2", "header3"}
  // before table3 is applied.
  //
  // The TableHeaderMap stores the ValidHeaderSet for each table the Inspector
  // has encountered.  The key is the table name.  After the inspection of
  // the sample statements completes, the TableHeaderMap consists of the
  // following pairs:
  //  {"table1", {}}
  //  {"table2", {"header2"}}
  //  {"table3", {"header2", "header3"}}
  typedef std::set<std::string> ValidHeaderSet;
  typedef std::map<std::string, ValidHeaderSet> TableHeaderMap;

  // This private constructor is for creating recursive instances only.
  HeaderValidInspector(P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
                       bool reject_is_valid, bool reject_table_apply,
                       ValidHeaderSet* valid_headers_in_scope,
                       TableHeaderMap* table_header_map);

  // A HeaderValidInspector uses recursion to use separate scopes to visit
  // the two branches of an IfStatement.
  void Recurse(bool reject_table_apply, const IR::Statement* statement);

  // A HeaderValidInspector calls this method when it visits an IR node that
  // represents a table apply.
  void ProcessValidTableHeaders(const IR::P4Table& p4_table);

  // Inspects the input method_call to see if it is an isValid() expression
  // operating on a header type.  It returns the header name upon finding
  // an acceptable expression.  Otherwise, it returns an empty string.
  std::string FindValidHeaderCheck(const IR::MethodCallExpression* method_call);

  // These members store the injected parameters.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // A preorder method can set this value to forbid the appearance of
  // header.isValid() expressions in deeper nodes.
  bool reject_is_valid_;

  // This flag is true when HeaderValidInspector needs to reject table applies
  // in the false block of an IfStatement.  It records the value passed into
  // the private constructor for recursion.
  const bool reject_table_apply_;

  // These pointers refer to the internal Inspector state, as described for
  // the private typedefs above.  The top-level HeaderValidInspector creates
  // the instances for each inspection and maintains ownership in local
  // variables.  It hands them down to recursive instances to share a common
  // state.
  ValidHeaderSet* valid_headers_in_scope_;
  TableHeaderMap* table_header_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_HEADER_VALID_INSPECTOR_H_
