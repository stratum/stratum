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

// This file declares an interface class that optimizes tunnel actions.
// Since optimization decisions may differ by target type, the implementations
// will be in target-specific subclasses.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_INTERFACE_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_INTERFACE_H_

#include "stratum/hal/lib/p4/p4_table_map.pb.h"

namespace stratum {
namespace p4c_backends {

// The p4c backend optionally runs a target-dependent tunnel optimizer on the
// P4ActionDescriptors in the P4PipelineConfig output.  A tunnel optimizer
// can do two things:
//  - Remove assignments in the input descriptor that are not relevant on the
//    target platform.  For example, BCM automatically assigns most GRE header
//    fields during encap, so they do not need to be included in the descriptor.
//  - Remove redundancies in merged action descriptors.  The p4c backend forms
//    encap/decap action descriptors by merging multiple P4 program actions,
//    some of which may contain the same action statements.
class TunnelOptimizerInterface {
 public:
  virtual ~TunnelOptimizerInterface() {}

  // TunnelOptimizerInterface's Optimize method runs an optimization pass on
  // the action descriptor referenced by input_action and stores the optimized
  // output in the descriptor at the optimized_action pointer.  Optimize expects
  // the input_action to contain tunnel_properties, and it tunes its decisions
  // based on whether the tunnel_properties specify encap or decap.  The return
  // value is true if the action is optimized or no possible optimizations were
  // found.  A false return indicates an error in input_action, and Optimize
  // also reports a P4 program error via p4c's ErrorReporter.  Subclass
  // implementations should allow the input and optimzed actions to refer to
  // the same action descriptor.
  virtual bool Optimize(const hal::P4ActionDescriptor& input_action,
                        hal::P4ActionDescriptor* optimized_action) = 0;

  // The MergeAndOptimize method takes two action descriptors as input and
  // consolidates their actions into a single optimized output action.  The
  // return value is true if the input actions are successfully merged.
  // A false return indicates an error in either input action, and
  // MergeAndOptimize also reports a P4 program error via p4c's ErrorReporter.
  // In a typical P4 program, the p4c backend often combines two input actions
  // to produce one output action.  MergeAndOptimze implementations should also
  // be prepared to handle inputs that have been created by previous calls to
  // MergeAndOptimize and/or Optimize.  Subclass implementations should also
  // allow the optimzed action pointer to refer to the same action descriptor
  // as one of the inputs.
  virtual bool MergeAndOptimize(const hal::P4ActionDescriptor& input_action1,
                                const hal::P4ActionDescriptor& input_action2,
                                hal::P4ActionDescriptor* optimized_action) = 0;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TUNNEL_OPTIMIZER_INTERFACE_H_
