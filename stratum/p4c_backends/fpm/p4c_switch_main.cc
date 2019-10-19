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

// This file contains the main entry for the Stratum FPM switch p4c backend.
// The most common switch use case is generating P4Info and a P4PipelineConfig
// from a P4 program. More information can be found here:
// stratum/g3doc/p4c_backends_overview.md

#include <functional>
#include <memory>
#include <vector>

#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/p4c_backends/common/backend_extension_interface.h"
#include "stratum/p4c_backends/common/backend_pass_manager.h"
#include "stratum/p4c_backends/common/p4c_front_mid_real.h"
#include "stratum/p4c_backends/fpm/bcm/bcm_target_info.h"
#include "stratum/p4c_backends/fpm/bcm/bcm_tunnel_optimizer.h"
#include "stratum/p4c_backends/fpm/midend.h"
#include "stratum/p4c_backends/fpm/switch_p4c_backend.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/target_info.h"

using stratum::p4c_backends::AnnotationMapper;
using stratum::p4c_backends::BackendExtensionInterface;
using stratum::p4c_backends::BackendPassManager;
using stratum::p4c_backends::BcmTargetInfo;
using stratum::p4c_backends::BcmTunnelOptimizer;
using stratum::p4c_backends::MidEnd;
using stratum::p4c_backends::MidEndInterface;
using stratum::p4c_backends::P4cFrontMidReal;
using stratum::p4c_backends::SwitchP4cBackend;
using stratum::p4c_backends::TableMapGenerator;
using stratum::p4c_backends::TargetInfo;

DECLARE_int32(stderrthreshold);

int main(int argc, char** argv) {
  FLAGS_stderrthreshold = 1;
  InitGoogle(argv[0], &argc, &argv, true);
  stratum::InitStratumLogging();
  std::unique_ptr<BcmTargetInfo> bcm_target_info(new BcmTargetInfo);
  TargetInfo::InjectSingleton(bcm_target_info.get());
  std::unique_ptr<BcmTunnelOptimizer> bcm_tunnel_optimizer(
      new BcmTunnelOptimizer);
  std::unique_ptr<AnnotationMapper> annotation_mapper(new AnnotationMapper);
  auto midend_callback = std::function<std::unique_ptr<MidEndInterface>(
      CompilerOptions* p4c_options)>(&MidEnd::CreateInstance);
  std::unique_ptr<P4cFrontMidReal> p4c_real_fe_me(
      new P4cFrontMidReal(midend_callback));
  std::unique_ptr<TableMapGenerator> table_mapper(new TableMapGenerator);
  std::unique_ptr<BackendExtensionInterface> extension(new SwitchP4cBackend(
      table_mapper.get(), p4c_real_fe_me.get(), annotation_mapper.get(),
      bcm_tunnel_optimizer.get()));
  std::vector<BackendExtensionInterface*> extensions {extension.get()};
  std::unique_ptr<BackendPassManager> backend(
      new BackendPassManager(p4c_real_fe_me.get(), extensions));
  backend->Compile();
}
