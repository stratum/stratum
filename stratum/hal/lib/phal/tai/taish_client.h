// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_

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
constexpr char kNetIfAttrTxLaserFreq[] =
    "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ";
constexpr char kNetIfAttrCurrentInputPower[] =
    "TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER";
constexpr char kNetIfAttrCurrentOutputPower[] =
    "TAI_NETWORK_INTERFACE_ATTR_CURRENT_OUTPUT_POWER";
constexpr char kNetIfAttrOutputPower[] =
    "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER";
constexpr char kNetIfAttrModulationFormat[] =
    "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT";

// FIXME(Yi): this map is based on ONOS ODTH driver, we should complete this
// map with values from the TAI library.
const absl::flat_hash_map<std::string, uint64> kModulationFormatIds = {
    {"dp-qpsk", 1},
    {"dp-16-qam", 2},
    {"dp-8-qam", 3},
};

class TaishClient final : public TaiInterface {
 public:
  util::Status Initialize() override EXCLUSIVE_LOCKS_REQUIRED(init_lock_);
  util::StatusOr<std::vector<uint64>> GetModuleIds() override;
  util::StatusOr<std::vector<uint64>> GetNetworkInterfaceIds(
      const uint64 module_id) override;

  util::StatusOr<std::vector<uint64>> GetHostInterfaceIds(
      const uint64 module_id) override;

  // TODO(Yi): Complete functions for Module and Host Interface

  // Functions for Network Interface.
  util::StatusOr<uint64> GetTxLaserFrequency(const uint64 netif_id) override
      LOCKS_EXCLUDED(init_lock_);
  util::StatusOr<double> GetCurrentInputPower(const uint64 netif_id) override
      LOCKS_EXCLUDED(init_lock_);
  util::StatusOr<double> GetCurrentOutputPower(const uint64 netif_id) override
      LOCKS_EXCLUDED(init_lock_);
  util::StatusOr<double> GetTargetOutputPower(const uint64 netif_id) override
      LOCKS_EXCLUDED(init_lock_);
  util::StatusOr<uint64> GetModulationFormat(const uint64 netif_id) override
      LOCKS_EXCLUDED(init_lock_);
  util::Status SetTargetOutputPower(const uint64 netif_id,
                                    const double power) override
      LOCKS_EXCLUDED(init_lock_);
  util::Status SetModulationFormat(const uint64 netif_id,
                                   const uint64 mod_format) override
      LOCKS_EXCLUDED(init_lock_);
  util::Status SetTxLaserFrequency(const uint64 netif_id,
                                   const uint64 frequency) override
      LOCKS_EXCLUDED(init_lock_);
  util::Status Shutdown() override LOCKS_EXCLUDED(init_lock_);

  // Gets the singleton instance.
  static TaishClient* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

 private:
  TaishClient();

  // Gets an attribute from a TAI object.
  util::StatusOr<std::string> GetAttribute(uint64 obj_id, uint64 attr_id)
      SHARED_LOCKS_REQUIRED(init_lock_);

  // Sets an attribute to a TAI object.
  util::Status SetAttribute(uint64 obj_id, uint64 attr_id, std::string value)
      SHARED_LOCKS_REQUIRED(init_lock_);

  util::StatusOr<uint64> GetModulationFormatIds(
      const std::string& modulation_format);
  util::StatusOr<std::string> GetModulationFormatName(const uint64 id);

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

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAISH_CLIENT_H_
