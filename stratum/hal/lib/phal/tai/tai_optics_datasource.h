// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_TAI_TAI_OPTICS_DATASOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/tai/tai_interface.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// TaiOpticsDataSource class updates Database<->TAI with fresh values.
class TaiOpticsDataSource final : public DataSource {
 public:
  static ::util::StatusOr<std::shared_ptr<TaiOpticsDataSource>> Make(
      const PhalOpticalModuleConfig::NetworkInterface& config,
      TaiInterface* tai_interface);

  // Accessors for managed attributes.
  ManagedAttribute* GetId() { return &id_; }
  ManagedAttribute* GetTxLaserFrequency() { return &tx_laser_frequency_; }
  ManagedAttribute* GetOperationalMode() { return &operational_mode_; }
  ManagedAttribute* GetTargetOutputPower() { return &target_output_power_; }
  ManagedAttribute* GetCurrentOutputPower() { return &current_output_power_; }
  ManagedAttribute* GetCurrentInputPower() { return &current_input_power_; }

 private:
  uint64 oid_;
  // Private constructor.
  TaiOpticsDataSource(int32 id, uint64 oid, CachePolicy* cache_policy,
                      TaiInterface* tai_interface);

  ::util::Status UpdateValues() override;

  // Reference to the tai interface, the data source doesn't own this object.
  TaiInterface* tai_interface_;

  // Managed attributes.
  TypedAttribute<int32> id_{this};
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
