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

#include "stratum/hal/lib/phal/tai/tai_optics_datasource.h"

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
 * \brief TaiOpticsDataSource::Make method \return constructed
 * TaiOpticsDataSource object
 *
 * \note Ownership is transferred to the caller
 */
::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>>
TaiOpticsDataSource::Make(uint64 id,
                          const PhalOpticalModuleConfig::Port& config,
                          TaiInterface* tai_interface) {
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  auto optics_datasource = std::shared_ptr<TaiOpticsDataSource>(
          new TaiOpticsDataSource(id, cache, tai_interface));

  return optics_datasource;
}

TaiOpticsDataSource::TaiOpticsDataSource(uint64 id, CachePolicy* cache_policy,
                                         TaiInterface* tai_interface)
    : id_(id), DataSource(cache_policy), tai_interface_(tai_interface) {
  // These values do not change during the lifetime of the data source.

  tx_laser_frequency_.AddSetter(
      [id, tai_interface](uint64 laser_frequency) -> ::util::Status {
        return tai_interface->SetTxLaserFrequency(id, laser_frequency);
      });
  operational_mode_.AddSetter(
      [id, tai_interface](uint64 operational_mode) -> ::util::Status {
        return tai_interface->SetModulationsFormats(id, operational_mode);
      });
  target_output_power_.AddSetter(
      [id, tai_interface](double output_power) -> ::util::Status {
        return tai_interface->SetTargetOutputPower(id, output_power);
      });
}

/*!
 * \brief TaiOpticsDataSource::UpdateValues method updates attributes with
 * fresh values from TAI.
 * \return ::util::OkStatus if success
 */
::util::Status TaiOpticsDataSource::UpdateValues() {
  // Update attributes with fresh values from Tai.
  ASSIGN_OR_RETURN(auto tx_laser_freq,
                   tai_interface_->GetTxLaserFrequency(id_));
  tx_laser_frequency_.AssignValue(tx_laser_freq);
  ASSIGN_OR_RETURN(auto op_mode,
                   tai_interface_->GetModulationFormats(id_));
  operational_mode_.AssignValue(op_mode);
  ASSIGN_OR_RETURN(auto current_output_power,
                   tai_interface_->GetCurrentOutputPower(id_));
  current_output_power_.AssignValue(current_output_power);
  ASSIGN_OR_RETURN(auto current_input_power,
                   tai_interface_->GetCurrentInputPower(id_));
  current_input_power_.AssignValue(current_input_power);
  ASSIGN_OR_RETURN(auto target_output_power,
                   tai_interface_->GetTargetOutputPower(id_));
  target_output_power_.AssignValue(target_output_power);
  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
