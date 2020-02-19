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

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

TaiPhal* TaiPhal::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex TaiPhal::init_lock_(absl::kConstInit);

TaiPhal::TaiPhal()
    : tai_manager_(nullptr) {}

TaiPhal::~TaiPhal() {}

TaiPhal* TaiPhal::CreateSingleton(tai::TAIManager* tai_manager) {
  absl::WriterMutexLock l(&init_lock_);

  if (!singleton_) {
    singleton_ = new TaiPhal();
  }

  auto status = singleton_->Initialize(tai_manager);
  if (!status.ok()) {
    LOG(ERROR) << "TaiPhal failed to initialize: " << status;
    delete singleton_;
    singleton_ = nullptr;
  }

  return singleton_;
}

// Initialize the tai interface and phal DB
::util::Status TaiPhal::Initialize(tai::TAIManager* tai_manager) {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    CHECK_RETURN_IF_FALSE(tai_manager != nullptr);
    tai_manager_ = tai_manager;

    initialized_ = true;
  }
  return ::util::OkStatus();
}

::util::Status TaiPhal::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);

  return ::util::OkStatus();
}

::util::Status TaiPhal::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status TaiPhal::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);

  tai_manager_ = nullptr;
  initialized_ = false;

  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
