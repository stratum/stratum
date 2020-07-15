// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <memory>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
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
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

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

::util::Status Main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  LOG(INFO)
      << "Starting Stratum in STANDALONE mode for a Broadcom-based switch...";

  // Create chassis-wide and per-node class instances.
  auto* bcm_diag_shell = BcmDiagShell::CreateSingleton();
  auto* bcm_sdk_wrapper = BcmSdkWrapper::CreateSingleton(bcm_diag_shell);
  auto* phal = phal::Phal::CreateSingleton();
  auto bcm_serdes_db_manager = BcmSerdesDbManager::CreateInstance();
  auto bcm_chassis_manager = BcmChassisManager::CreateInstance(
      OPERATION_MODE_STANDALONE, phal, bcm_sdk_wrapper,
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
  auto bcm_switch = BcmSwitch::CreateInstance(phal, bcm_chassis_manager.get(),
                                              unit_to_bcm_node);
  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(OPERATION_MODE_STANDALONE, bcm_switch.get(),
                                   auth_policy_checker.get(),
                                   credentials_manager.get());
  CHECK_RETURN_IF_FALSE(hal) << "Failed to create the Stratum Hal instance.";

  // Sanity check, setup and start serving RPCs.
  RETURN_IF_ERROR(hal->SanityCheck());

  auto status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR)
        << "Error when setting up Stratum HAL (but we will continue running): "
        << status.error_message();
  }
  RETURN_IF_ERROR(hal->Run());  // blocking

  LOG(INFO) << "See you later!";
  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::bcm::Main(argc, argv).error_code();
}
