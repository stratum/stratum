// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "absl/container/flat_hash_map.h"
#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_chassis_manager.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_switch.h"
#include "stratum/hal/lib/tdi/tdi_action_profile_manager.h"
#include "stratum/hal/lib/tdi/tdi_counter_manager.h"
#include "stratum/hal/lib/tdi/tdi_node.h"
#include "stratum/hal/lib/tdi/tdi_packetio_manager.h"
#include "stratum/hal/lib/tdi/tdi_pre_manager.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/hal/lib/tdi/tdi_table_manager.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

DEFINE_string(dpdk_sde_install, "/usr",
              "Absolute path to the directory where the SDE is installed");
DEFINE_bool(dpdk_switchd_background, false,
            "Run switchd in the background with no interactive features");
DEFINE_string(dpdk_switchd_cfg, "stratum/hal/bin/tdi/dpdk/dpdk_skip_p4.conf",
              "Path to the DPDK switchd json config file");

namespace stratum {
namespace hal {
namespace tdi {
namespace dpdk {

::util::Status Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // components with "device_id" instead of "node_id".
  const int device_id = 0;
  auto sde_wrapper = TdiSdeWrapper::CreateSingleton();
  RETURN_IF_ERROR(sde_wrapper->InitializeSde(FLAGS_dpdk_sde_install,
                                             FLAGS_dpdk_switchd_cfg,
                                             FLAGS_dpdk_switchd_background));
  /* ========== */
  // NOTE: Rework for DPDK
  ASSIGN_OR_RETURN(bool is_sw_model, sde_wrapper->IsSoftwareModel(device_id));
  const OperationMode mode =
      is_sw_model ? OPERATION_MODE_SIM : OPERATION_MODE_STANDALONE;

  VLOG(1) << "Detected is_sw_model: " << is_sw_model;
  VLOG(1) << "SDE version: " << sde_wrapper->GetSdeVersion();
  VLOG(1) << "Switch SKU: " << sde_wrapper->GetChipType(device_id);
  /* ========== */
  auto table_manager =
      TdiTableManager::CreateInstance(mode, sde_wrapper, device_id);
  auto action_profile_manager =
      TdiActionProfileManager::CreateInstance(sde_wrapper, device_id);
  auto packetio_manager =
      TdiPacketioManager::CreateInstance(sde_wrapper, device_id);
  auto pre_manager = TdiPreManager::CreateInstance(sde_wrapper, device_id);
  auto counter_manager =
      TdiCounterManager::CreateInstance(sde_wrapper, device_id);
  auto dpdk_node = TdiNode::CreateInstance(
      table_manager.get(), action_profile_manager.get(), packetio_manager.get(),
      pre_manager.get(), counter_manager.get(), sde_wrapper, device_id);
  absl::flat_hash_map<int, TdiNode*> device_id_to_dpdk_node = {
      {device_id, dpdk_node.get()},
  };
  auto chassis_manager = DpdkChassisManager::CreateInstance(mode, sde_wrapper);
  auto dpdk_switch = DpdkSwitch::CreateInstance(
      chassis_manager.get(), sde_wrapper, device_id_to_dpdk_node);

  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_STANDALONE,
                                   dpdk_switch.get(), auth_policy_checker.get(),
                                   credentials_manager.get());
  RET_CHECK(hal) << "Failed to create the Stratum Hal instance.";

  // Sanity check, setup and start serving RPCs.
  RETURN_IF_ERROR(hal->SanityCheck());

  ::util::Status status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR)
        << "Error when setting up Stratum HAL (but we will continue running): "
        << status.error_message();
  }
  RETURN_IF_ERROR(hal->Run());  // blocking

  LOG(INFO) << "See you later!";
  return ::util::OkStatus();
}

}  // namespace dpdk
}  // namespace tdi
}  // namespace hal
}  // namespace stratum

int main(int argc, char* argv[]) {
  return stratum::hal::tdi::dpdk::Main(argc, argv).error_code();
}
