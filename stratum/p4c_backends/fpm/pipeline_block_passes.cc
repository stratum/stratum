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

// This file contains implementations for pipeline block optimizing passes.

#include "stratum/p4c_backends/fpm/pipeline_block_passes.h"

#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/debugging/leak_check.h"
#include "external/com_github_p4lang_p4c/frontends/p4/methodInstance.h"
#include "external/com_github_p4lang_p4c/frontends/p4/tableApply.h"

namespace stratum {
namespace p4c_backends {

// The implementation of the FixedTableInspector starts here.
FixedTableInspector::FixedTableInspector() : has_fixed_table_(false) {
}

bool FixedTableInspector::FindFixedTables(const IR::P4Control& p4_control) {
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  p4_control.apply(*this);
  return has_fixed_table_;
}

bool FixedTableInspector::preorder(const IR::P4Table* table) {
  has_fixed_table_ |= IsPipelineStageFixed(GetAnnotatedPipelineStage(*table));
  return false;  // The IR nodes beneath the table have no relevant data.
}

// The implementation of the PipelineIfBlockInsertPass starts here.
const IR::P4Control* PipelineIfBlockInsertPass::InsertBlocks(
    const IR::P4Control& control) {
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  auto transformed_body = control.body->apply(*this);
  if (transformed_body == control.body)
    return &control;

  auto transformed_block = transformed_body->to<IR::BlockStatement>();
  DCHECK(transformed_block != nullptr)
      << "Transformed control body output is not an IR::BlockStatement";
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           transformed_block);
}

// Transforms the input statement if either the true block or the false block
// needs to be enclosed in an IR::BlockStatement.
const IR::IfStatement* PipelineIfBlockInsertPass::postorder(
    IR::IfStatement* statement) {
  return TransformMeterColorOrIf(statement);
}

const IR::IfStatement* PipelineIfBlockInsertPass::postorder(
    IR::MeterColorStatement* statement) {
  return TransformMeterColorOrIf(statement);
}

const IR::Statement* PipelineIfBlockInsertPass::ReplaceSingleStatementWithBlock(
    const IR::Statement* statement) {
  if (statement == nullptr)
    return statement;
  if (statement->is<IR::BlockStatement>())
    return statement;

  // Any statement type except MethodCallStatement is OK for subsequent passes
  // without a BlockStatement wrapper.  MethodCallStatements are interesting
  // because the called method may be an apply().  Additional qualifiers
  // could be added to explicitly limit this to applies at the expense of
  // additional complexity below.
  if (!statement->is<IR::MethodCallStatement>())
    return statement;

  auto new_block = new IR::BlockStatement;
  new_block->push_back(statement);
  return new_block;
}

const IR::IfStatement* PipelineIfBlockInsertPass::TransformMeterColorOrIf(
    const IR::IfStatement* statement) {
  auto true_block = ReplaceSingleStatementWithBlock(statement->ifTrue);
  auto false_block = ReplaceSingleStatementWithBlock(statement->ifFalse);
  if (true_block == statement->ifTrue && false_block == statement->ifFalse)
    return statement;

  const auto meter_statement = statement->to<IR::MeterColorStatement>();
  if (meter_statement) {
    return new IR::MeterColorStatement(
        meter_statement->srcInfo, meter_statement->condition, true_block,
        false_block, meter_statement->meter_color_actions);
  }

  return new IR::IfStatement(statement->srcInfo, statement->condition,
                             true_block, false_block);
}

// The implementation of the PipelineBlockPass starts here.
PipelineBlockPass::PipelineBlockPass(P4::ReferenceMap* ref_map,
                                     P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      optimized_(false) {}

const IR::P4Control* PipelineBlockPass::OptimizeControl(
    const IR::P4Control& control) {
  auto optimized_block = OptimizeBlock(*control.body);
  if (optimized_block == control.body)
    return &control;  // Input control was not optimized.

  // Since the control body has changed and the input control is immutable,
  // the return value is a new P4Control with the optimized body and
  // clones of all other control attributes.
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           optimized_block);
}

// The preorder pushes each IR BlockStatement on to a stack.  The real work
// happens as the postorder method pops the block.
const IR::BlockStatement* PipelineBlockPass::preorder(
    IR::BlockStatement* statement) {
  PushControlBlock();
  return statement;
}

// Calls PopControlBlock to do most of the work.  If PopControlBlock finds a
// pipeline stage to optimize, then the input statement needs to be replaced
// by a new PipelineStageStatement, which records the optimized stage but
// otherwise has the same underlying statement components as the original block.
const IR::BlockStatement* PipelineBlockPass::postorder(
    IR::BlockStatement* statement) {
  P4Annotation::PipelineStage optimize_stage = PopControlBlock();
  if (optimize_stage == P4Annotation::DEFAULT_STAGE) {
    return statement;
  }

  optimized_ = true;
  return new IR::PipelineStageStatement(
      statement->annotations, statement->components, optimize_stage);
}

// A TableHitStatement refers to an IR::P4Table which should be annotated
// with pipeline stage.
const IR::Node* PipelineBlockPass::preorder(IR::TableHitStatement* statement) {
  P4Annotation::PipelineStage stage =
      GetAnnotatedPipelineStageOrP4Error(*statement->p4_table);
  block_stage_stack_.back().insert(stage);
  return statement;
}

// Examines IR MethodCallExpressions for table applies.  Upon finding one,
// the block_stage_stack_ is updated to record the current block's usage of
// the applied table's pipeline stage.
const IR::Node* PipelineBlockPass::preorder(IR::MethodCallExpression* mce) {
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(mce, ref_map_, type_map_);
  P4Annotation::PipelineStage stage = P4Annotation::DEFAULT_STAGE;
  if (IsTableApplyInstance(*instance, &stage))
    block_stage_stack_.back().insert(stage);
  return mce;
}

const IR::BlockStatement* PipelineBlockPass::OptimizeBlock(
    const IR::BlockStatement& block_statement) {
  block_stage_stack_.clear();
  optimized_ = false;
  absl::LeakCheckDisabler disable_ir_block_leak_checks;
  auto optimized_node = block_statement.apply(*this);
  if (!optimized_)
    return &block_statement;

  // The apply function for the IR transform returns an IR::Node pointer,
  // which should represent an IR::BlockStatement for the optimized block.
  auto optimized_block = optimized_node->to<IR::BlockStatement>();
  DCHECK(optimized_block != nullptr)
      << "Optimized pipeline output is not an IR::BlockStatement";
  return optimized_block;
}

void PipelineBlockPass::PushControlBlock() {
  block_stage_stack_.push_back({});
}

// Determines whether the entire popped block can be optimized into a
// fixed-function pipeline stage.
P4Annotation::PipelineStage PipelineBlockPass::PopControlBlock() {
  DCHECK(!block_stage_stack_.empty())
      << "Mismatch between P4Control block pushes and pops";
  VLOG(3) << "Popped control block depth " << block_stage_stack_.size()
          << " stage count is " << block_stage_stack_.back().size();
  auto popped_stage_set = block_stage_stack_.back();
  block_stage_stack_.pop_back();

  // Three conditions negatively affect the block-level optimization decision.
  // Condition 1: If the popped stage set is empty, the block contains no
  // applies, so it has no effect on optimization decisions.
  if (popped_stage_set.empty()) {
    return P4Annotation::DEFAULT_STAGE;
  }

  // Condition 2: If the popped stage set refers to more than one stage, it
  // invalidates optimization all the way up the block stack hierarchy.
  // TODO: This approach works well for tor.p4, but it may be too
  // constraining.  For example, if a block applies an L2 table and an L3_LPM
  // table, but nothing else, should it be a candidate for optimization if
  // the hardware does L2 and L3 lookups in adjacent stages?
  if (popped_stage_set.size() > 1) {
    VLOG(2) << "Unable to optimize block with multiple pipeline stages";
    AbortBlockOptimization();
    return P4Annotation::DEFAULT_STAGE;
  }

  // Condition 3: If the popped block needs ACL stages, it cannot be optimized.
  P4Annotation::PipelineStage block_stage = *popped_stage_set.begin();
  if (!IsPipelineStageFixed(block_stage)) {
    VLOG(2) << "Unable to optimize block with non-fixed (ACL) stages";
    AbortBlockOptimization();
    return P4Annotation::DEFAULT_STAGE;
  }

  // The popped block qualifies for optimization at this point,
  // subject to the additional considerations below.
  if (block_stage_stack_.empty()) {
    // The top block can always be optimized if it has met conditions
    // 1 to 3 above.
    VLOG(2) << "Optimize top block";
    return block_stage;
  }

  // There are three distinct cases when the popped block is nested within
  // another block:
  // a) The popped block's stage is the same as the next block up the
  //    stack.  The popped block can be absorbed into the next block
  //    when it is popped, so nothing is done here.
  //    TODO: This could be a problem when the popped block
  //    is a TrueBlock in an IfStatement, and the FalseBlock
  //    subsequently needs a different pipeline stage.  A provisional
  //    optimization of the popped block may be in order here.
  // b) The next block up the stack doesn't refer to any pipeline
  //    stages, so the popped block can be optimized now, although there
  //    is some potential to be optimized again at the higher level.
  // c) The next block up the stack applies a different set of pipeline
  //    stages, so the popped blocked must be optimized here.
  P4Annotation::PipelineStage optimize_stage = P4Annotation::DEFAULT_STAGE;
  if (popped_stage_set == block_stage_stack_.back()) {
    VLOG(2) << "Deferring optimization to higher block";
  } else if (block_stage_stack_.back().empty()) {
    VLOG(2) << "Optimizing stage at depth " << block_stage_stack_.size()
            << " with potential additional optimization at next block";
    optimize_stage = block_stage;
  } else {
    VLOG(2) << "Optimize stage at depth " << block_stage_stack_.size();
    optimize_stage = block_stage;
  }

  return optimize_stage;
}

// Traverses the block hierarchy and updates all stage sets to {DEFAULT_STAGE}.
void PipelineBlockPass::AbortBlockOptimization() {
  for (auto& stage_set : block_stage_stack_) {
    stage_set.clear();
    stage_set.insert(P4Annotation::DEFAULT_STAGE);
  }
}

// The implementation of the PipelineIfElsePass starts here.
PipelineIfElsePass::PipelineIfElsePass(P4::ReferenceMap* ref_map,
                                       P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {}

const IR::P4Control* PipelineIfElsePass::OptimizeControl(
    const IR::P4Control& control) {
  // The stage stack always starts with one entry for control's main level.
  stage_stack_.clear();
  stage_stack_.push_back({});
  absl::LeakCheckDisabler disable_ir_control_leak_checks;
  auto optimized_body = control.body->apply(*this);
  if (optimized_body == control.body)
    return &control;

  auto optimized_block = optimized_body->to<IR::BlockStatement>();
  DCHECK(optimized_block != nullptr)
      << "Optimized pipeline output is not an IR::BlockStatement";
  return new IR::P4Control(control.srcInfo, control.name, control.type,
                           control.constructorParams, control.controlLocals,
                           optimized_block);
}

// The IfStatement preorder pushes an empty stage_stack_ entry to represent
// the statement as its transform begins.
const IR::Node* PipelineIfElsePass::preorder(IR::IfStatement* statement) {
  stage_stack_.push_back({});
  return statement;
}

// The IfStatement postorder completes the IfStatement transform.  It pops
// the statement's stage_stack_ entry and optimizes based on the entry's
// member stages.
const IR::Node* PipelineIfElsePass::postorder(IR::IfStatement* statement) {
  DCHECK_GT(stage_stack_.size(), 0) << "PipelineIfElsePass has no stack to pop";
  std::set<int> popped_stage_set = stage_stack_.back();
  stage_stack_.pop_back();

  // Any stages encountered by the popped statement are added to the new
  // top of the stack.  If the popped stage set is empty, there is nothing
  // to optimize.  If the popped stage set has more than one member, it means
  // no optimization can be done because a) the statement's true block and
  // false block refer to different pipeline stages, or b) at least one of the
  // blocks refers to multiple stages and can't form a PipelineStageStatement.
  stage_stack_.back().insert(popped_stage_set.begin(), popped_stage_set.end());
  if (popped_stage_set.size() != 1)
    return statement;

  // Non-fixed (ACL) stages can't be optimized.
  auto popped_stage = *popped_stage_set.begin();
  if (!IsPipelineStageFixed(
      static_cast<P4Annotation::PipelineStage>(popped_stage)))
    return statement;

  // At this point, the statement can be optimized, so it is wrapped
  // inside a new PipelineStageStatement.
  auto new_statement = new IR::PipelineStageStatement(popped_stage);
  new_statement->push_back(statement);
  return new_statement;
}

// When the transform pass encounters an existing PipelineStageStatement, it
// records the pipeline stage, and all IR nodes under the statement can be
// pruned from the transform because they've already been optimized.
const IR::Node* PipelineIfElsePass::preorder(
    IR::PipelineStageStatement* statement) {
  stage_stack_.back().insert(statement->stage);
  prune();
  return statement;
}

// A TableHitStatement refers to the applied IR::P4Table, which should
// have s stage annotation.
const IR::Node* PipelineIfElsePass::preorder(IR::TableHitStatement* statement) {
  P4Annotation::PipelineStage stage =
      GetAnnotatedPipelineStageOrP4Error(*statement->p4_table);
  stage_stack_.back().insert(stage);
  return statement;
}

// When the transform pass encounters an IR MethodCallExpression, it evaluates
// whether the expression applies a table, and if so, adds the table's stage
// to the current stage_stack_ set.
const IR::Node* PipelineIfElsePass::preorder(IR::MethodCallExpression* mce) {
  P4::MethodInstance* instance =
      P4::MethodInstance::resolve(mce, ref_map_, type_map_);
  P4Annotation::PipelineStage stage = P4Annotation::DEFAULT_STAGE;
  if (IsTableApplyInstance(*instance, &stage))
    stage_stack_.back().insert(stage);
  return mce;
}

}  // namespace p4c_backends
}  // namespace stratum
