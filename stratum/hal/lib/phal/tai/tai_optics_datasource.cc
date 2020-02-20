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
    : DataSource(cache_policy) {
  // These values do not change during the lifetime of the data source.

#if defined(WITH_GRPC_TAI)
  // note server address is hardcoded
  taish_client_ = absl::make_unique<TaishClient>(grpc::CreateChannel(
      "localhost:50051", grpc::InsecureChannelCredentials()));
#else
  // Pointer to the Tai Manager. Not created or owned by this class.
  tai_manager_ = tai::TAIManager::CreateSingleton();
#endif

  module_slot_.AssignValue(id);

  tx_laser_frequency_.AddSetter(
      [this](uint64 requested_tx_laser_frequency) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetTxLaserFrequency(slot, requested_tx_laser_frequency);
      });
  operational_mode_.AddSetter(
      [this](uint64 requested_operational_mode) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetOperationalMode(slot, requested_operational_mode);
      });
  output_power_.AddSetter(
      [this](double requested_output_power) -> ::util::Status {
        int slot = this->module_slot_.ReadValue<int>().ValueOrDie();
        return this->SetOutputPower(slot, requested_output_power);
      });
}

/*!
 * \brief TaiOpticsDataSource::UpdateValues method updates attributes with
 * fresh values from TAI.
 * \return ::util::OkStatus if success
 */
::util::Status TaiOpticsDataSource::UpdateValues() {
  // Update attributes with fresh values from Tai.

  int slot = module_slot_.ReadValue<int>().ValueOrDie();
#if defined(WITH_GRPC_TAI)
  ASSIGN_OR_RETURN(
      auto tx_laser_frequency,
      taish_client_->GetValue(std::to_string(slot), 0,
                              "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ"));
  tx_laser_frequency_.AssignValue(
      TypesConverter::HertzToMegahertz(tx_laser_frequency));

  ASSIGN_OR_RETURN(
      auto operational_mode,
      taish_client_->GetValue(std::to_string(slot), 0,
                              "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT"));
  operational_mode_.AssignValue(
      TypesConverter::ModulationToOperationalMode(operational_mode));

  ASSIGN_OR_RETURN(
      auto output_power,
      taish_client_->GetValue(std::to_string(slot), 0,
                              "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER"));
  output_power_.AssignValue(std::stof(output_power));

  ASSIGN_OR_RETURN(auto current_output_power,
                   taish_client_->GetValue(
                       std::to_string(slot), 0,
                       "TAI_NETWORK_INTERFACE_ATTR_CURRENT_OUTPUT_POWER"));
  current_output_power_.AssignValue(std::stof(current_output_power));

  ASSIGN_OR_RETURN(auto input_power,
                   taish_client_->GetValue(
                       std::to_string(slot), 0,
                       "TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER"));
  input_power_.AssignValue(std::stof(input_power));
#else  // defined(WITH_GRPC_TAI)
  ASSIGN_OR_RETURN(auto tx_laser_frequency,
                   tai_manager_->GetValue<uint64>(
                       TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, {slot, 0}));
  tx_laser_frequency_.AssignValue(tx_laser_frequency);

  ASSIGN_OR_RETURN(
      auto operational_mode,
      tai_manager_->GetValue<uint64>(
          TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT, {slot, 0}));
  operational_mode_.AssignValue(operational_mode);

  ASSIGN_OR_RETURN(auto output_power,
                   tai_manager_->GetValue<double>(
                       TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, {slot, 0}));
  output_power_.AssignValue(output_power);

  ASSIGN_OR_RETURN(
      auto current_output_power,
      tai_manager_->GetValue<double>(
          TAI_NETWORK_INTERFACE_ATTR_CURRENT_OUTPUT_POWER, {slot, 0}));
  current_output_power_.AssignValue(current_output_power);

  ASSIGN_OR_RETURN(
      auto input_power,
      tai_manager_->GetValue<double>(
          TAI_NETWORK_INTERFACE_ATTR_CURRENT_INPUT_POWER, {slot, 0}));
  input_power_.AssignValue(input_power);
#endif

  return ::util::OkStatus();
}

#if defined(WITH_GRPC_TAI)
::util::Status TaiOpticsDataSource::SetTxLaserFrequency(
    int slot, uint64 tx_laser_frequency) {
  return taish_client_->SetValue(
      std::to_string(slot), 0, "TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ",
      TypesConverter::MegahertzToHertz(tx_laser_frequency));
}

::util::Status TaiOpticsDataSource::SetOperationalMode(
    int slot, uint64 operational_mode) {
  // Now, the operational mode is only converted to modulation and vice versa.
  return taish_client_->SetValue(
      std::to_string(slot), 0, "TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT",
      TypesConverter::OperationalModeToModulation(operational_mode));
}

::util::Status TaiOpticsDataSource::SetOutputPower(int slot,
                                                   double output_power) {
  return taish_client_->SetValue(std::to_string(slot), 0,
                                 "TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER",
                                 std::to_string(output_power));
}

#else
::util::Status TaiOpticsDataSource::SetTxLaserFrequency(
    int slot, uint64 tx_laser_frequency) {
  return tai_manager_->SetValue<uint64>(
      tx_laser_frequency, TAI_NETWORK_INTERFACE_ATTR_TX_LASER_FREQ, {slot, 0});
}

::util::Status TaiOpticsDataSource::SetOperationalMode(
    int slot, uint64 operational_mode) {
  // Now, the operational mode is only converted to modulation and vice versa.
  return tai_manager_->SetValue<uint64>(
      operational_mode, TAI_NETWORK_INTERFACE_ATTR_MODULATION_FORMAT,
      {slot, 0});
}

::util::Status TaiOpticsDataSource::SetOutputPower(int slot,
                                                   double output_power) {
  return tai_manager_->SetValue<double>(
      output_power, TAI_NETWORK_INTERFACE_ATTR_OUTPUT_POWER, {slot, 0});
}
#endif

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
