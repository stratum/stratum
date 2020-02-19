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

#include <cmath>

#include "absl/memory/memory.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/tai/types_converter.h"
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
TaiOpticsDataSource::Make(int id, const PhalOpticalCardConfig& config) {
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  auto optics_datasource =
      std::shared_ptr<TaiOpticsDataSource>(new TaiOpticsDataSource(id, cache));

  return optics_datasource;
}

TaiOpticsDataSource::TaiOpticsDataSource(int id, CachePolicy* cache_policy)
    : DataSource(cache_policy),  // note server address is hardcoded
      taish_client_(grpc::CreateChannel("localhost:50051",
                                        grpc::InsecureChannelCredentials())) {
  // These values do not change during the lifetime of the data source.
  module_slot_.AssignValue(id);

  tx_laser_frequency_.AddSetter(
      [this](uint64 requeste_tx_laser_frequency) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetTxLaserFrequency(slot, requeste_tx_laser_frequency);
      });
  modulation_format_.AddSetter(
      [this](uint64 requeste_modulation_format) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetModulationFormat(slot, requeste_modulation_format);
      });
  output_power_.AddSetter(
      [this](float requeste_output_power) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetOutputPower(slot, requeste_output_power);
      });
}

/*!
 * \brief TaiOpticsDataSource::UpdateValues method updates attributes with
 * fresh values from TAI.
 * \return ::util::OkStatus if success
 */
::util::Status TaiOpticsDataSource::UpdateValues() {
  int slot = module_slot_.ReadValue<int>().ValueOrDie();

  ASSIGN_OR_RETURN(
      auto tx_laser_frequency,
      taish_client_.GetValue(std::to_string(slot), 0,
                             "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ"));
  tx_laser_frequency_.AssignValue(
      TypesConverter::HertzToMegahertz(tx_laser_frequency));

  ASSIGN_OR_RETURN(
      auto modulation_format,
      taish_client_.GetValue(std::to_string(slot), 0,
                             "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT"));
  modulation_format_.AssignValue(
      TypesConverter::ModulationToOperationalMode(modulation_format));

  ASSIGN_OR_RETURN(
      auto output_power,
      taish_client_.GetValue(std::to_string(slot), 0,
                             "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER"));
  output_power_.AssignValue(std::stof(output_power));

  ASSIGN_OR_RETURN(auto current_output_power,
                   taish_client_.GetValue(
                       std::to_string(slot), 0,
                       "TAI_NETWORK_INTERFACE_ATTR_CURRENT_OUTPUT_POWER"));
  current_output_power_.AssignValue(std::stof(current_output_power));

  ASSIGN_OR_RETURN(
      auto input_power,
      taish_client_.GetValue(std::to_string(slot), 0,
                             "TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER"));
  input_power_.AssignValue(std::stof(input_power));

  return ::util::OkStatus();
}

::util::Status TaiOpticsDataSource::SetTxLaserFrequency(
    int slot, uint64 tx_laser_frequency) {
  return taish_client_.SetValue(
      std::to_string(slot), 0, "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ",
      TypesConverter::MegahertzToHertz(tx_laser_frequency));
}

::util::Status TaiOpticsDataSource::SetModulationFormat(
    int slot, uint64 modulation_format) {
  return taish_client_.SetValue(
      std::to_string(slot), 0, "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT",
      TypesConverter::OperationalModeToModulation(modulation_format));
}

::util::Status TaiOpticsDataSource::SetOutputPower(int slot,
                                                   double output_power) {
  return taish_client_.SetValue(std::to_string(slot), 0,
                                "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER",
                                std::to_string(output_power));
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
