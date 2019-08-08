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

// The TunnelTypeMapper operates on P4PipelineConfig action descriptors, looks
// for the presence of packet tunneling operations, and attempts to simplify
// them into a single P4TunnelProperties message.  It also verifies that the P4
// program does not perform invalid or unsupported tunnel operations, such
// as attempting to encap and decap a packet in the same P4 action.

#ifndef STRATUM_P4C_BACKENDS_FPM_TUNNEL_TYPE_MAPPER_H_
#define STRATUM_P4C_BACKENDS_FPM_TUNNEL_TYPE_MAPPER_H_

#include <string>
#include <vector>

#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace p4c_backends {

// A TunnelTypeMapper runs after the p4c backend has populated the pipeline
// config with complete action descriptors and determined all possible field
// and header types.  At this point, a TunnelTypeMapper instance executes
// its ProcessTunnels method to refine tunnel type data in the P4PipelineConfig.
class TunnelTypeMapper {
 public:
  explicit TunnelTypeMapper(hal::P4PipelineConfig* p4_pipeline_config);
  virtual ~TunnelTypeMapper() {}

  // Iterates over all the action descriptors in the constructor-injected
  // p4_pipeline_config_ to find sequences of packet header changes that
  // perform packet encap and decap operations.  It simplifies these operations
  // into a single P4TunnelProperties message and updates the affected action
  // descriptor.  It also validates the tunnel operations in each action and
  // reports problems as P4 program errors via p4c's ErrorReporter.
  //
  // Example: an action descriptor representing this P4 action logic:
  //    hdr.inner.ipv4 = hdr.ipv4_base;
  //    <GRE flag assignment statements>;
  //    hdr.gre.setValid();
  //
  // Becomes this P4TunnelProperties data in the P4PipelineConfigaction
  // descriptor:
  //    tunnel_properties {
  //      encap_inner_header: P4_HEADER_IPV4
  //      is_gre_tunnel: true
  //    }
  void ProcessTunnels();

  // TunnelTypeMapper is neither copyable nor movable.
  TunnelTypeMapper(const TunnelTypeMapper&) = delete;
  TunnelTypeMapper& operator=(const TunnelTypeMapper&) = delete;

 private:
  // Processes any tunnel operations within a single action represented by
  // action_descriptor, updating the descriptor if the action does valid
  // tunnel encaps or decaps.
  void ProcessActionTunnel(hal::P4ActionDescriptor* action_descriptor);

  // These private methods process a potential tunnel_action within an
  // action descriptor.  Each method can do one of the following:
  //  - Update p4_tunnel_properties_ if it finds a valid encap or decap step.
  //  - Update gre_header_op_ if the encap/decap occurs with GRE.
  //  - Append error data to tunnel_error_message_ to report P4 program bugs.
  // All methods return true if the input tunnel_action turns out to be
  // encap or decap related, regardless of whether the encap or decap is
  // valid.  They return false only if the tunnel_action represents a
  // non-tunneled header.
  bool FindInnerEncapHeader(
      const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action);
  bool FindGreHeader(
      const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action);
  bool FindInnerDecapHeader(
      const hal::P4ActionDescriptor::P4TunnelAction& tunnel_action);

  // Checks whether the header type is something that Stratum knows
  // how to tunnel.
  bool CheckInnerHeaderType(P4HeaderType header_type);

  // Examines the assignments in action_descriptor for any that affect fields
  // of interest to tunneling, particularly the effects of encap/decap on
  // TTL, ECN, and DSCP; also determines the outer header type for encap.
  void ProcessTunnelAssignments(hal::P4ActionDescriptor* action_descriptor);

  // Performs special handling of TTL, ECN, and DSCP assignments.  When the
  // return value is true, ProcessDscpEcnTtlAssignment has integrated the
  // input assignment into the action's tunnel_properties.
  bool ProcessDscpEcnTtlAssignment(
      const hal::P4ActionDescriptor::P4ActionInstructions& assignment,
      const hal::P4FieldDescriptor& dest_field);

  // Once the action's tunnel properties have been successfully identified
  // and processed, this method commits the new p4_tunnel_properties_ data
  // into the action_descriptor.
  void UpdateActionTunnelProperties(hal::P4ActionDescriptor* action_descriptor);

  // The TunnelTypeMapper instance operates on this P4PipelineConfig, which
  // is injected via the costructor.
  hal::P4PipelineConfig* p4_pipeline_config_;

  bool processed_tunnels_;  // Becomes true after ProcessTunnels executes.

  // These private members define the tunnel processing state for the
  // P4 action currently being processed:
  //  action_name_ - identifies the action currently being processed, mainly
  //      for logging and error reporting.
  //  p4_tunnel_properties_ - accumulates P4TunnelProperties data for the
  //      current action.
  //  gre_header_op_ - detects attempts to do GRE encap and decap in the
  //      same action.
  //  is_encap_ and is_decap_ - these flags indicate whether the current P4
  //      action has encap or decap properties.  It is an error for both
  //      flags to be true.
  //  optimized_assignments_ - contains the field index of any action descriptor
  //      assignment field that TunnelTypeMapper determines can be optimized
  //      into the action's tunnel_properties.
  //  tunnel_error_message_ - contains the error string for p4c's ErrorReporter
  //      when the action does invalid tunneling; empty when the action
  //      has no tunnel errors.
  std::string action_name_;
  hal::P4ActionDescriptor::P4TunnelProperties p4_tunnel_properties_;
  P4HeaderOp gre_header_op_;
  bool is_encap_;
  bool is_decap_;
  std::vector<int> optimized_assignments_;
  std::string tunnel_error_message_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // STRATUM_P4C_BACKENDS_FPM_TUNNEL_TYPE_MAPPER_H_
