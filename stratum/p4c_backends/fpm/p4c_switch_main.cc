// This file contains the main entry for the Hercules switch p4c backend.
// The most common switch use case is generating P4Info and a P4PipelineConfig
// from a P4 program. More information can be found here:
// platforms/networking/hercules/g3doc/p4c_backend_overview.md

#include <functional>
#include <memory>
#include <vector>

#include "base/init_google.h"
#include "platforms/networking/hercules/p4c_backend/bcm/bcm_target_info.h"
#include "platforms/networking/hercules/p4c_backend/bcm/bcm_tunnel_optimizer.h"
#include "platforms/networking/hercules/p4c_backend/common/backend_extension_interface.h"
#include "platforms/networking/hercules/p4c_backend/common/backend_pass_manager.h"
#include "platforms/networking/hercules/p4c_backend/common/p4c_front_mid_real.h"
#include "platforms/networking/hercules/p4c_backend/switch/midend.h"
#include "platforms/networking/hercules/p4c_backend/switch/switch_p4c_backend.h"
#include "platforms/networking/hercules/p4c_backend/switch/table_map_generator.h"
#include "platforms/networking/hercules/p4c_backend/switch/target_info.h"

using google::hercules::p4c_backend::AnnotationMapper;
using google::hercules::p4c_backend::BackendExtensionInterface;
using google::hercules::p4c_backend::BackendPassManager;
using google::hercules::p4c_backend::BcmTargetInfo;
using google::hercules::p4c_backend::BcmTunnelOptimizer;
using google::hercules::p4c_backend::MidEnd;
using google::hercules::p4c_backend::MidEndInterface;
using google::hercules::p4c_backend::P4cFrontMidReal;
using google::hercules::p4c_backend::SwitchP4cBackend;
using google::hercules::p4c_backend::TableMapGenerator;
using google::hercules::p4c_backend::TargetInfo;

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
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
