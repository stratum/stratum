/* Copyright 2018-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern "C" {

#include "bf_switchd/bf_switchd.h"

int switch_pci_sysfs_str_get(char *name, size_t name_size);

}

#include "gflags/gflags.h"
#include "PI/frontends/proto/device_mgr.h"
#include "PI/frontends/proto/logging.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_switch.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

using ::pi::fe::proto::DeviceMgr;

DEFINE_string(bf_sde_install, "",
              "absolute path to the directory where the BF SDE is installed");
DEFINE_bool(bf_switchd_background, false,
            "Run bf_switchd in the background with no interactive features");
DEFINE_string(bf_switchd_cfg, "stratum/hal/bin/barefoot/tofino_skip_p4.conf",
              "path to the BF switchd json config file");

namespace stratum {
namespace hal {
namespace barefoot {

int
Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);

  char bf_sysfs_fname[128];
  FILE *fd;

  bf_switchd_context_t *switchd_main_ctx = new bf_switchd_context_t;
  memset(switchd_main_ctx, 0, sizeof(bf_switchd_context_t));

  /* Parse bf_switchd arguments */
  if (FLAGS_bf_sde_install == "") {
    LOG(ERROR) << "Flag --bf_sde_install is required";
    return -1;
  }
  switchd_main_ctx->install_dir = strdup(FLAGS_bf_sde_install.c_str());
  switchd_main_ctx->conf_file = strdup(FLAGS_bf_switchd_cfg.c_str());
  switchd_main_ctx->skip_p4 = true;
  if (FLAGS_bf_switchd_background)
    switchd_main_ctx->running_in_background = true;
  else
    switchd_main_ctx->shell_set_ucli = true;

  /* determine if kernel mode packet driver is loaded */
  switch_pci_sysfs_str_get(bf_sysfs_fname,
                           sizeof(bf_sysfs_fname) - sizeof("/dev_add"));
  strncat(bf_sysfs_fname, "/dev_add", sizeof("/dev_add"));
  printf("bf_sysfs_fname %s\n", bf_sysfs_fname);
  fd = fopen(bf_sysfs_fname, "r");
  if (fd != NULL) {
    /* override previous parsing if bf_kpkt KLM was loaded */
    printf("kernel mode packet driver present, forcing kernel_pkt option!\n");
    switchd_main_ctx->kernel_pkt = true;
    fclose(fd);
  }

  {
    int status = bf_switchd_lib_init(switchd_main_ctx);
    if (status != 0) {
      LOG(ERROR) << "Error when starting switchd";
      return status;
    } else {
      LOG(INFO) << "switchd started successfully";
    }
  }

  // no longer done the Barefoot SDE starting with 8.7.0
  DeviceMgr::init(256 /* max devices */);

  int unit(0);
  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // DeviceMgr with "unit" instead of "node_id". This works because DeviceMgr
  // does not do any device id checks.
  std::unique_ptr<DeviceMgr> device_mgr(new DeviceMgr(unit));

  auto pi_node = pi::PINode::CreateInstance(device_mgr.get(), unit);
  auto* phal_sim = PhalSim::CreateSingleton();
  std::map<int, pi::PINode*> unit_to_pi_node = {
    {unit, pi_node.get()},
  };
  auto bf_chassis_manager = BFChassisManager::CreateInstance(phal_sim);
  auto bf_switch = BFSwitch::CreateInstance(
      phal_sim, bf_chassis_manager.get(), unit_to_pi_node);

  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  auto credentials_manager = CredentialsManager::CreateInstance();
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_STANDALONE,
                                   bf_switch.get(),
                                   auth_policy_checker.get(),
                                   credentials_manager.get());
  if (!hal) {
    LOG(ERROR) << "Failed to create the Hercules Hal instance.";
    return -1;
  }

  // Setup and start serving RPCs.
  ::util::Status status = hal->Setup();
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

int
main(int argc, char* argv[]) {
  return stratum::hal::barefoot::Main(argc, argv);
}
