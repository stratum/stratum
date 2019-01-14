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

// The ControlInspector is a p4c Inspector subclass that visits the node
// hierarchy under an IR P4Control to interpret the control flow in a P4
// program.  The output is a hal::P4Control message that defines the control's
// sequence of applied tables, hit/miss conditions, etc.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_CONTROL_INSPECTOR_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_CONTROL_INSPECTOR_H_

#include <map>
#include <string>
#include <vector>

#include "stratum/hal/lib/p4/p4_control.host.pb.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/p4c_backends/fpm/switch_case_decoder.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "p4lang_p4c/frontends/p4/coreLibrary.h"
#include "p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"

namespace stratum {
namespace p4c_backends {

// A ControlInspector instance operates on one IR::P4Control to generate
// a hal::P4Control output message.  Typical usage is to construct a
// ControlInspector, call the Inspect method with the IR::P4Control of interest,
// and then use the output available from the control() accessor.
class ControlInspector : public Inspector {
 public:
  // The p4_info_manager provides access to the P4Info from previous p4c
  // passes.  The ref_map and type_map parameters are part of the p4c midend
  // output.  The switch_case_decoder assists with handling the logic within
  // IR::SwitchStatements.  The caller retains ownership of all pointers.
  // ControlInspector expects the shared P4ModelNames instance to identify
  // control and extern functions from the P4 architecture model.
  ControlInspector(
      const hal::P4InfoManager* p4_info_manager,
      P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
      SwitchCaseDecoder* switch_case_decoder,
      TableMapGenerator* table_map_generator);
  ~ControlInspector() override {}

  // The Inspect method visits the IR node hierarchy underneath the input
  // P4Control and produces the hal::P4Control message that is available
  // through the control() accessor upon return.  Inspect should only be called
  // once per ControlInspector instance.
  void Inspect(const IR::P4Control& control);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected IR::P4Control.  Per p4c convention, they return true
  // to visit deeper nodes in the IR, or false if the ControlInspector does
  // not need to visit any deeper nodes.
  bool preorder(const IR::IfStatement* if_statement) override;
  bool preorder(const IR::MethodCallExpression* mce) override;
  bool preorder(const IR::AssignmentStatement* assignment) override;
  bool preorder(const IR::TableHitStatement* hit_statement) override;
  bool preorder(const IR::ExitStatement* exit_statement) override;
  bool preorder(const IR::ReturnStatement* return_statement) override;
  bool preorder(const IR::PipelineStageStatement* pipeline_statement) override;
  bool preorder(const IR::SwitchStatement* switch_statement) override;
  bool preorder(const IR::MeterColorStatement* meter_statement) override;

  // Accesses the P4Control decoded by the Inspect method.
  const hal::P4Control& control() const { return control_; }

  // ControlInspector is neither copyable nor movable.
  ControlInspector(const ControlInspector&) = delete;
  ControlInspector& operator=(const ControlInspector&) = delete;

 private:
  // This Inspector subclass visits the IR nodes under an IR
  // PipelineStageStatement to find the affected P4Table objects and add them
  // to the fixed_pipeline message provided to the constructor.
  class PipelineInspector : public Inspector {
   public:
    PipelineInspector(const hal::P4InfoManager& p4_info_manager,
                      hal::FixedPipelineTables* fixed_pipeline);
    ~PipelineInspector() override {}

    // Applies the Inspector base class to generate a list of P4 logical tables
    // that are assigned to the input statement's pipeline stage.
    void GetTableList(const IR::PipelineStageStatement& statement);

    // Determine whether the input node refers to a table; return
    // whether deeper IR nodes are relevant.
    bool preorder(const IR::PathExpression* path_expression) override;
    bool preorder(const IR::TableHitStatement* statement) override;

   private:
    // Allows PipelineInspector instances access to the parent's P4InfoManager.
    const hal::P4InfoManager& p4_info_manager_;

    // GetTableList accumulates tables into this constructor-injected message
    // during node inspection.
    hal::FixedPipelineTables* fixed_pipeline_;
  };

  // Decodes the input IR expression, which represents a condition in an
  // IR::IfStatement node.  The return value is true when a condition is
  // available for the pipeline config output.  It may be false to suppress
  // the output, such as the hit status for a hidden table.
  bool DecodeCondition(const IR::Expression& condition);

  // Determines whether the input ApplyMethod refers to a table, and if so
  // calls AddApplyStatement so that the table appears in an apply statement
  // in the P4Control message output.
  void DecodeApplyTable(const P4::ApplyMethod& apply);

  // Adds an apply statement for table to the P4Control message output.
  void AddApplyStatement(const IR::P4Table& table);

  // Encodes a P4HeaderValidCondition for the input header name.
  void AddValidHeaderCondition(const std::string& header_name);

  // Adds a map entry between a temporary hit variable and the table
  // IR node that produced the hit/miss.
  void AddHitVar(const std::string& hit_var_name, const IR::P4Table* ir_table);

  // These methods manage the addition of statements and nested blocks within
  // the P4Control:
  //  AddStatement - adds a new statement to the current working P4ControlBlock.
  //  StartTrueBlock - starts a new P4ControlBlock for conditions evaluating
  //      to true within an existing if statement.
  //  StartFalseBlock - starts new P4ControlBlock for false conditions;
  //      internally calls EndBlock to end the current block for the related
  //      true condition.
  //  EndBlock - terminates the current block and restores the block scope
  //      to the enclosing block.
  //  EraseLastStatementInBlock - erases a statement that turns out to be a NOP.
  //  GetLastStatementInBlock - supports StartTrueBlock and StartFalseBlock
  //      by finding the last statement in the current working block.
  void AddStatement();
  void StartTrueBlock();
  void StartFalseBlock();
  void EndBlock();
  void EraseLastStatementInBlock();
  hal::P4ControlStatement* GetLastStatementInBlock();

  // Iterates the accumulated metered_tables_ and appends meter actions to
  // the related P4 tabe map action descriptors.
  void AppendMeterActions();

  // Additional injected members.
  const hal::P4InfoManager* p4_info_manager_;
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
  SwitchCaseDecoder* switch_case_decoder_;
  TableMapGenerator* table_map_generator_;

  // These members provide a means for the various preorder methods to record
  // and share progress in constructing the control flow.
  //  control_ - accumulates the control flow information for the input
  //      IR::P4Control.
  //  condition_ - provides a place for separate preorder methods to contribute
  //      fields of the condition under an IR:IfStatement node.
  //  nested_blocks_ - records the block structure of the P4Control logic.
  //      The first entry is always the control "main" block.  Additional
  //      entries are pushed and popped to represent the ifTrue/ifFalse blocks
  //      within IR::IfStatement nodes.
  //  working_block_ - always refers to the last/deepest nested_blocks_ entry.
  //  working_statement_ - points to the most recently added statement in
  //      working_block_.
  //  hit_vars_map_ - when the compiler front/mid ends encounter a table hit
  //      inside an if statement condition, they decompose it into a temporary
  //      variable assignment.  The map correlates the temporary variable name
  //      key with the IR P4Table node in the hit expression.
  //  table_hit_ - name of most recent table hit.
  hal::P4Control control_;
  hal::P4BranchCondition condition_;
  std::vector<hal::P4ControlBlock*> nested_blocks_;
  hal::P4ControlBlock* working_block_;
  hal::P4ControlStatement* working_statement_;
  std::map<std::string, const IR::P4Table*> hit_vars_map_;
  std::string table_hit_;

  // This container associates MeterColorStatements with their applied tables.
  // The map key is the table name.
  std::vector<std::pair<std::string, const IR::MeterColorStatement*>>
      metered_tables_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_CONTROL_INSPECTOR_H_
