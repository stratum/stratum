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


#include "stratum/hal/lib/phal/phal_sim.h"

#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"
#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

PhalSim* PhalSim::singleton_ = nullptr;
#ifdef ABSL_KCONSTINIT //FIXME remove when kConstInit is upstreamed
ABSL_CONST_INIT absl::Mutex PhalSim::init_lock_(absl::kConstInit);
#else
absl::Mutex PhalSim::init_lock_;
#endif

PhalSim::PhalSim() {}

PhalSim::~PhalSim() {}

::util::Status PhalSim::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    // TODO: Implement this function.
    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status PhalSim::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);
  initialized_ = false;

  return ::util::OkStatus();
}

::util::StatusOr<int> PhalSim::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {
  // TODO: Implement this function.
  return kInvalidWriterId;
}

::util::Status PhalSim::UnregisterTransceiverEventWriter(int id) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::SetPortLedState(int slot, int port, int channel,
                                        LedColor color, LedState state) {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

PhalSim* PhalSim::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new PhalSim();
  }

  return singleton_;
}

}  // namespace hal
}  // namespace stratum

