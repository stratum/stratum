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

// This file declares a TunnelOptimizerInterface subclass that tunes tunnel
// actions for Broadcom devices.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TUNNEL_OPTIMIZER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TUNNEL_OPTIMIZER_H_

#include "stratum/p4c_backends/fpm/tunnel_optimizer_interface.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace p4c_backends {

// The p4c backend can create one BcmTunnelOptimizer instance, then call
// Optimize and/or MergeAndOptimize repeatedly to handle all of the tunnel
// actions in the P4 program.
class BcmTunnelOptimizer : public TunnelOptimizerInterface {
 public:
  BcmTunnelOptimizer();
  ~BcmTunnelOptimizer() override {}

  // BCM-specific overrides.
  bool Optimize(const hal::P4ActionDescriptor& input_action,
                hal::P4ActionDescriptor* optimized_action) override;
  bool MergeAndOptimize(const hal::P4ActionDescriptor& input_action1,
                        const hal::P4ActionDescriptor& input_action2,
                        hal::P4ActionDescriptor* optimized_action) override;

  // BcmTunnelOptimizer is neither copyable nor movable.
  BcmTunnelOptimizer(const BcmTunnelOptimizer&) = delete;
  BcmTunnelOptimizer& operator=(const BcmTunnelOptimizer&) = delete;

 private:
  // Does common internal state initialization for Optimize and
  // MergeAndOptimize.
  void InitInternalState();

  // Determines whether the input action descriptor contains valid tunnel
  // properties.  Also assures that properties are consistent with the
  // internal state.  For example, the MergeAndOptimize method cannot have
  // one action that does encap and another that does decap.
  bool IsValidTunnelAction(const hal::P4ActionDescriptor& action);

  // Merges the tunnel_properties, assignments, and other descriptor content
  // from both input actions and stores the result in the internal_descriptor_.
  // Returns true unless conflicts are present in the inputs.
  bool MergeTunnelActions(const hal::P4ActionDescriptor& input_action1,
                          const hal::P4ActionDescriptor& input_action2);

  // OptimizeInternal does optimizations on the internal_descriptor_,
  // and writes optimized results to optimized_descriptor.  OptimizeEncap
  // and OptimizeDecap do encap/decap specific optimizations on the
  // internal_descriptor_.
  void OptimizeInternal(hal::P4ActionDescriptor* optimized_descriptor);
  void OptimizeEncap();
  void OptimizeDecap();

  // RemoveDuplicateHeaderTypes looks for duplicate P4HeaderType values in
  // header_types, which may arise after merging multiple sets of tunnel
  // properties.  Upon output, header_types may be modified by removal of
  // duplicates if any are present.  (The generated protoc files store the
  // header types as ints, not as P4HeaderTypes.)
  void RemoveDuplicateHeaderTypes(
      ::google::protobuf::RepeatedField<int>* header_types);

  // RemoveHeaderCopies scans the assignments in internal_descriptor_ for
  // header-to-header copies and removes them.  The tunnel_properties should
  // provide sufficient information in place of the header assignments.
  void RemoveHeaderCopies();

  // Members to track internal state:
  //  encap_or_decap_ - Records whether the input actions are doing encap
  //      or decap.
  //  internal_descriptor_ - provides intermediate action descriptor for
  //      processing inputs.
  hal::P4ActionDescriptor::P4TunnelProperties::EncapOrDecapCase encap_or_decap_;
  hal::P4ActionDescriptor internal_descriptor_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_BCM_BCM_TUNNEL_OPTIMIZER_H_
