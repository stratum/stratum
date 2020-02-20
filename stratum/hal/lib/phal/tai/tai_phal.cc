// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
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

#include "stratum/hal/lib/phal/tai/tai_phal.h"

#include <string>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/macros.h"

DEFINE_string(taimux_config_file, "", "The TAI MUX configuration file");

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaiPhal* TaiPhal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaiPhal::init_lock_(absl::kConstInit);

TaiPhal::TaiPhal() {}

TaiPhal::~TaiPhal() {}

TaiPhal* TaiPhal::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new TaiPhal();
  }

  return singleton_;
}

// Initialize the tai interface and phal DB
::util::Status TaiPhal::Initialize() {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    initialized_ = true;
  }
  return ::util::OkStatus();
}

// Initialize the "MUX TAI" library.
// The "--taimux_config_file" argument should be specified to provide the config
// location TAI MUX internals.
//
// Find the full documentation and HOWTOs in the official TAI repository:
// https://github.com/Telecominfraproject/oopt-tai-implementations/tree/master
// /tai_mux#static-platform-adapter.
void TaiPhal::InitTAI() {
  // Set platform adapter type.
  setenv("TAI_MUX_PLATFORM_ADAPTER", "static", true);

  // If passed to Stratum - set the TAI MUX config file.
  if (!FLAGS_taimux_config_file.empty())
    setenv(
      "TAI_MUX_STATIC_CONFIG_FILE", FLAGS_taimux_config_file.c_str(), true);
}

::util::Status TaiPhal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);

  return ::util::OkStatus();
}

::util::Status TaiPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

// Get TAI module and network identifiers related to the specific node and port
// (or an error).
::util::StatusOr<std::pair<uint32, uint32>>
TaiPhal::GetRelatedTAIModuleAndNetworkId(
    uint64 node_id, uint32 port_id) const {
  absl::WriterMutexLock l(&config_lock_);
  auto iter = node_port_id_to_module_netif_.find({node_id, port_id});
  if (iter == node_port_id_to_module_netif_.end())
    return MAKE_ERROR(ERR_INTERNAL)
        << "No related TAI module is found for "
        << "node_id=" << node_id
        << ", "
        << "port_id" << port_id;

  return iter->second;
}

::util::Status TaiPhal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  initialized_ = false;

  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
