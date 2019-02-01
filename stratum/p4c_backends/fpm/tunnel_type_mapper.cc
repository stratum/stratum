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

// This file contains the TunnelTypeMapper implementation.

#include "stratum/p4c_backends/fpm/tunnel_type_mapper.h"

#include "base/logging.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "absl/strings/substitute.h"

namespace stratum {
namespace p4c_backends {

TunnelTypeMapper::TunnelTypeMapper(hal::P4PipelineConfig* p4_pipeline_config)
    : p4_pipeline_config_(ABSL_DIE_IF_NULL(p4_pipeline_config)),
      processed_tunnels_(false),
      gre_header_op_(P4_HEADER_NOP),
      is_encap_(false),
      is_decap_(false) {
}

void TunnelTypeMapper::ProcessTunnels() {
  if (processed_tunnels_) {
    LOG(WARNING) << "Ignoring repeat call to " << __PRETTY_FUNCTION__;
    return;
  }

  for (auto& table_map_iter : *p4_pipeline_config_->mutable_table_map()) {
    if (!table_map_iter.second.has_action_descriptor()) continue;
    hal::P4ActionDescriptor* action_descriptor =
        table_map_iter.second.mutable_action_descriptor();
    if (action_descriptor->tunnel_actions_size() == 0) continue;
    action_name_ = table_map_iter.first;
    ProcessActionTunnel(action_descriptor);
  }

  processed_tunnels_ = true;
}

void TunnelTypeMapper::ProcessActionTunnel(
    hal::P4ActionDescriptor* action_descriptor) {
  // The per-action state needs to be reset.
  gre_header_op_ = P4_HEADER_NOP;
  is_encap_ = false;
  is_decap_ = false;
  p4_tunnel_properties_.Clear();
  optimized_assignments_.clear();
  tunnel_error_message_.clear();
  bool is_tunnel_action = false;

  for (const auto& tunnel_action : action_descriptor->tunnel_actions()) {
    bool found_encap_decap = FindInnerEncapHeader(tunnel_action);
    is_tunnel_action = is_tunnel_action || found_encap_decap;
    if (found_encap_decap) continue;
    found_encap_decap = FindGreHeader(tunnel_action);
    is_tunnel_action = is_tunnel_action || found_encap_decap;
    if (found_encap_decap) continue;
    found_encap_decap = FindInnerDecapHeader(tunnel_action);
    is_tunnel_action = is_tunnel_action || found_encap_decap;
  }

  if (is_tunnel_action) {
    // The action does tunneling.  Some action-wide error checks are done
    // below for consistency across all tunnel_actions in action_descriptor.
    if (is_encap_ && is_decap_) {
      tunnel_error_message_ +=
          "The same action cannot do both encap and decap tunnels. ";
    }

    if (gre_header_op_ != P4_HEADER_NOP) {
      if (!is_encap_ && !is_decap_) {
        tunnel_error_message_ +=
            "An action cannot do a GRE tunnel without an inner header "
            "encap or decap. ";
      } else if (gre_header_op_ == P4_HEADER_SET_VALID && is_decap_) {
        tunnel_error_message_ +=
            "An action cannot mark the GRE header valid during tunnel decap. ";
      } else if (gre_header_op_ == P4_HEADER_SET_INVALID && is_encap_) {
        tunnel_error_message_ +=
            "An action cannot invalidate the GRE header during tunnel encap. ";
      }
    }

    ProcessTunnelAssignments(action_descriptor);

    // For error-free tunnels, the p4_tunnel_properties_ replace the
    // original tunnel_actions in the action descriptor.
    if (tunnel_error_message_.empty()) {
      UpdateActionTunnelProperties(action_descriptor);
    } else {
      ::error("Backend: Action %s tunnel error - %s",
              action_name_.c_str(), tunnel_error_message_.c_str());
      VLOG(1) << "Action " << action_name_ << " tunnel error: "
              << tunnel_error_message_
              << p4_tunnel_properties_.ShortDebugString()
              << " descriptor: " << action_descriptor->ShortDebugString();
    }
  } else {
    // The action's tunnel_actions turned out to be irrelevant, so the
    // action_descriptor doesn't need them.
    action_descriptor->clear_tunnel_actions();
  }
}

// The input tunnel_action represents a tunnel encap when:
//  - The header valid bit is set or copied from another header.
//  - The header descriptor indicates an inner header.
bool TunnelTypeMapper::FindInnerEncapHeader(
    const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action) {
  bool is_encap = false;
  if (tunnel_action.header_op() == P4_HEADER_SET_VALID ||
      tunnel_action.header_op() == P4_HEADER_COPY_VALID) {
    const auto& header_descriptor = FindHeaderDescriptorOrDie(
        tunnel_action.header_name(), *p4_pipeline_config_);
    if (header_descriptor.depth() > 0) {
      if (!CheckInnerHeaderType(header_descriptor.type())) {
        return true;
      }
      if (p4_tunnel_properties_.encap().encap_inner_headers_size() != 0) {
        tunnel_error_message_ += absl::Substitute(
            "A P4 action cannot encap multiple inner headers, $0 and $1. ",
            P4HeaderType_Name(
                p4_tunnel_properties_.encap().encap_inner_headers(0)).c_str(),
            P4HeaderType_Name(header_descriptor.type()).c_str());
        return true;
      }
      p4_tunnel_properties_.mutable_encap()->add_encap_inner_headers(
          header_descriptor.type());
      is_encap = true;
    }
  }

  is_encap_ = is_encap_ || is_encap;
  return is_encap;
}

// The input tunnel_action represents a GRE tunnel when:
//  - The header descriptor indicates the GRE header type.
//  - The header valid bit is set or cleared directly.  A header-to-header
//    copy (P4_HEADER_COPY_VALID) makes no sense for a GRE header.
bool TunnelTypeMapper::FindGreHeader(
    const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action) {
  bool is_gre = false;
  const auto& header_descriptor = FindHeaderDescriptorOrDie(
      tunnel_action.header_name(), *p4_pipeline_config_);

  if (header_descriptor.type() == P4_HEADER_GRE) {
    is_gre = true;
    if (tunnel_action.header_op() == P4_HEADER_SET_VALID ||
        tunnel_action.header_op() == P4_HEADER_SET_INVALID) {
      if (gre_header_op_ != P4_HEADER_NOP &&
          gre_header_op_ != tunnel_action.header_op()) {
        tunnel_error_message_ +=
            "GRE encap and decap cannot occur in the same action. ";
        return true;
      }
      p4_tunnel_properties_.set_is_gre_tunnel(true);
      gre_header_op_ = tunnel_action.header_op();
    } else {
      // TODO: Are there any valid use cases for GRE header copies?
      tunnel_error_message_ +=
          "GRE header-to-header copy is an invalid tunnel operation. ";
      return true;
    }
  }

  return is_gre;
}

// The input tunnel_action represents a tunnel decap when:
//  - The header valid bit is invalidated.
//  - The header descriptor indicates an inner header.
// The Stratum P4 programs aggressively invalidate headers during decap,
// so the same tunnel can decap multiple header types, with the actual type
// being resolved by the P4Runtime service.
bool TunnelTypeMapper::FindInnerDecapHeader(
    const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action) {
  bool is_decap = false;
  if (tunnel_action.header_op() == P4_HEADER_SET_INVALID) {
    const auto& header_descriptor = FindHeaderDescriptorOrDie(
        tunnel_action.header_name(), *p4_pipeline_config_);
    if (header_descriptor.depth() > 0) {
      if (!CheckInnerHeaderType(header_descriptor.type())) {
        return true;
      }
      p4_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
          header_descriptor.type());
      is_decap = true;
    }
  }

  is_decap_ = is_decap_ || is_decap;
  return is_decap;
}

bool TunnelTypeMapper::CheckInnerHeaderType(P4HeaderType header_type) {
  bool valid_type = true;
  switch (header_type) {
    case P4_HEADER_IPV4:
    case P4_HEADER_IPV6:
      break;
    case P4_HEADER_GRE:
      tunnel_error_message_ += "GRE-in-GRE tunnels are not allowed. ";
      valid_type = false;
      break;
    default:
      tunnel_error_message_ += absl::Substitute(
          "$0 is not supported as an inner tunnel header. ",
          P4HeaderType_Name(header_type).c_str());
      valid_type = false;
      break;
  }

  return valid_type;
}

void TunnelTypeMapper::ProcessTunnelAssignments(
    hal::P4ActionDescriptor* action_descriptor) {
  // The default assumption is that ECN, DSCP, and TTL get copied between
  // outer and inner headers.
  p4_tunnel_properties_.mutable_ecn_value()->set_copy(true);
  p4_tunnel_properties_.mutable_dscp_value()->set_copy(true);
  p4_tunnel_properties_.mutable_ttl_value()->set_copy(true);
  P4HeaderType outer_encap_type = P4_HEADER_UNKNOWN;

  for (int i = 0; i < action_descriptor->assignments_size(); ++i) {
    const auto& assignment = action_descriptor->assignments(i);
    DCHECK(!assignment.destination_field_name().empty());
    const auto& field_name = assignment.destination_field_name();
    auto field_descriptor = FindFieldDescriptorOrNull(
        field_name, *p4_pipeline_config_);
    if (field_descriptor == nullptr) continue;  // Header-to-header copy.
    if (field_descriptor->is_local_metadata()) continue;
    if (field_descriptor->header_type() != P4_HEADER_IPV4 &&
        field_descriptor->header_type() != P4_HEADER_IPV6) {
      continue;
    }

    // Assignments to TTL, ECN, and DSCP are subject to special treatment.
    // When copied between inner and outer headers, their assignments can be
    // removed from the action descriptor.
    if (ProcessDscpEcnTtlAssignment(assignment, *field_descriptor)) {
      optimized_assignments_.push_back(i);
    }

    if (is_encap_) {
      // When this action does encap, the target field can reveal the
      // outer encap header type.
      const auto& header_descriptor = FindHeaderDescriptorForFieldOrDie(
          field_name, field_descriptor->header_type(), *p4_pipeline_config_);
      if (header_descriptor.depth() == 0) {
        if (outer_encap_type == P4_HEADER_UNKNOWN) {
          outer_encap_type = header_descriptor.type();
          p4_tunnel_properties_.mutable_encap()->set_encap_outer_header(
              outer_encap_type);
        } else if (outer_encap_type != header_descriptor.type()) {
          tunnel_error_message_ += absl::Substitute(
              "Action $0 has ambiguous outer encap headers $1 and $2. ",
              action_name_.c_str(), P4HeaderType_Name(outer_encap_type).c_str(),
              P4HeaderType_Name( header_descriptor.type()).c_str());
        }
      }
    }
  }
}

bool TunnelTypeMapper::ProcessDscpEcnTtlAssignment(
    const hal::P4ActionDescriptor::P4ActionInstructions& assignment,
    const hal::P4FieldDescriptor& dest_field) {
  // For assignment destination field types other than P4_FIELD_TYPE_NW_TTL,
  // P4_FIELD_TYPE_DSCP, or P4_FIELD_TYPE_ECN, there is nothing to do.
  if (dest_field.type() != P4_FIELD_TYPE_NW_TTL &&
      dest_field.type() != P4_FIELD_TYPE_DSCP &&
      dest_field.type() != P4_FIELD_TYPE_ECN) {
    return false;
  }

  bool field_to_field = false;
  switch (assignment.assigned_value().source_value_case()) {
    case P4AssignSourceValue::kSourceFieldName:
      // Copies of fields between inner and outer headers need more evaluation.
      field_to_field = true;
      break;
    case P4AssignSourceValue::kConstantParam:
      // Setting TTL/ECN/DSCP to a constant is currently unsupported.
      tunnel_error_message_ += absl::Substitute(
          "Action $0 has unsupported assignment of constant to tunnel field $1 "
          "in $2. ", action_name_.c_str(),
          assignment.destination_field_name().c_str(),
          assignment.ShortDebugString().c_str());
      break;
    case P4AssignSourceValue::kParameterName:
      // Setting TTL/ECN/DSCP to an action parameter is currently unsupported.
      tunnel_error_message_ += absl::Substitute(
          "Action $0 has unsupported assignment of action parameter to tunnel "
          "field $1 in $2. ", action_name_.c_str(),
          assignment.destination_field_name().c_str(),
          assignment.ShortDebugString().c_str());
      break;
    case P4AssignSourceValue::kSourceHeaderName:
    default:
      tunnel_error_message_ += absl::Substitute(
          "Action $0 has malformed assignment to tunnel field $1 in $2. ",
          action_name_.c_str(), assignment.destination_field_name().c_str(),
          assignment.ShortDebugString().c_str());
      break;
  }

  // Valid field-to-field copies should have matching field types, e.g.
  // both are P4_FIELD_TYPE_NW_TTL, and it should copy a field from one header
  // to another, not from an intermediate metadata field.
  if (field_to_field) {
    const std::string& source_field_name =
        assignment.assigned_value().source_field_name();
    auto source_field = FindFieldDescriptorOrNull(
        source_field_name, *p4_pipeline_config_);
    DCHECK(source_field != nullptr);
    if (source_field->type() != dest_field.type() ||
        source_field->is_local_metadata()) {
      tunnel_error_message_ += absl::Substitute(
          "Action $0 has unexpected assignment of non-matching tunnel field "
          "types in $1. ", action_name_.c_str(),
          assignment.ShortDebugString().c_str());
      field_to_field = false;
    }
  }

  return field_to_field;
}

void TunnelTypeMapper::UpdateActionTunnelProperties(
    hal::P4ActionDescriptor* action_descriptor) {
  *action_descriptor->mutable_tunnel_properties() = p4_tunnel_properties_;
  action_descriptor->clear_tunnel_actions();

  // Delete any assignments that were optimized out above.
  DeleteRepeatedFields(optimized_assignments_,
                       action_descriptor->mutable_assignments());
}

}  // namespace p4c_backends
}  // namespace stratum
