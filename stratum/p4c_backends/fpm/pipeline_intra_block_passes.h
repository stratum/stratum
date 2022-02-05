// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file declares a set of classes that help optimize P4Control logic
// into forwarding pipeline stages. Each class is an IR visitor subclass
// that performs a pass through the IR to optimize a specific construct.
// Classes in this file look for sequences of individual statements within
// thet same block to combine into one pipeline stage.  This optimization is
// most often needed where the same statement block refers to multiple stages.
// In this example:
//  {
//    vlan_acl_table.apply();
//    l2_table_1.apply();
//    l2_table_2.apply();
//  }
// The 2 L2 tables can be assigned to the L2 fixed pipeline stage, but earlier
// block level optimization passes don't handle this because the block also
// contains a VLAN ACL stage table.
//
// IR node ownership and the implications for IR transforms are described
// by the file header comments in pipeline_block_passes.h.

#ifndef STRATUM_P4C_BACKENDS_FPM_PIPELINE_INTRA_BLOCK_PASSES_H_
#define STRATUM_P4C_BACKENDS_FPM_PIPELINE_INTRA_BLOCK_PASSES_H_

#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/visitor.h"
#include "stratum/public/proto/p4_annotation.pb.h"

namespace stratum {
namespace p4c_backends {

// A PipelineIntraBlockPass determines whether any statement sequences within
// a P4Control can be combined into a PipelineStageStatement.  It catches
// statement sequences in the same stage that block-level optimization misses.
class PipelineIntraBlockPass {
 public:
  // The constructor requires the p4c ReferenceMap and TypeMap as inputs.
  PipelineIntraBlockPass(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // The OptimizeControl method applies a Transform pass to the input P4Control
  // and replaces statement sequences with PipelineStageStatements where
  // applicable.  If OptimizeControl alters the IR, it returns a pointer to
  // a new copy of the P4Control with the added optimizations.  If no
  // optimization occurs, OptimizeControl returns a pointer to the input
  // control.  The IR node ownership details in pipeline_block_passes.h also
  // apply here.
  const IR::P4Control* OptimizeControl(const IR::P4Control& control);

  // PipelineIntraBlockPass is neither copyable nor movable.
  PipelineIntraBlockPass(const PipelineIntraBlockPass&) = delete;
  PipelineIntraBlockPass& operator=(const PipelineIntraBlockPass&) = delete;

 private:
  // Injected objects from the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
};

// StatementStageInspector's typical usage is as a helper class for
// PipelineIntraBlockPass.  It inspects P4 program statements for pipeline
// stage assignments.  One StatementStageInspector generally processes a
// sequence of statements, such as the individual statements inside an
// IR::BlockStatement.
class StatementStageInspector : public Inspector {
 public:
  // The constructor requires the p4c ReferenceMap and TypeMap as inputs.
  // It also takes an initial pipeline stage value, which may be DEFAULT_STAGE
  // or any other stage that reflects the current pipeline processing state
  // of the caller.
  StatementStageInspector(P4Annotation::PipelineStage initial_stage,
                          P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // Inspect can be called repeatedly for a series of statements in the
  // same block.  It preserves the stage_ value from prior Inspect calls in
  // the internal class state.  Inspect processes the input statement and
  // returns a value from the PipelineStage enum:
  //  - A fixed pipeline stage value, i.e. L2 or L3_LPM, if the statement
  //    applies a table with an annotation for one of these stages.
  //  - DEFAULT_STAGE for statements that apply a table in a programmable
  //    ACL pipeline stage, such as VLAN_ACL.
  //  - DEFAULT_STAGE for statements that apply an unannotated table.
  //  - The current internal stage_ value for statements that do not apply
  //    a table.
  // Inspect always records the return value in the private stage_ in case
  // it needs to be returned for subsequent statements that do not alter
  // the pipeline stage assignment, such as assignments to header fields.
  P4Annotation::PipelineStage Inspect(const IR::StatOrDecl& statement);

  // These methods override the IR::Inspector base class to visit the nodes
  // under the inspected statement.  Per p4c convention, the preorder
  // functions return true to visit deeper nodes in the IR, or false if the
  // StatementStageInspector does not need to visit any deeper nodes.
  bool preorder(const IR::MethodCallExpression* mce) override;
  bool preorder(const IR::PipelineStageStatement* statement) override;
  bool preorder(const IR::TableHitStatement* statement) override;
  bool preorder(const IR::BlockStatement* statement) override;
  bool preorder(const IR::IfStatement* statement) override;
  bool preorder(const IR::SwitchStatement* statement) override;

  // StatementStageInspector is neither copyable nor movable.
  StatementStageInspector(const StatementStageInspector&) = delete;
  StatementStageInspector& operator=(const StatementStageInspector&) = delete;

 private:
  // Injected objects from the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // Records the pipeline stage decision from the most recent statement that
  // applied a table.
  P4Annotation::PipelineStage stage_;

  // For debugging - detects single statements that unexpectedly apply multiple
  // multiple tables.  StatementStageInspector expects earlier compiler passes
  // to simplify these statements.
  bool table_applied_;
};

// IntraBlockOptimizer's typical usage is as a helper class for
// PipelineIntraBlockPass.  It inspects a single P4 program BlockStatement
// and attempts to wrap PipelineStageStatements around statement sequences
// that refer to the same fixed pipeline stage.  One IntraBlockOptimizer
// instance processes a single P4 BlockStatement.  When the input block
// contains nested blocks, IntraBlockOptimizer recursively applies a new
// instance of itself to process the deeper blocks.
class IntraBlockOptimizer : public Transform {
 public:
  // The constructor requires the p4c ReferenceMap and TypeMap as inputs.
  // It also takes an initial pipeline stage value, which should generally
  // be DEFAULT_STAGE, but may be another value when IntraBlockOptimizer
  // recurses through nested blocks.
  IntraBlockOptimizer(P4Annotation::PipelineStage initial_stage,
                      P4::ReferenceMap* ref_map, P4::TypeMap* type_map);

  // The OptimizeBlock method applies a Transform pass to the input block
  // and replaces statement sequences with PipelineStageStatements where
  // applicable.  If OptimizeBlock alters the IR, it returns a pointer to
  // a new copy of the BlockStatement with the added optimizations.  If no
  // optimization occurs, OptimizeBlock returns a pointer to the input
  // block.  The IR node ownership details in pipeline_block_passes.h also
  // apply to OptimizeBlock and the preorder methods below.
  const IR::BlockStatement* OptimizeBlock(
      const IR::BlockStatement& block_statement);

  // The Transform base class calls these methods to visit IR nodes that
  // are involved in optimization decisions.  Transform methods return the
  // input pointer when no transformation occurs; they return a new IR node
  // pointer after performing a transformation.
  const IR::Node* preorder(IR::PipelineStageStatement* statement) override;
  const IR::Node* preorder(IR::BlockStatement* statement) override;
  const IR::Node* preorder(IR::IfStatement* statement) override;
  const IR::Node* preorder(IR::Statement* statement) override;
  const IR::Node* preorder(IR::SwitchStatement* statement) override;

  // IntraBlockOptimizer is neither copyable nor movable.
  IntraBlockOptimizer(const IntraBlockOptimizer&) = delete;
  IntraBlockOptimizer& operator=(const IntraBlockOptimizer&) = delete;

 private:
  // HandleBlockUpdate evaluates the IR Transform results for the input
  // statement and performs any possible statement consolidation.
  void HandleBlockUpdate(const IR::StatOrDecl* statement);

  // Processes decisions by HandleBlockUpdate to combine statements into
  // a PipelineStageStatement.
  void JoinStageStatements();

  // Handles BlockStatements nested deeper in OptimizeBlock's input block.
  const IR::BlockStatement* RecurseBlock(const IR::BlockStatement& block);

  // Injected objects from the constructor.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;

  // These two members maintain the pipeline stage state as IntraBlockOptimizer
  // processes statements within a block.
  P4Annotation::PipelineStage current_pipeline_stage_;
  P4Annotation::PipelineStage next_stage_;

  // This StatementStageInspector makes stage assignment decisions for
  // statements in the input block.
  StatementStageInspector stage_inspector_;

  // These members record information that is used to construct the transformed
  // BlockStatement output.  The new_components_ vector keeps a list of
  // statements that will form the output block, if needed.  It may consist of
  // statements from the original block, new PipelineStageStatements that were
  // formed by combining some of the original statements, or a combination of
  // both.  The statements_in_stage_ vector accumulates statements from the
  // original block that apply the same pipeline stage.  Once HandleBlockUpdate
  // determines that a complete statement sequence exists, it consolidates
  // all of these statements into one PipelineStageStatement.  Upon doing so,
  // components_transformed_ becomes true to indicate that new_components_
  // has at least one new statement.
  IR::IndexedVector<IR::StatOrDecl> new_components_;
  IR::IndexedVector<IR::StatOrDecl> statements_in_stage_;
  bool components_transformed_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_PIPELINE_INTRA_BLOCK_PASSES_H_
