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

// This file contains the PipelineOptimizer implementation.

#include "stratum/p4c_backends/fpm/pipeline_optimizer.h"

#include "base/commandlineflags.h"
#include "stratum/p4c_backends/fpm/pipeline_block_passes.h"
#include "stratum/p4c_backends/fpm/pipeline_intra_block_passes.h"

DEFINE_bool(enable_pipeline_optimization, true,
            "Enables optimization of P4Control logic into pipeline stages");

namespace stratum {
namespace p4c_backends {

PipelineOptimizer::PipelineOptimizer(P4::ReferenceMap* ref_map,
                                     P4::TypeMap* type_map)
    : ref_map_(ABSL_DIE_IF_NULL(ref_map)),
      type_map_(ABSL_DIE_IF_NULL(type_map)) {}

const IR::P4Control* PipelineOptimizer::Optimize(const IR::P4Control& control) {
  if (!FLAGS_enable_pipeline_optimization) {
    LOG(INFO) << "Pipeline stage optimization is disabled";
    return &control;
  }

  // The FixedTableInspector pass simply checks whether the control has any
  // potential optimizations.
  FixedTableInspector fixed_table_inspector;
  if (!fixed_table_inspector.FindFixedTables(control)) {
    VLOG(2) << "P4Control " << control.externalName()
            << " has no fixed tables";
    return &control;
  }

  // The passes below can each transform the input control if they are able
  // to perform their respective optimizations.
  PipelineIfBlockInsertPass pass1;
  auto pass1_control = pass1.InsertBlocks(control);
  PipelineBlockPass pass2(ref_map_, type_map_);
  auto pass2_control = pass2.OptimizeControl(*pass1_control);
  PipelineIfElsePass pass3(ref_map_, type_map_);
  auto pass3_control = pass3.OptimizeControl(*pass2_control);
  PipelineIntraBlockPass pass4(ref_map_, type_map_);
  auto pass4_control = pass4.OptimizeControl(*pass3_control);

  return pass4_control;
}

}  // namespace p4c_backends
}  // namespace stratum
