/*
 * Copyright 2020-present Open Networking Foundation
 * Copyright 2020 PLVision
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/phal/attribute_database.h"
#include "stratum/hal/lib/phal/phal_backend_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// Class "TaiPhal" is an implementation of PhalBackendInterface which is used to
// send the TaiPhal events to Stratum.
class TaiPhal final : public PhalBackendInterface {
 public:
  ~TaiPhal() override;

  // PhalInterface public methods.
  ::util::Status PushChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status VerifyChassisConfig(const ChassisConfig& config) override
      LOCKS_EXCLUDED(config_lock_);
  ::util::Status Shutdown() override LOCKS_EXCLUDED(config_lock_);
  ::util::StatusOr<std::pair<uint32, uint32>> GetRelatedTAIModuleAndNetworkId(
      uint64 node_id, uint32 port_id) const LOCKS_EXCLUDED(config_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static TaiPhal* CreateSingleton()
      LOCKS_EXCLUDED(config_lock_, init_lock_);

  static void InitTAI();

  // TaiPhal is neither copyable nor movable.
  TaiPhal(const TaiPhal&) = delete;
  TaiPhal& operator=(const TaiPhal&) = delete;

 private:
  // Private constructor.
  TaiPhal();

  // Calls all the one time start initialisations
  ::util::Status Initialize()
      LOCKS_EXCLUDED(config_lock_);

  // Internal mutex lock for protecting the internal maps and initializing the
  // singleton instance.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static TaiPhal* singleton_ GUARDED_BY(init_lock_);

  // Mutex lock for protecting the internal state when config is pushed or the
  // class is initialized so that other threads do not access the state while
  // the are being changed.
  mutable absl::Mutex config_lock_;

  // Determines if PHAL is fully initialized.
  bool initialized_ GUARDED_BY(config_lock_) = false;

  // Map from Stratum port configs (node_id, port_id) to TAI identifiers
  // (module_id, netif_id) for the related optical transceiver plugged into that
  // port.
  std::map<std::pair<uint64, uint32>, std::pair<uint32, uint32>>
      node_port_id_to_module_netif_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_PHAL_H_
