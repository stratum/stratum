// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

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

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaiPhal* TaiPhal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaiPhal::init_lock_(absl::kConstInit);

TaiPhal::TaiPhal(TaiInterface* tai_interface) : tai_interface_(tai_interface) {}

TaiPhal::~TaiPhal() {}

TaiPhal* TaiPhal::CreateSingleton(TaiInterface* tai_interface) {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new TaiPhal(tai_interface);
  }

  auto status = singleton_->Initialize();
  if (!status.ok()) {
    LOG(ERROR) << "TaiPhal failed to initialize: " << status;
    delete singleton_;
    singleton_ = nullptr;
  }

  return singleton_;
}

::util::Status TaiPhal::Initialize() {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    initialized_ = true;
  }
  return ::util::OkStatus();
}

::util::Status TaiPhal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);

  // Initialize optical network interfaces
  for (const auto& netif : config.optical_network_interfaces()) {
    uint64 oid = netif.id();
    RETURN_IF_ERROR(
        tai_interface_->SetTxLaserFrequency(oid, netif.frequency()));
    RETURN_IF_ERROR(
        tai_interface_->SetTargetOutputPower(oid, netif.target_output_power()));
    RETURN_IF_ERROR(
        tai_interface_->SetModulationFormat(oid, netif.operational_mode()));
  }
  return ::util::OkStatus();
}

::util::Status TaiPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status TaiPhal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  tai_interface_->Shutdown();
  initialized_ = false;

  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
