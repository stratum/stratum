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

// The p4c backend creates an InternalAction in the following situations:
//  1) Action statements from multiple P4 logical tables need to merge into
//     a single action that applies to one physical table.  The Stratum
//     encap/decap flows with hidden tables represent this situation.
//  2) An action needs to be extended to include metering conditions that the
//     P4 program defines in the control logic outside the action.  The
//     Stratum punt control with meter-based cloning represents this situation.
// In both of these situations, the InternalAction avoids changing the logic
// in the original action.  This is important where multiple tables use the
// same action, and new internal actions contain logic that varies from
// table to table.

#ifndef STRATUM_P4C_BACKENDS_FPM_INTERNAL_ACTION_H_
#define STRATUM_P4C_BACKENDS_FPM_INTERNAL_ACTION_H_

#include <string>

#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/tunnel_optimizer_interface.h"

namespace stratum {
namespace p4c_backends {

// The typical usage is to construct an InternalAction based on an original
// action, which is generally from the first table in a sequence of multiple
// logical tables.  Thereafter, the InternalAction can be updated by merging
// additional action descriptors and/or metering conditions with the
// MergeAction and MergeMeterCondition methods, respectively.  As these
// updates occur, this class updates a unique internal name and an action
// descriptor to represent the InternalAction, both of which are available
// via accessors.  At present, InternalAction only supports action descriptors
// with type P4_ACTION_TYPE_FUNCTION.  In many cases, the original_action
// input to the constructor is not the same as the existing P4ActionDescriptor
// for original_name in pipeline_config, such as when the descriptor comes from
// a HiddenTableMapper's ActionRedirectMap.  However, the other InternalAction
// methods expect to be able to look up merged actions in pipeline_config.
class InternalAction {
 public:
  // The InternalAction has two flavors, with and without an injected
  // TunnelOptimizerInterface.  Creators of instances with no tunnel actions
  // do not need to provide the TunnelOptimizerInterface.
  InternalAction(const std::string& original_name,
                 const hal::P4ActionDescriptor& original_action,
                 const hal::P4PipelineConfig& pipeline_config);
  InternalAction(const std::string& original_name,
                 const hal::P4ActionDescriptor& original_action,
                 const hal::P4PipelineConfig& pipeline_config,
                 TunnelOptimizerInterface* tunnel_optimizer);
  virtual ~InternalAction() {}

  // This method merges the behavior of action_name into this InternalAction
  // instance.  It generally applies to Stratum encap/decap tables, where
  // the actions from hidden tables need to merge into actions that are
  // dynamically updated via the P4 runtime API.  MergeAction can be called
  // multiple times per instance, with the limitation that a single instance
  // can only record one set of P4TunnelProperties.
  //
  // If MergeAction encounters multiple levels of redirection, which happens
  // when the input action has an internal_link to another InternalAction, it
  // removes the indirect link by merging a copy of the linked action into
  // this InternalAction.  This kind of indirection typically happens when
  // a hidden table also performs metering actions.
  void MergeAction(const std::string& action_name);

  // This method merges text-encoded P4MeterColorActions into this
  // InternalAction instance.  In typical usage, the meter_condition string
  // comes from an IR MeterColorStatement.
  void MergeMeterCondition(const std::string& meter_condition);

  // This method makes a pass through the internal_descriptor_ assignments
  // looking for sequences such as:
  //  m = p;
  //  f = m;
  // That can be simplified into:
  //  f = p;
  // These sequences typically occur during MergeAction when the original
  // action assigns a parameter "p" to a metadata field "m", and then a merged
  // hidden action subsequently assigns "m" to a packet header field "f".
  // Thus, Optimize should generally be called once after all MergeAction
  // updates.
  void Optimize();

  // These two methods output a table map entry for this InternalAction.  The
  // two methods accommodate callers that manage P4PipelineConfig entries
  // directly or make use of a TableMapGenerator.  The caller is responsible
  // for creating the necessary links from the original action to the
  // InternalAction.
  void WriteToP4PipelineConfig(hal::P4PipelineConfig* pipeline_config);
  void WriteToTableMapGenerator(TableMapGenerator* table_map_generator);

  // Accessors.
  const std::string& internal_name() const { return internal_name_; }
  const hal::P4ActionDescriptor& internal_descriptor() const {
    return internal_descriptor_;
  }

  // InternalAction is neither copyable nor movable.
  InternalAction(const InternalAction&) = delete;
  InternalAction& operator=(const InternalAction&) = delete;

 private:
  // Does common initialization for both constructors.
  void Init(const std::string& original_name);

  // Does the common work for the public MergeAction and the private
  // MergeRedirectActions.
  void MergeActionDescriptor(const std::string& action_name,
                             const hal::P4ActionDescriptor& action_descriptor);

  // Handles cases where MergeAction finds that its input action is already
  // redirecting to other actions.
  void MergeRedirectActions(const hal::P4ActionDescriptor& action_descriptor);

  // Scans internal_descriptor_ and removes any duplicate assignments that
  // appear after merging multiple actions.
  void RemoveDuplicateAssignments();

  // Appends the input name to internal_name_.
  void AppendName(const std::string& name);

  // Contains a unique name for identifying this InternalAction - expands
  // with each MergeAction or MergeMeterCondition update.
  std::string internal_name_;

  // Provides an action descriptor for this instance, which is based on the
  // original action plus all descriptor data updates by MergeAction and/or
  // MergeMeterCondition.
  hal::P4ActionDescriptor internal_descriptor_;

  // Injected via constructor.
  const hal::P4PipelineConfig& pipeline_config_;

  // Injected via constructor, may be nullptr if this InternalAction does not
  // need to deal with encap/decap tunnel properties in actions.
  TunnelOptimizerInterface* tunnel_optimizer_;

  // Provides an integer component for internal_name_ each time
  // MergeMeterCondition is called.
  int meter_count_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_INTERNAL_ACTION_H_
