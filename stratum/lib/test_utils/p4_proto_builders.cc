// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/test_utils/p4_proto_builders.h"

namespace stratum {
namespace test_utils {
namespace p4_proto_builders {

hal::P4ControlStatement HitBuilder::Build() {
  hal::P4ControlStatement control_statement;
  // If there is no condition or control block, there is nothing to populate.
  if (condition_.ByteSizeLong() == 0 && control_block_.ByteSizeLong() == 0) {
    return control_statement;
  }
  // Populate the condition.
  hal::P4IfStatement* branch = control_statement.mutable_branch();
  if (condition_.ByteSizeLong() != 0) {
    *branch->mutable_condition()->mutable_hit() = condition_;
    branch->mutable_condition()->set_not_operator(on_hit_ == use_false_);
  }
  // Populate the control block.
  if (control_block_.ByteSizeLong() != 0) {
    if (use_false_) {
      *branch->mutable_false_block() = control_block_;
    } else {
      *branch->mutable_true_block() = control_block_;
    }
  }
  return control_statement;
}

hal::P4ControlStatement IsValidBuilder::Build() {
  hal::P4ControlStatement statement;
  hal::P4IfStatement* branch = statement.mutable_branch();
  branch->mutable_condition()->set_not_operator(use_not_);
  branch->mutable_condition()->mutable_is_valid()->set_header_name(
      P4HeaderType_Name(header_type_));
  branch->mutable_condition()->mutable_is_valid()->set_header_type(
      header_type_);
  if (use_not_) {
    if (invalid_control_block_.ByteSizeLong() != 0) {
      *branch->mutable_true_block() = invalid_control_block_;
    }
    if (valid_control_block_.ByteSizeLong() != 0) {
      *branch->mutable_false_block() = valid_control_block_;
    }
  } else {
    if (invalid_control_block_.ByteSizeLong() != 0) {
      *branch->mutable_false_block() = invalid_control_block_;
    }
    if (valid_control_block_.ByteSizeLong() != 0) {
      *branch->mutable_true_block() = valid_control_block_;
    }
  }
  return statement;
}

hal::P4ControlBlock ApplyNested(std::vector<hal::P4ControlTableRef> tables) {
  hal::P4ControlBlock control_block;
  hal::P4ControlBlock* current_block = &control_block;
  for (auto table_iter = tables.begin(); table_iter != tables.end();
       ++table_iter) {
    *current_block->add_statements()->mutable_apply() = *table_iter;
    if (table_iter + 1 != tables.end()) {
      *current_block->add_statements() =
          HitBuilder().OnMiss(*table_iter).UseFalse().Build();
      current_block = current_block->mutable_statements()
                          ->rbegin()
                          ->mutable_branch()
                          ->mutable_false_block();
    }
  }
  return control_block;
}

// Same as above, but takes in ::p4::config::v1::Table objects.
hal::P4ControlBlock ApplyNested(std::vector<::p4::config::v1::Table> tables,
                                P4Annotation::PipelineStage stage) {
  std::vector<hal::P4ControlTableRef> table_refs;
  table_refs.reserve(tables.size());
  for (const auto& table : tables) {
    table_refs.push_back(
        P4ControlTableRefBuilder(table.preamble()).Stage(stage).Build());
  }
  return ApplyNested(table_refs);
}

}  // namespace p4_proto_builders
}  // namespace test_utils
}  // namespace stratum
