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

// This file contains the BcmTunnelOptimizer's implementation.

#include "stratum/p4c_backends/fpm/bcm/bcm_tunnel_optimizer.h"

#include <set>
#include <vector>

#include "stratum/glue/logging.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/p4c_backends/fpm/utils.h"

namespace stratum {
namespace p4c_backends {

BcmTunnelOptimizer::BcmTunnelOptimizer()
    : encap_or_decap_(
          hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET) {
}

bool BcmTunnelOptimizer::Optimize(
    const hal::P4ActionDescriptor& input_action,
    hal::P4ActionDescriptor* optimized_action) {
  DCHECK(optimized_action != nullptr);
  InitInternalState();
  if (!IsValidTunnelAction(input_action)) return false;
  internal_descriptor_ = input_action;
  OptimizeInternal(optimized_action);

  return true;
}

bool BcmTunnelOptimizer::MergeAndOptimize(
    const hal::P4ActionDescriptor& input_action1,
    const hal::P4ActionDescriptor& input_action2,
    hal::P4ActionDescriptor* optimized_action) {
  DCHECK(optimized_action != nullptr);
  InitInternalState();
  if (!IsValidTunnelAction(input_action1)) return false;
  if (!IsValidTunnelAction(input_action2)) return false;

  if (!MergeTunnelActions(input_action1, input_action2)) return false;
  OptimizeInternal(optimized_action);

  return true;
}

void BcmTunnelOptimizer::InitInternalState() {
  internal_descriptor_.Clear();
  encap_or_decap_ =
      hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET;
}

// If there is ever a non-BCM target, this might belong in a common base class.
bool BcmTunnelOptimizer::IsValidTunnelAction(
    const hal::P4ActionDescriptor& action) {
  bool valid = action.has_tunnel_properties();
  if (valid) {
    auto encap_or_decap = action.tunnel_properties().encap_or_decap_case();
    if (encap_or_decap !=
        hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET &&
        (encap_or_decap == encap_or_decap_ ||
         encap_or_decap_ ==
         hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET)) {
      encap_or_decap_ = encap_or_decap;
    } else {
      valid = false;
    }
  }

  if (!valid) {
    ::error("Backend: Failed to optimize invalid tunnel action %s",
            action.ShortDebugString().c_str());
  }

  return valid;
}

bool BcmTunnelOptimizer::MergeTunnelActions(
    const hal::P4ActionDescriptor& input_action1,
    const hal::P4ActionDescriptor& input_action2) {
  DCHECK_NE(hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET,
            encap_or_decap_);

  // The inner headers can be different in the merged actions.  Differences
  // will be handled during P4Runtime action processing. GRE, ECN, DSCP, and
  // TTL must all match.  These constraints are imposed by creating temporary
  // descriptor copies in tunnel1 and tunnel2, clearing the relevant inner
  // headers, and then using a MessageDifferencer to compare the remaining
  // fields.
  hal::P4ActionDescriptor::P4TunnelProperties tunnel1 =
                                            input_action1.tunnel_properties();
  hal::P4ActionDescriptor::P4TunnelProperties tunnel2 =
                                            input_action2.tunnel_properties();
  if (encap_or_decap_ == hal::P4ActionDescriptor::P4TunnelProperties::kEncap) {
    tunnel1.mutable_encap()->clear_encap_inner_headers();
    tunnel2.mutable_encap()->clear_encap_inner_headers();
  } else {
    tunnel1.mutable_decap()->clear_decap_inner_headers();
    tunnel2.mutable_decap()->clear_decap_inner_headers();
  }

  ::google::protobuf::util::MessageDifferencer msg_differencer;
  msg_differencer.set_repeated_field_comparison(
      ::google::protobuf::util::MessageDifferencer::AS_SET);
  if (!msg_differencer.Compare(tunnel1, tunnel2)) {
    ::error("Backend: Unable to merge tunnel properties %s and %s",
            tunnel1.ShortDebugString().c_str(),
            tunnel2.ShortDebugString().c_str());
    return false;
  }

  // If above validation succeeds, a simple merge is followed by cleaning
  // up any redundant inner header types.
  internal_descriptor_ = input_action1;
  internal_descriptor_.MergeFrom(input_action2);
  auto tunnel_properties = internal_descriptor_.mutable_tunnel_properties();
  if (encap_or_decap_ == hal::P4ActionDescriptor::P4TunnelProperties::kEncap) {
    RemoveDuplicateHeaderTypes(
        tunnel_properties->mutable_encap()->mutable_encap_inner_headers());
  } else {
    RemoveDuplicateHeaderTypes(
        tunnel_properties->mutable_decap()->mutable_decap_inner_headers());
  }

  return true;
}

void BcmTunnelOptimizer::OptimizeInternal(
    hal::P4ActionDescriptor* optimized_descriptor) {
  switch (encap_or_decap_) {
    case hal::P4ActionDescriptor::P4TunnelProperties::kEncap:
      OptimizeEncap();
      break;
    case hal::P4ActionDescriptor::P4TunnelProperties::kDecap:
      OptimizeDecap();
      break;
    case hal::P4ActionDescriptor::P4TunnelProperties::ENCAP_OR_DECAP_NOT_SET:
      DLOG(FATAL) << "Expected encap or decap case to be set";
      break;
  }

  RemoveHeaderCopies();
  *optimized_descriptor = internal_descriptor_;
}

void BcmTunnelOptimizer::OptimizeEncap() {
  // There are some redundant outer header and GRE protocol assignments that
  // could be filtered here, but for now they remain in the descriptor
  // pending P4Runtime switch implementation.
}

void BcmTunnelOptimizer::OptimizeDecap() {
}

void BcmTunnelOptimizer::RemoveDuplicateHeaderTypes(
    ::google::protobuf::RepeatedField<int>* header_types) {
  std::set<int> header_types_used;
  for (int h = 0; h < header_types->size(); ++h) {
    header_types_used.insert(header_types->Get(h));
  }
  if (header_types_used.size() != header_types->size()) {
    header_types->Clear();
    for (int header_type : header_types_used) {
      header_types->Add(header_type);
    }
  }
}

void BcmTunnelOptimizer::RemoveHeaderCopies() {
  std::vector<int> header_assignments;
  for (int a = 0; a < internal_descriptor_.assignments_size(); ++a) {
    const auto& assignment = internal_descriptor_.assignments(a);
    if (assignment.assigned_value().source_value_case() ==
        P4AssignSourceValue::kSourceHeaderName) {
      header_assignments.push_back(a);
    }
  }

  DeleteRepeatedFields(header_assignments,
                       internal_descriptor_.mutable_assignments());
}

}  // namespace p4c_backends
}  // namespace stratum
