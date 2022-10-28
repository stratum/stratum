// Copyright 2018-2019 Barefoot Networks, Inc.
// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/bin/tdi/tofino/tofino_main.h"

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/hal/lib/tdi/tdi_action_profile_manager.h"
#include "stratum/hal/lib/tdi/tdi_counter_manager.h"
#include "stratum/hal/lib/tdi/tdi_node.h"
#include "stratum/hal/lib/tdi/tdi_pre_manager.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/hal/lib/tdi/tdi_table_manager.h"
#include "stratum/hal/lib/tdi/tofino/tofino_chassis_manager.h"
#include "stratum/hal/lib/tdi/tofino/tofino_hal.h"
#include "stratum/hal/lib/tdi/tofino/tofino_switch.h"
#include "stratum/lib/security/auth_policy_checker.h"

DEFINE_string(tdi_sde_install, "/usr",
              "Absolute path to the directory where the SDE is installed");
DEFINE_bool(tdi_switchd_background, false,
            "Run switch daemon in the background with no interactive features");
// TODO: Target-specific default.
DEFINE_string(tdi_switchd_cfg, "/usr/share/stratum/tofino_skip_p4.conf",
              "Path to the switch daemon json config file");

namespace stratum {
namespace hal {
namespace tdi {

::util::Status TofinoMain(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // components with "device_id" instead of "node_id".
  const int device_id = 0;

  auto sde_wrapper = TdiSdeWrapper::CreateSingleton();

  RETURN_IF_ERROR(sde_wrapper->InitializeSde(
      FLAGS_tdi_sde_install, FLAGS_tdi_switchd_cfg, FLAGS_tdi_switchd_background));

  ASSIGN_OR_RETURN(bool is_sw_model,
                   sde_wrapper->IsSoftwareModel(device_id));
  const OperationMode mode =
      is_sw_model ? OPERATION_MODE_SIM : OPERATION_MODE_STANDALONE;

  VLOG(1) << "Detected is_sw_model: " << is_sw_model;
  VLOG(1) << "SDE version: " << sde_wrapper->GetSdeVersion();
  VLOG(1) << "Switch SKU: " << sde_wrapper->GetChipType(device_id);

  auto table_manager =
      TdiTableManager::CreateInstance(mode, sde_wrapper, device_id);

  auto action_profile_manager =
      TdiActionProfileManager::CreateInstance(sde_wrapper, device_id);

  auto packetio_manager =
      TdiPacketioManager::CreateInstance(sde_wrapper, device_id);

  auto pre_manager =
      TdiPreManager::CreateInstance(sde_wrapper, device_id);

  auto counter_manager =
      TdiCounterManager::CreateInstance(sde_wrapper, device_id);

  auto tdi_node = TdiNode::CreateInstance(
      table_manager.get(), action_profile_manager.get(),
      packetio_manager.get(), pre_manager.get(),
      counter_manager.get(), sde_wrapper, device_id);

  PhalInterface* phal = PhalSim::CreateSingleton();

  std::map<int, TdiNode*> device_id_to_tdi_node = {
      {device_id, tdi_node.get()},
  };

  auto chassis_manager =
      TofinoChassisManager::CreateInstance(mode, phal, sde_wrapper);

  auto tdi_switch = TofinoSwitch::CreateInstance(
      chassis_manager.get(), device_id_to_tdi_node);

  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();

  // Create the 'Hal' class instance.
  auto* hal = TofinoHal::CreateSingleton(
      stratum::hal::OPERATION_MODE_STANDALONE, tdi_switch.get(),
      auth_policy_checker.get());
  CHECK_RETURN_IF_FALSE(hal) << "Failed to create the Stratum Hal instance.";

  // Set up P4 runtime servers.
  ::util::Status status = hal->Setup();
  if (!status.ok()) {
    LOG(ERROR)
        << "Error when setting up Stratum HAL (but we will continue running): "
        << status.error_message();
  }

  // Start serving RPCs.
  RETURN_IF_ERROR(hal->Run());  // blocking

  LOG(INFO) << "See you later!";
  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
