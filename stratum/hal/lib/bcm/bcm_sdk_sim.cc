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


#include "third_party/stratum/hal/lib/bcm/bcm_sdk_sim.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "third_party/absl/synchronization/mutex.h"

#include "base/commandlineflags.h"
#include "third_party/stratum/glue/logging.h"
#include "third_party/stratum/hal/lib/bcm/macros.h"
#include "third_party/stratum/lib/macros.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/absl/strings/substitute.h"
#include "util/gtl/map_util.h"
#include "util/gtl/stl_util.h"

#define MAX_SIM_ARGS 20
#define MAX_WAIT_TIME_TO_TERM_SIM_SECS 3

namespace stratum {
namespace hal {
namespace bcm {

namespace {

// Reads the simulator server output to extract rpc port simulator server is
// listening to.
int GetSimServerRpcPort(FILE *fp, const int bin_path_len) {
  char *str;
  int port = -1;
  // Allocate buffer that is large enough hold the line we want, which
  // looks like:
  // ${pcid_path}: Emulating ${chip}, listening on SOC_TARGET_PORT ${port}\n
  // see ${SDK}/systems/sim/pcid/pcidappl.c
  int buffer_len = bin_path_len + 100;
  char *buffer = reinterpret_cast<char *>(malloc(buffer_len));
  while (fgets(buffer, buffer_len, fp)) {
    str = strstr(buffer, "SOC_TARGET_PORT");
    if (!str) continue;
    if (sscanf(str, "SOC_TARGET_PORT %d", &port) == 1) {
      break;
    }
  }
  free(buffer);
  return port;
}

}  // namespace

BcmSdkSim::BcmSdkSim(const std::string& bcm_sdk_sim_bin)
    : BcmSdkWrapper(nullptr),
      unit_to_dev_info_(),
      bcm_sdk_sim_bin_(bcm_sdk_sim_bin) {}

BcmSdkSim::~BcmSdkSim() { ShutdownAllUnits().IgnoreError(); }

::util::Status BcmSdkSim::InitializeSdk(
    const std::string &config_file_path,
    const std::string &config_flush_file_path,
    const std::string &bcm_shell_log_file_path) {
  // TODO: hardcode one simulator instance for now. Later get the
  // map from config_file_path.
  std::map<int, BcmChip::BcmChipType> unit_to_type = {{0, BcmChip::TRIDENT2}};

  // Set environment variables and spawn all the simulator processes. Then
  // initialize the SDK.
  std::string num_chips = absl::Substitute("$0", unit_to_type.size());
  setenv("BCM_CONFIG_FILE", config_file_path.c_str(), 1);
  setenv("SOC_TARGET_COUNT", num_chips.c_str(), 1);
  setenv("SOC_TARGET_SERVER", "localhost", 1);
  for (const auto &e : unit_to_type) {
    RETURN_IF_ERROR(InitializeSim(e.first, e.second));
  }
  RETURN_IF_ERROR(BcmSdkWrapper::InitializeSdk(
      config_file_path, config_flush_file_path, bcm_shell_log_file_path));

  return ::util::OkStatus();
}

::util::Status BcmSdkSim::FindUnit(int unit, int pci_bus, int pci_slot,
                                   BcmChip::BcmChipType chip_type) {
  {
    absl::WriterMutexLock l(&sim_lock_);
    BcmSimDeviceInfo *info = gtl::FindPtrOrNull(unit_to_dev_info_, unit);
    CHECK_RETURN_IF_FALSE(info != nullptr) << "Unit " << unit << " not found!";
    CHECK_RETURN_IF_FALSE(info->chip_type == chip_type)
        << "Inconsistent state. Unit " << unit << " must be " << chip_type
        << " but got " << chip_type;
    info->pci_bus = pci_bus;
    info->pci_slot = pci_slot;
  }
  RETURN_IF_ERROR(BcmSdkWrapper::FindUnit(unit, pci_bus, pci_slot, chip_type));

  return ::util::OkStatus();
}

::util::Status BcmSdkSim::ShutdownAllUnits() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, BcmSdkWrapper::ShutdownAllUnits());
  APPEND_STATUS_IF_ERROR(status, ShutdownAllSimProcesses());

  return status;
}

::util::Status BcmSdkSim::StartLinkscan(int unit) {
  LOG(WARNING) << "Skipped starting linkscan in sim mode.";
  return ::util::OkStatus();  // NOOP
}

::util::Status BcmSdkSim::StopLinkscan(int unit) {
  LOG(WARNING) << "Skipped stopping linkscan in sim mode.";
  return ::util::OkStatus();  // NOOP
}

::util::Status BcmSdkSim::DeleteL2EntriesByVlan(int unit, int vlan) {
  // TODO: Implement this function.
  LOG(WARNING) << "Skipped DeleteL2EntriesByVlan in sim mode.";
  return ::util::OkStatus();
}

::util::Status BcmSdkSim::CreateKnetIntf(int unit, int vlan,
                                         std::string *netif_name,
                                         int *netif_id) {
  *netif_name = absl::Substitute("fake-knet-intf-$0", unit + 1);
  *netif_id = unit + 1;
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::DestroyKnetIntf(int unit, int netif_id) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::StatusOr<int> BcmSdkSim::CreateKnetFilter(int unit, int netif_id,
                                                  KnetFilterType type) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::DestroyKnetFilter(int unit, int filter_id) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::StartRx(int unit, const RxConfig &rx_config) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::StopRx(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::SetRateLimit(
    int unit, const RateLimitConfig &rate_limit_config) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::GetKnetHeaderForDirectTx(int unit, int port, int cos,
                                                   uint64 smac,
                                                   std::string *header) {
  *header = "";
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

::util::Status BcmSdkSim::GetKnetHeaderForIngressPipelineTx(
    int unit, uint64 smac, std::string *header) {
  *header = "";
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

size_t BcmSdkSim::GetKnetHeaderSizeForRx(int unit) { return 0; }

::util::Status BcmSdkSim::ParseKnetHeaderForRx(int unit,
                                               const std::string &header,
                                               int *ingress_logical_port,
                                               int *egress_logical_port,
                                               int *cos) {
  *ingress_logical_port = -1;
  *egress_logical_port = -1;
  *cos = -1;
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported in sim mode.";
}

BcmSdkSim *BcmSdkSim::CreateSingleton(const std::string& bcm_sdk_sim_bin) {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BcmSdkSim(bcm_sdk_sim_bin);
  }

  return static_cast<BcmSdkSim *>(singleton_);
}

::util::Status BcmSdkSim::GetPciInfo(int unit, uint32 *bus, uint32 *slot) {
  absl::ReaderMutexLock l(&sim_lock_);
  BcmSimDeviceInfo *info = gtl::FindPtrOrNull(unit_to_dev_info_, unit);
  CHECK_RETURN_IF_FALSE(info != nullptr) << "Unit " << unit << " not found!";
  *bus = static_cast<uint32>(info->pci_bus);
  *slot = static_cast<uint32>(info->pci_slot);

  return ::util::OkStatus();
}

::util::Status BcmSdkSim::CleanupKnet(int unit) {
  return ::util::OkStatus();  // NOOP
}

::util::Status BcmSdkSim::InitializeSim(int unit,
                                        BcmChip::BcmChipType chip_type) {
  // Bring up the simulator based on the given chip type.
  // Simulator Command Syntax:
  // <path>/pcid.sim <chip_name> -p<rpc_port> -R<revision_id> -D<device_id>
  // sim_args must have these arguments with a nullptr at the end.
  // chip_name is required. RPC port is randomly selected if unspecified.
  // Revision and Device ID have default values for each chip.
  const char *argv[MAX_SIM_ARGS];
  argv[0] = bcm_sdk_sim_bin_.c_str();
  switch (chip_type) {
    case BcmChip::TRIDENT2:
      argv[1] = "BCM56850_A0";
      argv[2] = "-p0";
      argv[3] = "-R3";
      argv[4] = nullptr;
      break;
    case BcmChip::TRIDENT_PLUS:
      argv[1] = "BCM56840_A0";
      argv[2] = "-p0";
      argv[3] = "-R1";
      argv[4] = "-D0xb846";
      argv[5] = nullptr;
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Unsupported Chip Type "
                                      << BcmChip::BcmChipType_Name(chip_type);
  }

  // Spawn the sim process.
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid < 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error in fork(): " << pid << ".";
  }
  if (pid == 0) {
    // Child -- simulator process
    close(fds[0]);
    dup2(fds[1], fileno(stdout));  // Redirect standard output for child.
    execv(bcm_sdk_sim_bin_.c_str(),
          const_cast<char *const *>(argv));  // blocking
    if (errno != 0) {
      LOG(ERROR) << "Simulator process ends with error: " << strerror(errno);
    }
    exit(0);
  } else {
    // Parent -- main process
    close(fds[1]);
    // Read the output of the simulator server
    FILE *fp = fdopen(fds[0], "r");

    if (fp == nullptr) {
      return MAKE_ERROR(ERR_INTERNAL) << "Unable to run simulator.";
    }
    // Extract server port. If hang here, the server is not up.
    int rpc_port = GetSimServerRpcPort(fp, bcm_sdk_sim_bin_.size());
    if (rpc_port <= 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Unable to find the RPC port for the simulator.";
    }
    // Set rpc port environment variable so the client can connect to.
    // SOC_TARGET_PORT0 for unit 0, and so on.
    std::string port_env_name = absl::Substitute("SOC_TARGET_PORT$0", unit);
    std::string port = absl::StrCat(rpc_port);
    setenv(port_env_name.c_str(), port.c_str(), 1);
    LOG(INFO) << "Sim for unit " << unit << " is listening on port " << rpc_port
              << " and has PID " << pid << ".";
    // Keep track of the RPC port and PID for the simulator.
    absl::WriterMutexLock l(&sim_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_dev_info_.count(unit) == 0)
        << "Unit " << unit << " already exists!";
    unit_to_dev_info_[unit] = new BcmSimDeviceInfo();
    unit_to_dev_info_[unit]->chip_type = chip_type;
    unit_to_dev_info_[unit]->rpc_port = rpc_port;
    unit_to_dev_info_[unit]->pid = pid;
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkSim::ShutdownAllSimProcesses() {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
