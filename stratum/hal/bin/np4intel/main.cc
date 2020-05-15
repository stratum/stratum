// Copyright 2018-present Barefoot Networks, Inc.
// Copyright 2019-present Dell EMC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "PI/frontends/proto/device_mgr.h"
#include "PI/frontends/proto/logging.h"
#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/bin/np4intel/dpdk_config.pb.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/np4intel/np4_switch.h"
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"
#include "stratum/lib/utils.h"
#include "targets/np4/device_mgr.h"

DEFINE_string(initial_pipeline, "stratum/hal/bin/np4intel/dummy.json",
              "Path to initial pipeline for Netcope (required for "
              "starting Netcope)");
DEFINE_uint32(cpu_port, 128,
              "Netcope port number for CPU port (used for packet I/O)");
DEFINE_bool(np4_sim, false, "Run with the NP4 simulator");
DEFINE_string(dpdk_config, "", "DPDK EAL init config file");

namespace stratum {
namespace hal {
namespace np4intel {

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

// Initialise the DPDK EAL
::util::Status DPDKEalInit() {
  std::string pgm_name = "stratum_np4intel";

  // Arguments vector
  std::vector<char*> argv;
  argv.push_back(strdup(pgm_name.c_str()));

  // Were we passed a dpdk config file
  ::stratum::hal::np4intel::DPDKConfig dpdk_config;
  if (!FLAGS_dpdk_config.empty()) {
    RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_dpdk_config, &dpdk_config));

    // create argv
    for (const auto& arg : dpdk_config.eal_args())
      argv.push_back(strdup(arg.c_str()));
  }
  argv.push_back(nullptr);

  if (dpdk_config.disabled()) {
    LOG(INFO) << "DPDK is disabled";
  } else {
    // Call the DPDK EAL init
    auto rc = ::pi::np4::DeviceMgr::DPDKInit(argv.size() - 1, &argv[0]);

    // Now log the return code message
    if (rc != 0) {
      RETURN_ERROR(ERR_INTERNAL) << "DPDK EAL Init failed";
    }
    LOG(INFO) << "DPDK EAL Init successful";
  }

  return ::util::OkStatus();
}

}  // namespace

::util::Status Main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  DeviceMgr::init(256 /* max devices */);
  registerDeviceMgrLogger();
  RETURN_IF_ERROR(DPDKEalInit());

  // Create Phal
  PhalInterface* phal_impl;
  if (FLAGS_np4_sim) {
    phal_impl = PhalSim::CreateSingleton();
  } else {
    phal_impl = phal::Phal::CreateSingleton();
  }

  auto np4_chassis_manager = NP4ChassisManager::CreateInstance(phal_impl);
  auto pi_switch =
      NP4Switch::CreateInstance(phal_impl, np4_chassis_manager.get());

  // Create the 'Hal' class instance.
  auto auth_policy_checker = AuthPolicyChecker::CreateInstance();
  ASSIGN_OR_RETURN(auto credentials_manager,
                   CredentialsManager::CreateInstance());
  auto* hal = Hal::CreateSingleton(stratum::hal::OPERATION_MODE_SIM,
                                   pi_switch.get(), auth_policy_checker.get(),
                                   credentials_manager.get());
  CHECK_RETURN_IF_FALSE(hal) << "Failed to create the Hal instance.";

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

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum

int main(int argc, char* argv[]) {
  return stratum::hal::np4intel::Main(argc, argv).error_code();
}
