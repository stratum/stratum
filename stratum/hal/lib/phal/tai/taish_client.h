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
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/phal/tai/tai_interface.h"
#include "taish/taish.grpc.pb.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// Attribute names.
const std::string kNetIfAttrTxLaserFreq =
    "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ";
const std::string kNetIfAttrCurrentInputPower =
    "TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER";
const std::string kNetIfAttrCurrentOutputPower =
    "TAI_NETWORK_INTERFACE_ATTR_CURRENT_OUTPUT_POWER";
const std::string kNetIfAttrOutputPower =
    "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER";
const std::string kNetIfAttrModulationFormat =
    "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT";

class TaishClient : public TaiInterface {
 public:
  util::Status Initialize() override EXCLUSIVE_LOCKS_REQUIRED(init_lock_);
  util::StatusOr<std::vector<uint64>> GetModuleIds() override;
  util::StatusOr<std::vector<uint64>> GetNetworkInterfaceIds(
      const uint64 module_id) override;

  util::StatusOr<std::vector<uint64>> GetHostInterfaceIds(
      const uint64 module_id) override;

  // TODO(Yi): Complete functions for Module and Host Interface

  // Functions for Network Interface.
  util::StatusOr<uint64> GetTxLaserFrequency(const uint64 netif_id) override;
  util::StatusOr<double> GetCurrentInputPower(const uint64 netif_id) override;
  util::StatusOr<double> GetCurrentOutputPower(const uint64 netif_id) override;
  util::StatusOr<double> GetTargetOutputPower(const uint64 netif_id) override;
  util::StatusOr<uint64> GetModulationFormats(const uint64 netif_id) override;
  util::Status SetTargetOutputPower(const uint64 netif_id,
                                    const double power) override;
  util::Status SetModulationsFormats(const uint64 netif_id,
                                     const uint64 mod_format) override;
  util::Status SetTxLaserFrequency(const uint64 netif_id,
                                   const uint64 frequency) override;
  virtual util::Status Shutdown() override;

  // Gets the singleton instance.
  static TaishClient* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

 private:
  // Gets an attribute from a TAI object.
  util::StatusOr<std::string> GetAttribute(uint64 obj_id, uint64 attr_id)
      EXCLUSIVE_LOCKS_REQUIRED(init_lock_);

  // Sets an attribute to a TAI object.
  util::Status SetAttribute(uint64 obj_id, uint64 attr_id, std::string value)
      EXCLUSIVE_LOCKS_REQUIRED(init_lock_);

  static absl::Mutex init_lock_;
  static TaishClient* singleton_ GUARDED_BY(init_lock_);
  std::unique_ptr<taish::TAI::Stub> taish_stub_;
  bool initialized_ GUARDED_BY(init_lock_);

  // Caches TAI modules when we initialized the wrapper
  std::vector<taish::Module> modules_;

  // Caches attribute name and id when we initialized the wrapper
  absl::flat_hash_map<std::string, uint64> module_attr_map_;
  absl::flat_hash_map<std::string, uint64> netif_attr_map_;
  absl::flat_hash_map<std::string, uint64> hostif_attr_map_;
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAISH_WRAPPER_H_
