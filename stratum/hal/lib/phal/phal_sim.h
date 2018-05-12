/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_
#define STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_

#include <functional>

#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

// Class "PhalSim" is an implementation of PhalInterface which is used to
// simulate the PHAL events to Hercules.
class PhalSim : public PhalInterface {
 public:
  ~PhalSim() override;

  // PhalInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(config_lock_);
  ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority) override LOCKS_EXCLUDED(config_lock_);
  ::util::Status UnregisterTransceiverEventWriter(int id) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status GetFrontPanelPortInfo(
      int slot, int port, FrontPanelPortInfo* fp_port_info) override
      LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static PhalSim* CreateSingleton() LOCKS_EXCLUDED(config_lock_);

  // PhalSim is neither copyable nor movable.
  PhalSim(const PhalSim&) = delete;
  PhalSim& operator=(const PhalSim&) = delete;

 private:
  // Private constructor.
  PhalSim();

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static PhalSim* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if PHAL is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_PHAL_SIM_H_
