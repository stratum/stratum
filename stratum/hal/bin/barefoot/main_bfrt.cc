// Copyright 2018-2019 Barefoot Networks, Inc.
// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_init.h"
#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"
#include "stratum/hal/lib/barefoot/bfrt_action_profile_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_node.h"
#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"
#include "stratum/hal/lib/barefoot/bfrt_switch.h"
#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

DEFINE_string(bf_sde_install, "/usr",
              "Absolute path to the directory where the BF SDE is installed");
DEFINE_bool(bf_switchd_background, false,
            "Run bf_switchd in the background with no interactive features");
DEFINE_string(bf_switchd_cfg, "stratum/hal/bin/barefoot/tofino_skip_p4.conf",
              "Path to the BF switchd json config file");
DEFINE_bool(bf_sim, false, "Run with the Tofino simulator");

namespace stratum {
namespace hal {
namespace barefoot {

::util::Status Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  // Initialize bf_switchd library.
  {
    CHECK_RETURN_IF_FALSE(FLAGS_bf_sde_install != "")
        << "Flag --bf_sde_install is required";
    int status = InitBfSwitchd(FLAGS_bf_sde_install.c_str(),
                               FLAGS_bf_switchd_cfg.c_str(),
                               FLAGS_bf_switchd_background);
    CHECK_RETURN_IF_FALSE(status == 0)
        << "Error when starting switchd, status: " << status;
    LOG(INFO) << "switchd started successfully";
  }

  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // components with "device_id" instead of "node_id".
  int device_id = 0;

  auto bf_sde_wrapper = BfSdeWrapper::CreateSingleton();
  ASSIGN_OR_RETURN(bool is_sw_model,
                   bf_sde_wrapper->IsSoftwareModel(device_id));
  const OperationMode mode =
      is_sw_model ? OPERATION_MODE_SIM : OPERATION_MODE_STANDALONE;
  VLOG(1) << "Detected is_sw_model: " << is_sw_model;
  VLOG(1) << "SDE version: " << bf_sde_wrapper->GetSdeVersion();

  auto bfrt_table_manager =
      BfrtTableManager::CreateInstance(mode, bf_sde_wrapper, device_id);
  auto bfrt_action_profile_manager =
      BfrtActionProfileManager::CreateInstance(bf_sde_wrapper, device_id);
  auto bfrt_packetio_manager =
      BfrtPacketioManager::CreateInstance(bf_sde_wrapper, device_id);
  auto bfrt_pre_manager =
      BfrtPreManager::CreateInstance(bf_sde_wrapper, device_id);
  auto bfrt_counter_manager =
      BfrtCounterManager::CreateInstance(bf_sde_wrapper, device_id);
  auto bfrt_node = BfrtNode::CreateInstance(
      bfrt_table_manager.get(), bfrt_action_profile_manager.get(),
      bfrt_packetio_manager.get(), bfrt_pre_manager.get(),
      bfrt_counter_manager.get(), bf_sde_wrapper, device_id);
  std::map<int, BfrtNode*> device_id_to_bfrt_node = {
      {device_id, bfrt_node.get()},
  };

  PhalInterface* phal_impl;
  if (FLAGS_bf_sim) {
    phal_impl = PhalSim::CreateSingleton();
  } else {
    phal_impl = phal::Phal::CreateSingleton();
  }

  auto bf_chassis_manager =
      BFChassisManager::CreateInstance(mode, phal_impl, bf_sde_wrapper);
  auto bf_switch =
      BfrtSwitch::CreateInstance(phal_impl, bf_chassis_manager.get(),
                                 bf_sde_wrapper, device_id_to_bfrt_node);

  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_STANDALONE,
                                   bf_switch.get(), auth_policy_checker.get(),
                                   credentials_manager.get());
  CHECK_RETURN_IF_FALSE(hal) << "Failed to create the Stratum Hal instance.";

  // Setup and start serving RPCs.
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

int main(int argc, char* argv[]) {
  return stratum::hal::barefoot::Main(argc, argv).error_code();
}
