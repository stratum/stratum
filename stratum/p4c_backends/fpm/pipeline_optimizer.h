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

// The PipelineOptimizer attempts to optimize P4Control logic according to
// the available forwarding pipeline stages. It runs various optimization
// passes to place as many tables and related logic as possibe into fixed-
// function pipeline stages.
// TODO: Consider an interface wrapper around this class to handle
// different types of targets in the future.

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_PIPELINE_OPTIMIZER_H_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_PIPELINE_OPTIMIZER_H_

#include "external/com_github_p4lang_p4c/frontends/common/resolveReferences/referenceMap.h"
#include "external/com_github_p4lang_p4c/frontends/p4/coreLibrary.h"
#include "external/com_github_p4lang_p4c/frontends/p4/typeChecking/typeChecker.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

class PipelineOptimizer {
 public:
  // The constructor requires p4c's ReferenceMap and TypeMap for use by its
  // internal optimization passes.
  PipelineOptimizer(P4::ReferenceMap* ref_map, P4::TypeMap* type_map);
  virtual ~PipelineOptimizer() {}

  // Runs a series of optimization passes on the P4Control input.  If any
  // optimization is possible, it returns a pointer to a transformed control.
  // If no optimization is possible, it returns the original control.
  // There is no clear ownership of the returned P4Control pointer.  Instead
  // of establishing object ownership rules, p4c depends on a garbage collector
  // to free memory that is no longer used.  The Stratum p4c binary does
  // not enable this garbage collector.
  virtual const IR::P4Control* Optimize(const IR::P4Control& control);

  // PipelineOptimizer is neither copyable nor movable.
  PipelineOptimizer(const PipelineOptimizer&) = delete;
  PipelineOptimizer& operator=(const PipelineOptimizer&) = delete;

 private:
  // These members store the injected parameters.
  P4::ReferenceMap* ref_map_;
  P4::TypeMap* type_map_;
};

}  // namespace p4c_backends
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_PIPELINE_OPTIMIZER_H_
