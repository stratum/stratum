/*
 * Copyright 2020-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAISH_WRAPPER_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAISH_WRAPPER_H_

#include <memory>
#include <vector>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/phal/tai/tai_interface.h"
#include "taish/taish.grpc.pb.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaishWrapper : public stratum::hal::phal::tai::TaiInterface {
 public:
  // Initialize the TAI interface.
  util::Status Initialize() override EXCLUSIVE_LOCKS_REQUIRED(init_lock_);

  // Gets all module id.
  util::StatusOr<std::vector<uint64>> GetModuleIds() override;

  // Gets all network interface id from a module.
  util::StatusOr<std::vector<uint64>> GetNetworkInterfacesFromModule(
      const uint64 module_id) override;

  // Gets all host interface id from a module
  util::StatusOr<std::vector<uint64>> GetHostInterfacesFromModule(
      const uint64 module_id) override;

  // TODO(Yi): Complete functions for Module and Host Interface

  // Functions for Network Interface
  // Gets frequency from a network interface.
  util::StatusOr<uint64> GetFrequency(const uint64 netif_id) override;

  // Gets input power from a network interface.
  util::StatusOr<double> GetCurrentInputPower(const uint64 netif_id) override;

  // Gets output power from a network interface.
  util::StatusOr<double> GetCurrentOutputPower(const uint64 netif_id) override;

  // Gets target output power from a network interface.
  util::StatusOr<double> GetTargetOutputPower(const uint64 netif_id) override;

  // Gets modulation format from a network interface.
  util::StatusOr<uint64> GetModulationFormats(const uint64 netif_id) override;

  // Sets target output power to a network interafce.
  util::Status SetTargetOutputPower(const uint64 netif_id,
                                    const double power) override;

  // Sets modulation format to a network interface.
  util::Status SetModulationsFormats(const uint64 netif_id,
                                     const uint64 mod_format) override;

  // Gets the singleton instance.
  static TaishWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

 private:
  // Gets an attribute from a TAI object.
  util::StatusOr<std::string> GetAttribute(uint64 obj_id, uint64 attr_id);

  // Sets an attribute to a TAI object.
  util::Status SetAttribute(uint64 obj_id, uint64 attr_id, std::string value);

  static absl::Mutex init_lock_;
  static TaishWrapper* singleton_ GUARDED_BY(init_lock_);
  std::unique_ptr<taish::TAI::Stub> taish_stub_;
  bool initialized_ GUARDED_BY(init_lock_);

  // Caches object ids when we initialized the wrapper
  std::vector<uint64> modules_;
  std::vector<uint64> network_interfaces_;
  std::vector<uint64> host_interfaces_;

  // Caches attribute name and id when we initialized the wrapper
  absl::flat_hash_map<std::string, uint64> module_attr_map_;
  absl::flat_hash_map<std::string, uint64> network_interface_attr_map_;
  absl::flat_hash_map<std::string, uint64> host_interface_attr_map_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAISH_WRAPPER_H_
