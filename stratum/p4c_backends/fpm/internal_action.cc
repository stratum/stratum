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

// This file implements the InternalAction class.

#include "stratum/p4c_backends/fpm/internal_action.h"

#include <map>
#include <set>
#include <vector>

#include "stratum/glue/logging.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"
#include "absl/strings/substitute.h"
#include "stratum/glue/gtl/map_util.h"

namespace stratum {
namespace p4c_backends {

InternalAction::InternalAction(
    const std::string& original_name,
    const hal::P4ActionDescriptor& original_action,
    const hal::P4PipelineConfig& pipeline_config)
    : internal_name_(""),
      internal_descriptor_(original_action),
      pipeline_config_(pipeline_config),
      tunnel_optimizer_(nullptr),
      meter_count_(0) {
  Init(original_name);
}

InternalAction::InternalAction(
    const std::string& original_name,
    const hal::P4ActionDescriptor& original_action,
    const hal::P4PipelineConfig& pipeline_config,
    TunnelOptimizerInterface* tunnel_optimizer)
    : internal_name_(""),
      internal_descriptor_(original_action),
      pipeline_config_(pipeline_config),
      tunnel_optimizer_(ABSL_DIE_IF_NULL(tunnel_optimizer)),
      meter_count_(0) {
  Init(original_name);
}

void InternalAction::MergeAction(const std::string& action_name) {
  MergeActionDescriptor(
      action_name, FindActionDescriptorOrDie(action_name, pipeline_config_));
}

// Since the TableMapGenerator knows how to add a text representation of a
// P4MeterColorAction to an action descriptor, a private TableMapGenerator
// instance does most of the work here.
void InternalAction::MergeMeterCondition(const std::string& meter_condition) {
  AppendName(absl::Substitute("Meter$0", ++meter_count_));
  TableMapGenerator internal_generator;
  internal_generator.AddAction(internal_name_);
  internal_generator.ReplaceActionDescriptor(
      internal_name_, internal_descriptor_);
  internal_generator.AddMeterColorActionsFromString(
      internal_name_, meter_condition);
  internal_descriptor_ = FindActionDescriptorOrDie(
      internal_name_, internal_generator.generated_map());
}

// Currently, Optimize updates field-to-field assignments if and only if
// an action parameter was previously assigned to the source field.
void InternalAction::Optimize() {
  // The map below keeps a list of parameter-to-field assignments in the
  // internal_descriptor_.  The key is the field name, and the value is the
  // name of the parameter assigned to the key field.  The optimized_assignments
  // descriptor accumulates the modified assignments and ultimately replaces
  // the assignments in internal_descriptor_.
  std::map<std::string, std::string> field_to_param;

  // The new_param_assigns set keeps track of parameter names that are
  // substituted for field names in assignment source values.
  std::set<std::string> new_param_assigns;
  hal::P4ActionDescriptor optimized_assignments;

  for (const auto& old_assignment : internal_descriptor_.assignments()) {
    auto new_assignment = optimized_assignments.add_assignments();
    *new_assignment = old_assignment;
    const auto source_case =
        old_assignment.assigned_value().source_value_case();

    // The conditions below evaluate each assignment to determine whether
    // optimization is feasible.  The following outcomes may occur:
    //  1) Any parameter-to-field assignment updates the field_to_param map.
    //  2) A field-to-field assignment checks whether the source field was
    //     previously given a parameter assignment:
    //      a) If true, then the parameter name replaces the source field name.
    //      b) If false, then the destination field may be getting reassigned
    //         a non-parameter, so its field_to_param entry becomes invalid.
    //  3) An assignment of any other source type is equivalent to item 2b.
    if (!new_assignment->destination_field_name().empty()) {
      if (source_case == P4AssignSourceValue::kParameterName) {
        gtl::InsertOrUpdate(&field_to_param,
                            new_assignment->destination_field_name(),
                            new_assignment->assigned_value().parameter_name());
      } else if (source_case == P4AssignSourceValue::kSourceFieldName) {
        const std::string* source_field = gtl::FindOrNull(
            field_to_param,
            new_assignment->assigned_value().source_field_name());
        if (source_field != nullptr) {
          new_assignment->mutable_assigned_value()->set_parameter_name(
              *source_field);
          if (old_assignment.assigned_value().bit_width() == 0) {
            new_param_assigns.insert(*source_field);
          }
        } else {
          field_to_param.erase(new_assignment->destination_field_name());
        }
      } else {
        field_to_param.erase(new_assignment->destination_field_name());
      }
    }
  }

  // This pass removes redundant internal_descriptor_ assignments corresponding
  // to field_to_param entries, subject to membership in the new_param_assigns
  // set, which imposes the following qualifications:
  //  - The loop above introduces another reference to the action parameter
  //    in the field_to_param entry, i.e. at least one assignment of the
  //    parameter needs to remain, as indicated by parameter presence in
  //    new_param_assigns.
  //  - At least one new reference is an exact bit-for-bit replacement.  An
  //    unsliced gre_flags parameter assignment to local_metadata.gre_flags
  //    would be dangerous to remove when all other gre_flags references are
  //    specific bit slices.  Hence, new_param_assigns does not contain entries
  //    for bit-sliced assignments.
  // The intent is to remove parameter assignments to local metadata fields
  // that do nothing but pass parameters between actions with different P4
  // control scope.  There is some slight risk that the "redundant" metadata
  // field has other purposes.  Stratum P4 programs don't have any local
  // metadata at risk for this issue, and even if they did, Stratum targets
  // have no way to transfer application-specific metadata; all metadata is
  // built in to the hardware.
  std::vector<int> redundant_assignments;
  for (int a = 0; a < optimized_assignments.assignments_size(); ++a) {
    auto assignment = optimized_assignments.mutable_assignments(a);
    if (assignment->destination_field_name().empty())
      continue;
    if (assignment->assigned_value().source_value_case() !=
        P4AssignSourceValue::kParameterName) {
      continue;
    }
    const auto& param_name = assignment->assigned_value().parameter_name();
    if (new_param_assigns.find(param_name) == new_param_assigns.end())
      continue;
    const auto& dest_name = assignment->destination_field_name();
    const std::string* field_param =
        gtl::FindOrNull(field_to_param, dest_name);
    if (field_param == nullptr || *field_param != param_name)
        continue;
    redundant_assignments.push_back(a);
  }

  // All assignments that were marked as redundant above are deleted here.
  DeleteRepeatedFields(redundant_assignments,
                       optimized_assignments.mutable_assignments());
  internal_descriptor_.mutable_assignments()->Clear();
  *internal_descriptor_.mutable_assignments() =
      optimized_assignments.assignments();

  // Some action merges end up with redundant P4_ACTION_OP_NOP primitives,
  // which are removed here.
  std::vector<int> action_nops;
  for (int n = 0; n < internal_descriptor_.primitive_ops_size(); ++n) {
    if (internal_descriptor_.primitive_ops(n) == P4_ACTION_OP_NOP)
      action_nops.push_back(n);
  }
  if (!action_nops.empty()) {
    // All NOPs go away if the action does anything else.  Otherwise,
    // one NOP needs to remain.  There is currently no check for the presence
    // of tunnel_properties, but they are unlikely to be present without
    // related assignments.
    if (action_nops.size() < internal_descriptor_.primitive_ops_size() ||
        internal_descriptor_.color_actions_size() > 0 ||
        internal_descriptor_.assignments_size() > 0) {
      DeleteRepeatedNonPtrFields(action_nops,
                                 internal_descriptor_.mutable_primitive_ops());
    } else if (action_nops.size() > 1) {
      action_nops.pop_back();  // Pops one NOP from the deletions vector.
      DeleteRepeatedNonPtrFields(action_nops,
                                 internal_descriptor_.mutable_primitive_ops());
    }
  }
}

void InternalAction::WriteToP4PipelineConfig(
    hal::P4PipelineConfig* pipeline_config) {
  hal::P4TableMapValue temp_value;
  *temp_value.mutable_internal_action() = internal_descriptor_;
  (*pipeline_config->mutable_table_map())[internal_name_] = temp_value;
}

void InternalAction::WriteToTableMapGenerator(
    TableMapGenerator* table_map_generator) {
  table_map_generator->AddInternalAction(internal_name_, internal_descriptor_);
}

void InternalAction::Init(const std::string& original_name) {
  DCHECK(!original_name.empty());
  DCHECK_EQ(P4_ACTION_TYPE_FUNCTION, internal_descriptor_.type());
  AppendName(original_name);

  // The action_redirects in the original action are never useful.
  internal_descriptor_.clear_action_redirects();
}

void InternalAction::MergeActionDescriptor(
    const std::string& action_name,
    const hal::P4ActionDescriptor& action_descriptor) {
  // If action_descriptor is already redirecting, then MergeRedirectActions
  // bypasses the indirection.
  if (action_descriptor.action_redirects_size() != 0) {
    MergeRedirectActions(action_descriptor);
    return;
  }

  DCHECK_EQ(P4_ACTION_TYPE_FUNCTION, action_descriptor.type());
  AppendName(action_name);

  // If no tunnel properties are involved, the action descriptors are passed
  // to a vanilla protobuf MergeFrom.  If tunnel properties are present, the
  // merge is handed off to the tunnel_optimizer_.
  if (tunnel_optimizer_ != nullptr &&
      internal_descriptor_.has_tunnel_properties() &&
      action_descriptor.has_tunnel_properties()) {
    tunnel_optimizer_->MergeAndOptimize(internal_descriptor_, action_descriptor,
                                        &internal_descriptor_);
  } else {
    internal_descriptor_.MergeFrom(action_descriptor);
    if (internal_descriptor_.has_tunnel_properties()) {
      tunnel_optimizer_->Optimize(internal_descriptor_, &internal_descriptor_);
    }
  }

  RemoveDuplicateAssignments();
}

void InternalAction::MergeRedirectActions(
    const hal::P4ActionDescriptor& action_descriptor) {
  if (action_descriptor.action_redirects_size() != 1) {
    LOG(ERROR) << "Unable to handle InternalAction redirection with more than "
               << "one redirected action: "
               << action_descriptor.ShortDebugString();
    return;
  }
  if (action_descriptor.action_redirects(0).internal_links_size() != 1) {
    LOG(ERROR) << "Unable to handle InternalAction redirection with more than "
               << "one internal link: " << action_descriptor.ShortDebugString();
    return;
  }

  const hal::P4ActionDescriptor::P4InternalActionLink& internal_link =
      action_descriptor.action_redirects(0).internal_links(0);
  const auto& table_map_value = gtl::FindOrDie(
      pipeline_config_.table_map(), internal_link.internal_action_name());
  DCHECK(table_map_value.has_internal_action());
  MergeActionDescriptor(internal_link.internal_action_name(),
                        table_map_value.internal_action());
}

void InternalAction::RemoveDuplicateAssignments() {
  // The key for the assignment_targets map is the destination field in an
  // action assignment, and the value is a pointer to a P4ActionInstructions
  // message with the complete assignment.  The vector records the index
  // values of duplicate assignments for deletion.
  std::map<std::string, const hal::P4ActionDescriptor::P4ActionInstructions*>
      assignment_targets;
  std::vector<int> duplicates;
  ::google::protobuf::util::MessageDifferencer assignment_differencer;
  assignment_differencer.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);

  for (int a = 0; a < internal_descriptor_.assignments_size(); ++a) {
    const auto& assignment = internal_descriptor_.assignments(a);
    DCHECK(!assignment.destination_field_name().empty());
    const auto assignment_pointer_value = gtl::FindOrNull(
        assignment_targets, assignment.destination_field_name());

    // These are the possible map lookup outcomes for each assignment:
    //  1) The destination field is already in the assignment_targets map:
    //    a) The current assignment is the same as the map value, so it
    //       is marked as a duplicate for removal.
    //    b) The current assignment differs from the map value, which happens
    //       in some merged tunnel actions where a protocol field is set
    //       according to IPv4 or IPv6 encapsulation.  These are left for
    //       the tunnel optimizer to sort out later, but the map entry is
    //       replaced just in case there is a sequence such as
    //          proto-field = 1;
    //            ...
    //          proto-field = 2;  // Not OK to delete.
    //            ...
    //          proto-field = 2;  // OK to delete.
    //  2) The destination field is not yet in the assignment_targets map,
    //     so a new map entry is added to detect future duplicates of
    //     this assignment.
    if (assignment_pointer_value != nullptr) {
      if (assignment_differencer.Compare(
          assignment, **assignment_pointer_value)) {
        duplicates.push_back(a);
      } else {
        VLOG(1) << "Potential assignment conflict in internal action "
                << internal_name_ << " for field "
                << assignment.destination_field_name();
        gtl::InsertOrUpdate(&assignment_targets,
                            assignment.destination_field_name(), &assignment);
      }
    } else {
      gtl::InsertOrDie(&assignment_targets,
                       assignment.destination_field_name(), &assignment);
    }
  }

  // All duplicate assignments are deleted below.
  DeleteRepeatedFields(duplicates, internal_descriptor_.mutable_assignments());
}

void InternalAction::AppendName(const std::string& name) {
  internal_name_ += absl::Substitute("__$0", name.c_str());
}

}  // namespace p4c_backends
}  // namespace stratum
