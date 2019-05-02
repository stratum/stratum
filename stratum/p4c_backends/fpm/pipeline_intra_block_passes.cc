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

// This file contains implementations for pipeline intra-block
// optimizing passes.

#include "stratum/p4c_backends/fpm/pipeline_intra_block_passes.h"

#include "base/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"
#include "p4lang_p4c/frontends/p4/methodInstance.h"
#include "p4lang_p4c/frontends/p4/tableApply.h"

namespace stratum {
namespace p4c_backends {

PipelineIntraBlockPass::PipelineIntraBlockPass(P4::ReferenceMap* ref_map,
                                               P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {}

const IR::P4Control* PipelineIntraBlockPass::OptimizeControl(
    const IR::P4Control& control) {
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  IntraBlockOptimizer statement_pass(
      P4Annotation::DEFAULT_STAGE, ref_map_, type_map_);
  const IR::BlockStatement* optimized_block =
      statement_pass.OptimizeBlock(*control.body);
  if (optimized_block == control.body)
    return &control;
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           optimized_block);
}


// The StatementStageInspector implementation starts here.
StatementStageInspector::StatementStageInspector(
    P4Annotation::PipelineStage initial_stage, P4::ReferenceMap* ref_map,
    P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      stage_(initial_stage),
      table_applied_(false) {}

P4Annotation::PipelineStage StatementStageInspector::Inspect(
    const IR::StatOrDecl& statement) {
  table_applied_ = false;
  statement.apply(*this);
  return stage_;
}

// The stage for any statement that does a method call depends on whether
// the expression applies a table.  For applies, the table annotation
// determines the stage.  For non-applies, the stage_ remains unchanged to
// preserves the stage across assignments and other statements that are part
// of the current pipeline stage.  All of StatementStageInspector's preorder
// methods always return false since there is nothing relevant in deeper nodes.
bool StatementStageInspector::preorder(const IR::MethodCallExpression* mce) {
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(mce, ref_map_, type_map_);
  P4Annotation::PipelineStage stage = P4Annotation::DEFAULT_STAGE;
  if (IsTableApplyInstance(*instance, &stage)) {
    DCHECK(!table_applied_)
        << "Unexpected multiple table applies in one statement";
    if (IsPipelineStageFixed(stage))
      stage_ = stage;
    else
      stage_ = P4Annotation::DEFAULT_STAGE;
    table_applied_ = true;
  }
  return false;
}

// The stage for a PipelineStageStatement is embedded in the statement instance.
bool StatementStageInspector::preorder(
    const IR::PipelineStageStatement* statement) {
  stage_ = static_cast<P4Annotation::PipelineStage>(statement->stage);
  table_applied_ = true;
  return false;
}

bool StatementStageInspector::preorder(const IR::TableHitStatement* statement) {
  P4Annotation::PipelineStage stage =
      GetAnnotatedPipelineStageOrP4Error(*statement->p4_table);
  if (IsPipelineStageFixed(stage))
    stage_ = stage;
  else
    stage_ = P4Annotation::DEFAULT_STAGE;
  table_applied_ = true;
  return false;
}

// These preorder functions protect against statements that should never
// reach this inspector.  All of them could potentially contain multiple
// table applies deeper in the IR statement node hierarchy.
bool StatementStageInspector::preorder(const IR::BlockStatement* statement) {
  stage_ = P4Annotation::DEFAULT_STAGE;
  ::error("Unexpected statement type %s", statement);
  return false;
}

bool StatementStageInspector::preorder(const IR::IfStatement* statement) {
  stage_ = P4Annotation::DEFAULT_STAGE;
  ::error("Unexpected statement type %s", statement);
  return false;
}

bool StatementStageInspector::preorder(const IR::SwitchStatement* statement) {
  stage_ = P4Annotation::DEFAULT_STAGE;
  ::error("Unexpected statement type %s", statement);
  return false;
}


// The implementation of the IntraBlockOptimizer starts here.
IntraBlockOptimizer::IntraBlockOptimizer(
    P4Annotation::PipelineStage initial_stage, P4::ReferenceMap* ref_map,
    P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      current_pipeline_stage_(initial_stage),
      next_stage_(initial_stage),
      stage_inspector_(initial_stage, ref_map, type_map),
      components_transformed_(false) {}

const IR::BlockStatement* IntraBlockOptimizer::OptimizeBlock(
    const IR::BlockStatement& block_statement) {
  DCHECK(new_components_.empty())
      << "IntraBlockOptimizer can only process one IR::BlockStatement";

  // This loop applies the Transform to the block on a statement-by-statement
  // basis, then passes the result to HandleBlockUpdate to deal with the
  // outcome.
  for (auto statement : block_statement.components) {
    next_stage_ = current_pipeline_stage_;
    auto new_statement_node = statement->apply(*this);
    auto new_statement = new_statement_node->to<IR::StatOrDecl>();
    DCHECK(new_statement != nullptr)
        << "Unexpected non-statement IR type in Transform output";
    if (new_statement != statement) {
      components_transformed_ = true;
    }
    HandleBlockUpdate(new_statement);
  }

  // Before returning, JoinStageStatements checks for any lingering
  // statements in the current stage that need to be collapsed.  If any
  // component statements of the input block have been collapsed into one
  // pipeline stage, they need to be incorporated into a new BlockStatement.
  JoinStageStatements();
  if (!components_transformed_)
    return &block_statement;

  return new IR::BlockStatement(block_statement.srcInfo,
                                block_statement.annotations, new_components_);
}

// Any PipelineStageStatement encountered in the transform visit has already
// been optimized by previous passes, so no deeper node visits are done.
const IR::Node* IntraBlockOptimizer::preorder(
    IR::PipelineStageStatement* statement) {
  next_stage_ = stage_inspector_.Inspect(*statement);
  prune();
  return statement;
}

// Previous passes would have turned a BlockStatement into a
// PipelineStageStatement if the entire block could be optimized. A
// possibility exists that this block contains a sequence such as:
//  {
//    stageM.apply();
//    stageN.apply();
//    stageN.apply();
//  }
// For this case, IntraBlockOptimizer recursively invokes another instance
// of itself to attempt consolidation of statement sequences within the
// nested block.
const IR::Node* IntraBlockOptimizer::preorder(IR::BlockStatement* statement) {
  prune();  // RecurseBlock visits deeper nodes in the block as needed.
  return RecurseBlock(*statement);
}

// The IfStatement preorder exists to allow the Transform visits to descend
// into deeper blocks.  Otherwise, the catch-all IR::Statement preorder
// would prune them.
const IR::Node* IntraBlockOptimizer::preorder(IR::IfStatement* statement) {
  return statement;
}

// This preorder handles all statement types without explicit preorders of
// their own.  It inspects the statement to see if it has any impact on
// pipeline stage processing and then prunes to avoid visits to deeper nodes.
const IR::Node* IntraBlockOptimizer::preorder(IR::Statement* statement) {
  next_stage_ = stage_inspector_.Inspect(*statement);
  prune();
  return statement;
}

// SwitchStatements are too complex to handle, so the next stage reverts to
// the DEFAULT_STAGE to force consolidation of any pending statements.
const IR::Node* IntraBlockOptimizer::preorder(IR::SwitchStatement* statement) {
  next_stage_ = P4Annotation::DEFAULT_STAGE;
  prune();
  return statement;
}

// This method evaluates the transform results for each statement.  There
// are three possible outcomes:
//  1) The next_stage_ is now DEFAULT_STAGE, which means the current statement
//     is not eligible for consolidation into a fixed pipeline stage.  Any
//     preceding eligible statements should now be consolidated.
//  2) The next_stage_ is unchanged, which means the current statement is
//     eligible for consolidation with any preceding or subsequent statements
//     in the stage.
//  3) The next stage represents a change from one fixed pipeline stage to
//     another, which means a combination of outcomes (1) and (2).
void IntraBlockOptimizer::HandleBlockUpdate(const IR::StatOrDecl* statement) {
  if (next_stage_ == P4Annotation::DEFAULT_STAGE) {
    JoinStageStatements();
    new_components_.push_back(statement);
  } else if (next_stage_ == current_pipeline_stage_) {
    statements_in_stage_.push_back(statement);
  } else {
    JoinStageStatements();
    statements_in_stage_.push_back(statement);
  }
  current_pipeline_stage_ = next_stage_;
}

// Combines any pending statements from the statements_in_stage_ vector
// into a single PipelineStageStatement.  As a special case, a single pending
// PipelineStageStatement is unchanged.  However, multiple pending
// PipelineStageStatements are combined into a new PipelineStageStatement.
// Subsequent P4Control processing handles this case properly.
void IntraBlockOptimizer::JoinStageStatements() {
  if (statements_in_stage_.empty())
    return;
  VLOG(1) << "Combining " << statements_in_stage_.size() << " in "
          << P4Annotation::PipelineStage_Name(current_pipeline_stage_)
          << " stage";
  if (statements_in_stage_.size() == 1 &&
      statements_in_stage_[0]->to<IR::PipelineStageStatement>()) {
    new_components_.push_back(statements_in_stage_[0]);
  } else {
    auto new_statement = new IR::PipelineStageStatement(
        statements_in_stage_, current_pipeline_stage_);
    new_components_.push_back(new_statement);
    components_transformed_ = true;
  }
  statements_in_stage_.clear();
}

// Constructs a new IntraBlockOptimizer to process a nested BlockStatement,
// taking care to pass the pipeline stage value through the recursion sequence.
const IR::BlockStatement* IntraBlockOptimizer::RecurseBlock(
    const IR::BlockStatement& block) {
  IntraBlockOptimizer recurse_pass(
      current_pipeline_stage_, ref_map_, type_map_);
  const IR::BlockStatement* recurse_block = recurse_pass.OptimizeBlock(block);
  next_stage_ = recurse_pass.current_pipeline_stage_;
  return recurse_block;
}

}  // namespace p4c_backends
}  // namespace stratum
