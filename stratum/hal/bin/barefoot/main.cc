// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

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
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_pal_wrapper.h"
#include "stratum/hal/lib/barefoot/bf_pd_wrapper.h"
#include "stratum/hal/lib/barefoot/bf_switch.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

using ::pi::fe::proto::DeviceMgr;

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

namespace {

void registerDeviceMgrLogger() {
  using ::pi::fe::proto::LogWriterIface;
  using ::pi::fe::proto::LoggerConfig;
  class P4RuntimeLogger : public LogWriterIface {
    void write(Severity severity, const char *msg) override {
      ::google::LogSeverity new_severity = INFO;
      switch (severity) {
        case Severity::TRACE:
          if (!VLOG_IS_ON(3)) return;
          new_severity = INFO;
          break;
        case Severity::DEBUG:
          if (!VLOG_IS_ON(1)) return;
          new_severity = INFO;
          break;
        case Severity::INFO:
          new_severity = INFO;
          break;
        case Severity::WARN:
          new_severity = WARNING;
          break;
        case Severity::ERROR:
          new_severity = ERROR;
          break;
        case Severity::CRITICAL:
          new_severity = FATAL;
          break;
      }

      // we use google::LogMessage directly (instead of the LOG macro) so we can
      // control the file name and line number displayed in the logs. Probably
      // not ideal but stratum/glue/status/status.h already does the same thing.
      constexpr char kDummyFile[] = "PI-device_mgr.cpp";
      constexpr int kDummyLine = 0;
      google::LogMessage log_message(kDummyFile, kDummyLine, new_severity);
      log_message.stream() << msg;
    }
  };
  LoggerConfig::set_writer(std::make_shared<P4RuntimeLogger>());
}

}  // namespace

::util::Status Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  char bf_sysfs_fname[128];
  FILE *fd;

  bf_switchd_context_t *switchd_main_ctx = new bf_switchd_context_t;
  memset(switchd_main_ctx, 0, sizeof(bf_switchd_context_t));

  /* Parse bf_switchd arguments */
  CHECK_RETURN_IF_FALSE(FLAGS_bf_sde_install != "")
      << "Flag --bf_sde_install is required";
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
    CHECK_RETURN_IF_FALSE(status == 0)
        << "Error when starting switchd, status: " << status;
    LOG(INFO) << "switchd started successfully";
  }

  // no longer done by the Barefoot SDE starting with 8.7.0
  DeviceMgr::init(256 /* max devices */);
  registerDeviceMgrLogger();

  int unit(0);
  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // DeviceMgr with "unit" instead of "node_id". This works because DeviceMgr
  // does not do any device id checks.
  std::unique_ptr<DeviceMgr> device_mgr(new DeviceMgr(unit));

  auto pi_node = pi::PINode::CreateInstance(device_mgr.get(), unit);
  PhalInterface* phal_impl;
  if (FLAGS_bf_sim) {
    phal_impl = PhalSim::CreateSingleton();
  } else {
    phal_impl = phal::Phal::CreateSingleton();
  }
  std::map<int, pi::PINode*> unit_to_pi_node = {
    {unit, pi_node.get()},
  };
  auto bf_chassis_manager = BFChassisManager::CreateInstance(
      phal_impl, BFPalWrapper::GetSingleton());
  auto bf_switch = BFSwitch::CreateInstance(
      phal_impl, bf_chassis_manager.get(), BFPdWrapper::GetSingleton(),
      unit_to_pi_node);

  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_STANDALONE,
                                   bf_switch.get(),
                                   auth_policy_checker.get(),
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
