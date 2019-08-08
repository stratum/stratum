// Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_DUMMY_DUMMY_PHAL_H_
#define STRATUM_HAL_LIB_DUMMY_DUMMY_PHAL_H_

#include <memory>
#include <functional>
#include <utility>
#include <map>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/dummy/dummy_box.h"
#include "stratum/hal/lib/dummy/dummy_global_vars.h"
#include "stratum/hal/lib/dummy/dummy_phal.h"

namespace stratum {
namespace hal {
namespace dummy_switch {

class DummyPhal : public PhalInterface {
 public:
  // Methods from PhalInterface
  ~DummyPhal() override;
  ::util::Status PushChassisConfig(const ChassisConfig& config)
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::Status VerifyChassisConfig(const ChassisConfig& config)
  SHARED_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::Status Shutdown()
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::StatusOr<int> RegisterTransceiverEventWriter(
      std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
      int priority)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
      LOCKS_EXCLUDED(phal_lock_) override;
  ::util::Status UnregisterTransceiverEventWriter(int id)
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::Status GetFrontPanelPortInfo(
      int slot, int port, FrontPanelPortInfo* fp_port_info)
  SHARED_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::Status SetPortLedState(int slot, int port, int channel,
                                         LedColor color, LedState state)
  EXCLUSIVE_LOCKS_REQUIRED(chassis_lock)
  LOCKS_EXCLUDED(phal_lock_) override;

  ::util::Status RegisterSfpConfigurator(int slot, int port,
        ::stratum::hal::phal::SfpConfigurator* configurator) override;

  // Factory function for creating the instance of the DummyPhal.
  static DummyPhal* CreateSingleton() EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // DummyPhal is neither copyable nor movable.
  DummyPhal(const DummyPhal&) = delete;
  DummyPhal& operator=(const DummyPhal&) = delete;

 private:
  DummyPhal();
  int xcvr_event_writer_id_;
  DummyBox* dummy_box_;
  ::absl::Mutex phal_lock_;

  // Map from std::pair<int, int> representing (slot, port) of singleton port
  // to the vector of sfp datasource id
  std::map<std::pair<int, int>, ::stratum::hal::phal::SfpConfigurator*>
      slot_port_to_configurator_;
};

}  // namespace dummy_switch
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_DUMMY_DUMMY_PHAL_H_
