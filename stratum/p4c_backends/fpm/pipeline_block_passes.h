// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares a set of classes that help optimize P4Control logic
// into forwarding pipeline stages. Each class is an IR visitor subclass
// that performs a pass through the IR to optimize a specific construct.
// Classes in this file do block-level optimization by assigning entire
// blocks to pipeline stages when every statement within the block applies
// to the same stage.
//
// General note about IR node ownership:
// In p4c, Transform passes often return pointers to IR nodes.  When no
// transform occurs, the pointer refers to the input node.  When a transform
// occurs, the pointer is a new node containing the transformed data.  A
// transformed node may have child nodes that are also transformed, it may
// have children that were pre-existing children of the input node, or it
// may have a combination of pre-existing and new child nodes.  In theory,
// the IR should be the ultimate owner of all of these nodes, but because
// p4c developers prefer to reclaim unused memory via a garbage collector,
// IR nodes have no real ownership strategy.

#ifndef STRATUM_P4C_BACKENDS_FPM_PIPELINE_BLOCK_PASSES_H_
#define STRATUM_P4C_BACKENDS_FPM_PIPELINE_BLOCK_PASSES_H_

#include <set>
#include <vector>

#include "stratum/public/proto/p4_annotation.pb.h"
#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/visitor.h"

namespace stratum {
namespace p4c_backends {

// The FixedTableInspector looks at all the P4Table objects in an IR P4Control
// to determine whether any of them can be mapped to physical tables in
// fixed-function pipeline stages.  It is a preliminary inspection pass to
// determine whether any additional transform passes can potentially produce
// optimized control logic.
class FixedTableInspector : public Inspector {
 public:
  FixedTableInspector();
  ~FixedTableInspector() override {}

  // FindFixedTables inspects the input p4_control and returns true if it finds
  // at least one table that can be mapped to a pipeline stage physical table.
  bool FindFixedTables(const IR::P4Control& p4_control);

  // As the Inspector visits each table, this preorder looks at the table
  // annotation to see if it identifies a fixed-function pipeline stage.
  // Per p4c convention, it returns true to visit deeper nodes in the IR, or
  // false if the FixedTableInspector does not need to visit any deeper nodes.
  bool preorder(const IR::P4Table* table) override;

  // FixedTableInspector is neither copyable nor movable.
  FixedTableInspector(const FixedTableInspector&) = delete;
  FixedTableInspector& operator=(const FixedTableInspector&) = delete;

 private:
  // Becomes true when the preorder method sees a table that can use one
  // of the fixed pipeline stages.
  bool has_fixed_table_;
};

// PipelineIfBlockInsertPass does a preliminary transformation to simplify
// PipelineBlockPass.  Given a simple IR::IfStatement similar to:
//
//  if (<condition>)
//    table1.apply();
//
// It wraps an IR::BlockStatement around the IR::MethodCallStatement that
// does the apply, allowing PipelineBlockPass to focus on BlockStatements.
class PipelineIfBlockInsertPass : public Transform {
 public:
  PipelineIfBlockInsertPass() {}

  // InsertBlocks applies the IR transform pass to the input control.  It
  // returns the original control if no transformation occurs.  It returns
  // a transformed control if at least one of the control's statements needs
  // to be transformed.  See file header comments for ownership details.
  const IR::P4Control* InsertBlocks(const IR::P4Control& control);

  // The Transform base class calls these postorder methods to visit specific IR
  // statement nodes.  They return the input pointer when no transformation
  // occurs, or a new statement pointer if they transform either the if clause
  // or else clause into a BlockStatement.  See file header comments for
  // ownership details.  The MeterColorStatement is a Stratum-specific
  // IfStatement subclass.
  const IR::IfStatement* postorder(IR::IfStatement* statement) override;
  const IR::IfStatement* postorder(IR::MeterColorStatement* statement) override;

  // PipelineIfBlockInsertPass is neither copyable nor movable.
  PipelineIfBlockInsertPass(const PipelineIfBlockInsertPass&) = delete;
  PipelineIfBlockInsertPass& operator=(const PipelineIfBlockInsertPass&) =
      delete;

 private:
  // Evaluates the input statement from the "ifTrue" or "ifFalse" member of
  // an IR::IfStatement and transforms it into an IR::BlockStatement when
  // necessary.
  const IR::Statement* ReplaceSingleStatementWithBlock(
      const IR::Statement* statement);

  // Attempts to transform an IR::IfStatement or an IR::MeterColorStatement,
  // returning a new statement if a transform occurs.  Otherwise, the
  // unmodified input statement is returned.
  const IR::IfStatement* TransformMeterColorOrIf(
      const IR::IfStatement* statement);
};

// The PipelineBlockPass does a Transform pass on an IR P4Control.  It looks
// for BlockStatements that can be optimized into one PipelineStageStatement.
// In this example with all of the logical tables mapping to one physical
// table in a fixed pipeline stage:
//
//  if (<condition>) {
//    if (!lpm_table1.apply().hit) {
//      if (!lpm_table2.apply().hit) {
//        lpm_table3.apply()
//      }
//    }
//  } else {
//    lpm_table4.apply();
//  }
//
// The transformed output is equivalent to:
//
//  if (<condition>) {
//    pipeline_stage_statement({lpm_table1, lpm_table2, lpm_table3});
//  } else {
//    pipeline_stage_statement({lpm_table4});
//  }
//
// where "pipeline_stage_statement" is an abstraction of all the table
// operations that can occur based on <condition>.  PipelineBlockPass does
// better optimization when preceded by PipelineIfBlockInsertPass.
class PipelineBlockPass : public Transform {
 public:
  // The constructor requires the p4c ReferenceMap and TypeMap as inputs.
  PipelineBlockPass(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // The OptimizeControl method applies a Transform pass to the input P4Control
  // and replaces BlockStatements with PipelineStageStatements where applicable,
  // as described above.  If Optimize alters the IR, it returns a pointer to
  // a new copy of the P4Control with the added optimizations.  If no
  // optimization occurs, OptimizeControl returns a pointer to the input
  // control.  See file header comments for ownership details.
  const IR::P4Control* OptimizeControl(const IR::P4Control& control);

  // The Transform base class calls these methods to visit IR nodes that
  // are involved in optimization decisions.  Transform methods return the
  // input pointer when no transformation occurs; they return a new IR node
  // pointer after performing a transformation.  See file header comments for
  // ownership details.
  const IR::BlockStatement* preorder(IR::BlockStatement* statement) override;
  const IR::BlockStatement* postorder(IR::BlockStatement* statement) override;
  const IR::Node* preorder(IR::TableHitStatement* statement) override;
  const IR::Node* preorder(IR::MethodCallExpression* mce) override;

  // PipelineBlockPass is neither copyable nor movable.
  PipelineBlockPass(const PipelineBlockPass&) = delete;
  PipelineBlockPass& operator=(const PipelineBlockPass&) = delete;

 private:
  // The OptimizeBlock method is like OptimizeControl, but it applies the
  // Transform pass to the input BlockStatement and returns a modified
  // BlockStatement if optimizations are possible.  The return value follows
  // standard p4c transform conventions.
  const IR::BlockStatement* OptimizeBlock(
      const IR::BlockStatement& block_statement);

  // PushControlBlock and PopControlBlock handle transitions between levels
  // of the block hierarchy.  PopControlBlock decides whether a lower-level
  // block can be optimized into a higher-level block.  It returns DEFAULT_STAGE
  // when it is not ready or able to optimize the current block, or it returns
  // a stage number for blocks to optimize upon return.  AbortBlockOptimization
  // handles cases where PopControlBlock decides optimization is not possible.
  void PushControlBlock();
  P4Annotation::PipelineStage PopControlBlock();
  void AbortBlockOptimization();

  // Injected objects from the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // PipelineBlockPass updates this vector of sets each time it transitions
  // into another level of the P4Control's BlockStatement hierarchy.  The set
  // contains the pipeline stages that are affected at each block level.
  std::vector<std::set<P4Annotation::PipelineStage>> block_stage_stack_;

  // Becomes true whan at least one pipeline stage optimization has occurred.
  bool optimized_;
};

// The PipelineIfElsePass does a Transform pass on an IR P4Control.  It looks
// for IR IfStatements that can be optimized into one PipelineStageStatement.
// It runs after PipelineBlockPass, handling cases where the earlier passes
// have optimized the true block and the false block of an IfStatement,
// but additional optimization of the entire statement is possible.  Given
// this sample output from PipelineBlockPass:
//
//  if (<condition>) {
//    pipeline_stage_statement({lpm_table1, lpm_table2, lpm_table3});
//  } else {
//    pipeline_stage_statement({lpm_table4});
//  }
//
// The PipelineIfElsePass detects when both the true block and the false block
// refer to the same stage, so it can optimize the entire IfSatement to:
//
//  pipeline_stage_statement({lpm_table1, lpm_table2, lpm_table3, lpm_table4});
//
// where the set of tables in "pipeline_stage_statement" is the union of tables
// from the original IfStatement's true block and false block.
class PipelineIfElsePass : public Transform {
 public:
  // The constructor requires the p4c ReferenceMap and TypeMap as inputs.
  PipelineIfElsePass(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // The OptimizeControl method applies a Transform pass to the input P4Control
  // and replaces IR IfStatements with PipelineStageStatements where applicable,
  // as described above.  If Optimize alters the IR, it returns a pointer to
  // a new copy of the P4Control with the added optimizations.  If no
  // optimization occurs, OptimizeControl returns a pointer to the input
  // control.  See file header comments for ownership details.
  const IR::P4Control* OptimizeControl(const IR::P4Control& control);

  // The Transform base class calls these methods to visit IR nodes that
  // are involved in optimization decisions.  Transform methods return the
  // input pointer when no transformation occurs; they return a new IR node
  // pointer after performing a transformation.  See file header comments for
  // ownership details.
  const IR::Node* preorder(IR::IfStatement* statement) override;
  const IR::Node* postorder(IR::IfStatement* statement) override;
  const IR::Node* preorder(IR::PipelineStageStatement* statement) override;
  const IR::Node* preorder(IR::TableHitStatement* statement) override;
  const IR::Node* preorder(IR::MethodCallExpression* mce) override;

  // PipelineIfElsePass is neither copyable nor movable.
  PipelineIfElsePass(const PipelineIfElsePass&) = delete;
  PipelineIfElsePass& operator=(const PipelineIfElsePass&) = delete;

 private:
  // Injected objects from the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // This member keeps track of the pipeline stages referenced by each level
  // of nested IfStatements.
  std::vector<std::set<int>> stage_stack_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_PIPELINE_BLOCK_PASSES_H_
