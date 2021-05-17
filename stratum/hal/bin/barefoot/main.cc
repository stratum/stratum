// Copyright 2018-present Barefoot Networks, Inc.
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "PI/frontends/proto/device_mgr.h"
#include "PI/frontends/proto/logging.h"
#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"
#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"
#include "stratum/hal/lib/barefoot/bf_switch.h"
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

using ::pi::fe::proto::DeviceMgr;

namespace {

void registerDeviceMgrLogger() {
  using ::pi::fe::proto::LoggerConfig;
  using ::pi::fe::proto::LogWriterIface;
  class P4RuntimeLogger : public LogWriterIface {
    void write(Severity severity, const char* msg) override {
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

  // no longer done by the Barefoot SDE starting with 8.7.0
  DeviceMgr::init(256 /* max devices */);
  registerDeviceMgrLogger();

  // TODO(antonin): The SDE expects 0-based device ids, so we instantiate
  // components with "device_id" instead of "node_id". This works because
  // DeviceMgr does not do any device id checks.
  int device_id = 0;

  std::unique_ptr<DeviceMgr> device_mgr(new DeviceMgr(device_id));

  auto pi_node = pi::PINode::CreateInstance(device_mgr.get(), device_id);
  PhalInterface* phal_impl;
  if (FLAGS_bf_sim) {
    phal_impl = PhalSim::CreateSingleton();
  } else {
    phal_impl = phal::Phal::CreateSingleton();
  }
  std::map<int, pi::PINode*> device_id_to_pi_node = {
      {device_id, pi_node.get()},
  };
  auto bf_sde_wrapper = BfSdeWrapper::CreateSingleton();
  RETURN_IF_ERROR(bf_sde_wrapper->InitializeSde(
      FLAGS_bf_sde_install, FLAGS_bf_switchd_cfg, FLAGS_bf_switchd_background));
  ASSIGN_OR_RETURN(bool is_sw_model,
                   bf_sde_wrapper->IsSoftwareModel(device_id));
  const OperationMode mode =
      is_sw_model ? OPERATION_MODE_SIM : OPERATION_MODE_STANDALONE;
  VLOG(1) << "Detected is_sw_model: " << is_sw_model;
  VLOG(1) << "SDE version: " << bf_sde_wrapper->GetSdeVersion();
  auto bf_chassis_manager =
      BfChassisManager::CreateInstance(mode, phal_impl, bf_sde_wrapper);
  auto bf_switch =
      BfSwitch::CreateInstance(phal_impl, bf_chassis_manager.get(),
                               bf_sde_wrapper, device_id_to_pi_node);

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
