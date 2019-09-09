/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_SIM_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_SIM_H_

#include <map>
#include <string>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"
#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// Holds all the info on all the attached simulated devices. Each device
// corresponds to an instance of the simulator process whose PID we save here.
struct BcmSimDeviceInfo {
  BcmChip::BcmChipType chip_type;
  int pci_bus;
  int pci_slot;
  int rpc_port;
  int pid;
  BcmSimDeviceInfo() : pci_bus(-1), pci_slot(-1), rpc_port(-1), pid(-1) {}
};

// The "BcmSdkSim" is an implementation of BcmSdkInterface, derived from
// BcmSdkWrapper, which is used to test most of the APIs in BcmSdkWrapper on
// simulated ASICs.
class BcmSdkSim : public BcmSdkWrapper {
 public:
  explicit BcmSdkSim(const std::string& bcm_sdk_sim_bin);
  ~BcmSdkSim() override;

  // Overloaded version of BcmSdkWrapper public methods for simulator.
  ::util::Status InitializeSdk(
      const std::string& config_file_path,
      const std::string& config_flush_file_path,
      const std::string& bcm_shell_log_file_path) override;
  ::util::Status FindUnit(int unit, int pci_bus, int pci_slot,
                          BcmChip::BcmChipType chip_type) override
      LOCKS_EXCLUDED(sim_lock_);
  ::util::Status ShutdownAllUnits() override LOCKS_EXCLUDED(sim_lock_);
  ::util::Status StartLinkscan(int unit) override;
  ::util::Status StopLinkscan(int unit) override;
  ::util::Status DeleteL2EntriesByVlan(int unit, int vlan) override;
  ::util::Status CreateKnetIntf(int unit, int vlan, std::string* netif_name,
                                int* netif_id) override;
  ::util::Status DestroyKnetIntf(int unit, int netif_id) override;
  ::util::StatusOr<int> CreateKnetFilter(int unit, int netif_id,
                                         KnetFilterType type) override;
  ::util::Status DestroyKnetFilter(int unit, int filter_id) override;
  ::util::Status StartRx(int unit, const RxConfig& rx_config) override;
  ::util::Status StopRx(int unit) override;
  ::util::Status SetRateLimit(
      int unit, const RateLimitConfig& rate_limit_config) override;
  ::util::Status GetKnetHeaderForDirectTx(int unit, int port, int cos,
                                          uint64 smac,
                                          std::string* header) override;
  ::util::Status GetKnetHeaderForIngressPipelineTx(
      int unit, uint64 smac, std::string* header) override;
  size_t GetKnetHeaderSizeForRx(int unit) override;
  ::util::Status ParseKnetHeaderForRx(int unit, const std::string& header,
                                      int* ingress_logical_port,
                                      int* egress_logical_port,
                                      int* cos) override;

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BcmSdkSim* CreateSingleton(const std::string& bcm_sdk_sim_bin)
      LOCKS_EXCLUDED(init_lock_);

  // The following public functions are specific to this class. They are to be
  // called by SDK callbacks or functions defined in the private namespace in
  // the .cc file only.

  // Find PCI info for a simulated BDE device. To be called in the SDK method
  // linux_bde_get_pci_info.
  ::util::Status GetPciInfo(int unit, uint32* bus, uint32* slot)
      LOCKS_EXCLUDED(sim_lock_);

 protected:
  // Overloaded version of BcmSdkWrapper protected methods for simulator.
  ::util::Status CleanupKnet(int unit) override;

 private:
  // Brings up the simulator based on the given chip type.
  ::util::Status InitializeSim(int unit, BcmChip::BcmChipType chip_type)
      LOCKS_EXCLUDED(sim_lock_);

  // Kills all the simulator processes. To be called in ShutdownAllUnits.
  ::util::Status ShutdownAllSimProcesses() LOCKS_EXCLUDED(sim_lock_);

  // RW mutex lock for protecting the internal simulator data structures.
  mutable absl::Mutex sim_lock_;

  // An internal map of dev_num of the simulated device (which is identical
  // to unit number) to the BcmSimDeviceInfo holding the info on this device.
  std::map<int, BcmSimDeviceInfo*> unit_to_dev_info_ GUARDED_BY(sim_lock_);

  // Path to the BCMSIM or PCID binary.
  std::string bcm_sdk_sim_bin_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_SIM_H_
