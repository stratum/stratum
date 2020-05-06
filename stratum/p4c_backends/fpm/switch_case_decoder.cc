// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file implements the Stratum p4c backend's SwitchCaseDecoder.

#include "stratum/p4c_backends/fpm/switch_case_decoder.h"

#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/p4c_backends/fpm/field_name_inspector.h"
#include "stratum/p4c_backends/fpm/method_call_decoder.h"
#include "external/com_github_p4lang_p4c/frontends/p4/tableApply.h"

namespace stratum {
namespace p4c_backends {

SwitchCaseDecoder::SwitchCaseDecoder(
    const std::map<std::string, std::string>& action_name_map,
    P4::ReferenceMap* ref_map, P4::TypeMap* type_map,
    TableMapGenerator* table_mapper)
    : action_name_map_(action_name_map),
      ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)),
      table_mapper_(ABSL_DIE_IF_NULL(table_mapper)) {
  ClearDecodeState();
}

// For mock use only - sets some private members to nullptr assuming they'll
// never be referenced by mock overrides.
SwitchCaseDecoder::SwitchCaseDecoder(
    const std::map<std::string, std::string>& action_name_map)
    : action_name_map_(action_name_map),
      ref_map_(nullptr),
      type_map_(nullptr),
      table_mapper_(nullptr) {
  ClearDecodeState();
}

void SwitchCaseDecoder::Decode(const IR::SwitchStatement& switch_statement) {
  ClearDecodeState();

  // According to the P4_16 spec, the switch statement's expression must be
  // a table apply result.
  applied_table_ = P4::TableApplySolver::isActionRun(
      switch_statement.expression, ref_map_, type_map_);
  if (!applied_table_) {
    ::error("Backend: Unexpected switch statement expression %s. "
            "Expession must be table.apply().action_run",
            switch_statement.expression);
    return;
  }

  for (auto switch_case : switch_statement.cases) {
    ClearCaseState();
    if (switch_case->label->is<IR::DefaultExpression>()) {
      ::error("Backend: Stratum FPM does not allow default cases in "
              "P4 switch statement %s", switch_case);
      continue;
    }
    auto case_label = switch_case->label->to<IR::PathExpression>();
    DCHECK(case_label != nullptr) << "Expected p4c frontend/midend to reject "
                                  << "invalid switch case label type";
    if (!case_label->type->is<IR::Type_Action>()) {
      ::error("Backend: Expected IR::Type_Action for switch case label - "
              "found %s", case_label->type);
      return;
    }

    action_ = std::string(case_label->path->name.name);
    auto iter = action_name_map_.find(action_);
    if (iter == action_name_map_.end()) {
      // TODO(unknown): This might be a compiler bug.
      ::error("Backend: Internal action name %s is not an externally "
              "visible action", action_);
      return;
    }
    action_ = iter->second;  // The external name is now in action_.

    if (VLOG_IS_ON(2)) ::dump(switch_case);
    if (switch_case->statement != nullptr) {
      switch_case->statement->apply(*this);
    } else {
      // Lack of a statement after the case indicates fall-through,
      // which is unsupported by Stratum.
      ::error("Backend: Switch case %s has no statements", switch_case);
    }
  }

  // When all switch cases decode without errors, the color-based actions are
  // written to the P4PipelineConfig via table_mapper_.
  // TODO(unknown): This should be converted to use IndirectActions.
  if (::errorCount() == 0) {
    for (const auto& mapper_pair : color_actions_) {
      table_mapper_->AddMeterColorActionsFromString(
          mapper_pair.first, mapper_pair.second);
    }
  }
}

// IR::BlockStatements are acceptable but not interesting to SwitchCaseDecoder.
// The return value is true because statements in the block are interesting.
bool SwitchCaseDecoder::preorder(const IR::BlockStatement* statement) {
  VLOG(1) << "BlockStatement in switch case";
  return true;
}

// The previous MeterColorMapper pass forms MeterColorStatements with all
// the information needed for the P4TableMap color actions.
bool SwitchCaseDecoder::preorder(const IR::MeterColorStatement* statement) {
  VLOG(1) << "MeterColorStatement in switch case";
  const std::string meter_color_actions(statement->meter_color_actions);
  color_actions_.push_back(std::make_pair(action_, meter_color_actions));
  return false;
}

// The general IR::Statement preorder catches any statements that the
// SwitchCaseDecoder does not explictly support in other preorder methods.
bool SwitchCaseDecoder::preorder(const IR::Statement* statement) {
  ::error("Backend: Unexpected %s statement in switch case", statement);
  return false;
}

void SwitchCaseDecoder::ClearDecodeState() {
  applied_table_ = nullptr;
  color_actions_.clear();
  ClearCaseState();
}

void SwitchCaseDecoder::ClearCaseState() {
  action_.clear();
}

}  // namespace p4c_backends
}  // namespace stratum
