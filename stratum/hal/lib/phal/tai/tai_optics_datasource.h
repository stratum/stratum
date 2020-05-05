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

#include "stratum/hal/lib/phal/tai/tai_interface.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

/*!
 * \brief TaiOpticsDataSource class updates Database<->TAI with fresh values
 */
class TaiOpticsDataSource final : public DataSource {
 public:
  static ::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>>
      Make(uint64 id, const PhalOpticalCardConfig& config,
           TaiInterface* tai_interface);

  // Accessors for managed attributes.
  ManagedAttribute* GetTxLaserFrequency() { return &tx_laser_frequency_; }
  ManagedAttribute* GetOperationalMode() { return &operational_mode_; }
  ManagedAttribute* GetTargetOutputPower() { return &target_output_power_; }
  ManagedAttribute* GetCurrentOutputPower() { return &current_output_power_; }
  ManagedAttribute* GetCurrentInputPower() { return &current_input_power_; }

  // Setter functions.
  ::util::Status SetTxLaserFrequency(uint64 tx_laser_frequency);
  ::util::Status SetOperationalMode(uint64 modulation_format);
  ::util::Status SetTargetOutputPower(double target_output_power);

 private:
  uint64 id_;
  // Private constructor.
  TaiOpticsDataSource(uint64 id, CachePolicy* cache_policy,
                      TaiInterface* tai_interface);

  ::util::Status UpdateValues() override;

  // Reference to the tai interface, the data source doesn't own this object.
  TaiInterface* tai_interface_;

  // Managed attributes.
  TypedAttribute<uint64> tx_laser_frequency_{this};
  TypedAttribute<uint64> operational_mode_{this};
  TypedAttribute<double> target_output_power_{this};
  TypedAttribute<double> current_output_power_{this};
  TypedAttribute<double> current_input_power_{this};
};

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
