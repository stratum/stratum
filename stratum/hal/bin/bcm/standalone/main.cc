// Copyright 2018 Google LLC
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


#include <memory>

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/bcm_acl_manager.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/bcm_diag_shell.h"
#include "stratum/hal/lib/bcm/bcm_l2_manager.h"
#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "stratum/hal/lib/bcm/bcm_node.h"
#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"
#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "stratum/hal/lib/bcm/bcm_switch.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
// TODO(craigs): need to add real phal header files here
// #include "stratum/hal/lib/phal/legacy_phal.h"
// #include "stratum/hal/lib/phal/udev.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

DEFINE_int32(max_units, 1,
             "Maximum number of units supported on the switch platform.");

namespace stratum {
namespace hal {
namespace bcm {

// Encapsulates all the class instaces which are created per node (aka
// chip/ASIC/unit).
struct PerNodeInstances {
  std::unique_ptr<BcmAclManager> bcm_acl_manager;
  std::unique_ptr<BcmL2Manager> bcm_l2_manager;
  std::unique_ptr<BcmL3Manager> bcm_l3_manager;
  std::unique_ptr<BcmPacketioManager> bcm_packetio_manager;
  std::unique_ptr<BcmTableManager> bcm_table_manager;
  std::unique_ptr<BcmTunnelManager> bcm_tunnel_manager;
  std::unique_ptr<BcmNode> bcm_node;
  std::unique_ptr<P4TableMapper> p4_table_mapper;

  PerNodeInstances(BcmSdkInterface* bcm_sdk_interface,
                   BcmChassisManager* bcm_chassis_manager, int unit) {
    p4_table_mapper = P4TableMapper::CreateInstance();
    bcm_table_manager = BcmTableManager::CreateInstance(
        bcm_chassis_manager, p4_table_mapper.get(), unit);
    bcm_acl_manager = BcmAclManager::CreateInstance(
        bcm_chassis_manager, bcm_table_manager.get(), bcm_sdk_interface,
        p4_table_mapper.get(), unit);
    bcm_l2_manager = BcmL2Manager::CreateInstance(bcm_chassis_manager,
                                                  bcm_sdk_interface, unit);
    bcm_l3_manager = BcmL3Manager::CreateInstance(
        bcm_sdk_interface, bcm_table_manager.get(), unit);
    bcm_tunnel_manager = BcmTunnelManager::CreateInstance(
        bcm_sdk_interface, bcm_table_manager.get(), unit);
    bcm_packetio_manager = BcmPacketioManager::CreateInstance(
        OPERATION_MODE_STANDALONE, bcm_chassis_manager, p4_table_mapper.get(),
        bcm_sdk_interface, unit);
    bcm_node = BcmNode::CreateInstance(
        bcm_acl_manager.get(), bcm_l2_manager.get(), bcm_l3_manager.get(),
        bcm_packetio_manager.get(), bcm_table_manager.get(),
        bcm_tunnel_manager.get(), p4_table_mapper.get(), unit);
  }
};

int Main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitHerculesLogging();

  LOG(INFO)
      << "Starting Hercules in STANDALONE mode for a Broadcom-based switch...";

  // Create chassis-wide and per-node class instances.
  auto* bcm_diag_shell = BcmDiagShell::CreateSingleton();
  auto* bcm_sdk_wrapper = BcmSdkWrapper::CreateSingleton(bcm_diag_shell);
  // TODO(craigs) - use phal_sim temporarily until real phal used
  auto* phal_sim = PhalSim::CreateSingleton();
  auto bcm_serdes_db_manager = BcmSerdesDbManager::CreateInstance();
  auto bcm_chassis_manager = BcmChassisManager::CreateInstance(
      OPERATION_MODE_STANDALONE, phal_sim, bcm_sdk_wrapper,
      bcm_serdes_db_manager.get());
  std::vector<PerNodeInstances> per_node_instances;
  std::map<int, BcmNode*> unit_to_bcm_node;
  // We assume BCM ASICs have unit numbers {0,...,FLAGS_max_units-1}.
  for (int unit = 0; unit < FLAGS_max_units; ++unit) {
    per_node_instances.emplace_back(bcm_sdk_wrapper, bcm_chassis_manager.get(),
                                    unit);
    unit_to_bcm_node[unit] = per_node_instances[unit].bcm_node.get();
  }
  // Give BcmChassisManager the node map. This is needed to enable
  // BcmChassisManager to publish events which impact the per-node managers, as
  // those managers are passed a pointer to BcmChassisManager on creation.
  bcm_chassis_manager->SetUnitToBcmNodeMap(unit_to_bcm_node);
  // Create 'BcmSwitch' class instace.
  auto bcm_switch = BcmSwitch::CreateInstance(
      phal_sim, bcm_chassis_manager.get(), unit_to_bcm_node);
  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  auto credentials_manager = CredentialsManager::CreateInstance();
  auto* hal = Hal::CreateSingleton(OPERATION_MODE_STANDALONE, bcm_switch.get(),
                                   auth_policy_checker.get(),
                                   credentials_manager.get());
  CHECK(hal != nullptr) << "Failed to create the Hercules Hal instance.";

  // Sanity check, setup and start serving RPCs.
  ::util::Status status = hal->SanityCheck();
  if (!status.ok()) {
    LOG(ERROR) << "HAL sanity check failed: " << status.error_message();
    return -1;
  }
  status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR)
        << "Error when setting up Hercules HAL (but we will continue running): "
        << status.error_message();
  }
  status = hal->Run();  // blocking
  if (!status.ok()) {
    LOG(ERROR) << "Error when running Hercules HAL: " << status.error_message();
    return -1;
  }

  LOG(INFO) << "See you later!";
  return 0;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::bcm::Main(argc, argv);
}
