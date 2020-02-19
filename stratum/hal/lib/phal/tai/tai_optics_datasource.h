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

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/tai/tai_wrapper/tai_manager.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaiOpticsDataSource : public DataSource {
 public:
  static ::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>> Make(
      int id, tai::TAIManager* tai_manager,
      const PhalOpticalCardConfig& config);

  // Accessors for managed attributes.
  ManagedAttribute* GetModuleSlot() { return &module_slot_; }
  ManagedAttribute* GetModuleHardwareState() { return &module_hw_state_; }
  ManagedAttribute* GetModuleVendor() { return &card_vendor_; }

  ManagedAttribute* GetTxLaserFrequency() { return &tx_laser_frequency_; }
  ManagedAttribute* GetOperationalMode() { return &operational_mode_; }
  ManagedAttribute* GetOutputPower() { return &output_power_; }
  ManagedAttribute* GetCurrentOutputPower() { return &current_output_power_; }
  ManagedAttribute* GetInputPower() { return &input_power_; }

  // Setter functions.
  ::util::Status SetTxLaserFrequency(int slot, uint64 tx_laser_frequency);
  ::util::Status SetOperationalMode(int slot, uint64 operational_mode);
  ::util::Status SetOutputPower(int slot, double output_power);

 private:
  // Private constructor.
  TaiOpticsDataSource(int id, tai::TAIManager* tai_manager,
                      CachePolicy* cache_policy);

  ::util::Status UpdateValues() override;

  // Pointer to the Tai Manager. Not created or owned by this class.
  tai::TAIManager* tai_manager_;

  // Managed attributes.
  TypedAttribute<int> module_slot_{this};
  EnumAttribute module_hw_state_{HwState_descriptor(), this};
  TypedAttribute<std::string> card_vendor_{this};

  TypedAttribute<uint64> tx_laser_frequency_{this};
  TypedAttribute<uint64> operational_mode_{this};
  TypedAttribute<double> output_power_{this};
  TypedAttribute<double> current_output_power_{this};
  TypedAttribute<double> input_power_{this};
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
