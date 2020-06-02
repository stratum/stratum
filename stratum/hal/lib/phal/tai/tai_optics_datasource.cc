// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

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

// Creates a new TAI optics data source.
::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>>
TaiOpticsDataSource::Make(
    const PhalOpticalModuleConfig::NetworkInterface& config,
    TaiInterface* tai_interface) {
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));
  std::shared_ptr<TaiOpticsDataSource> datasource(
      new TaiOpticsDataSource(config.network_interface(),
                              config.vendor_specific_id(),
                              cache, tai_interface));

  datasource->UpdateValuesUnsafelyWithoutCacheOrLock();
  return datasource;
}

TaiOpticsDataSource::TaiOpticsDataSource(int32 id, uint64 oid,
                                         CachePolicy* cache_policy,
                                         TaiInterface* tai_interface)
    : DataSource(cache_policy), oid_(oid), tai_interface_(tai_interface) {
  // These values do not change during the lifetime of the data source.
  id_.AssignValue(id);
  tx_laser_frequency_.AddSetter(
      [oid, tai_interface](uint64 laser_frequency) -> ::util::Status {
        return tai_interface->SetTxLaserFrequency(oid, laser_frequency);
      });
  operational_mode_.AddSetter(
      [oid, tai_interface](uint64 operational_mode) -> ::util::Status {
        return tai_interface->SetModulationFormat(oid, operational_mode);
      });
  target_output_power_.AddSetter(
      [oid, tai_interface](double output_power) -> ::util::Status {
        return tai_interface->SetTargetOutputPower(oid, output_power);
      });
}

::util::Status TaiOpticsDataSource::UpdateValues() {
  // Update attributes with fresh values from Tai.
  ASSIGN_OR_RETURN(auto tx_laser_freq,
                   tai_interface_->GetTxLaserFrequency(oid_));
  tx_laser_frequency_.AssignValue(tx_laser_freq);
  ASSIGN_OR_RETURN(auto op_mode, tai_interface_->GetModulationFormat(oid_));
  operational_mode_.AssignValue(op_mode);
  ASSIGN_OR_RETURN(auto current_output_power,
                   tai_interface_->GetCurrentOutputPower(oid_));
  current_output_power_.AssignValue(current_output_power);
  ASSIGN_OR_RETURN(auto current_input_power,
                   tai_interface_->GetCurrentInputPower(oid_));
  current_input_power_.AssignValue(current_input_power);
  ASSIGN_OR_RETURN(auto target_output_power,
                   tai_interface_->GetTargetOutputPower(oid_));
  target_output_power_.AssignValue(target_output_power);
  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
