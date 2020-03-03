// Copyright 2019 Edgecore Networks Corporation
// Phani Karanam <phani_karanam@edge-core.com>
// Copyright 2019 Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_THERMAL_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_THERMAL_DATASOURCE_H_

#include <memory>
#include <string>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpThermalDataSource : public DataSource {
  // Makes a shared_ptr to an ThermalDataSource which manages an ONLP THERMAL
  // object. Returns error if the OID object is not of the correct type or
  // not present.
 public:
  // OnlpThermalDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpThermalDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpThermalDataSource>> Make(
      int thermal_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  // Accessors for managed attributes.
  ManagedAttribute* GetThermalId() { return &thermal_id_; }
  ManagedAttribute* GetThermalDesc() { return &thermal_desc_; }
  ManagedAttribute* GetThermalHardwareState() { return &thermal_hw_state_; }
  // Thermal capabilities
  ManagedAttribute* GetThermalCurTemp() { return &thermal_cur_temp_; }
  ManagedAttribute* GetThermalWarnTemp() { return &thermal_warn_temp_; }
  ManagedAttribute* GetThermalErrorTemp() { return &thermal_error_temp_; }
  ManagedAttribute* GetThermalShutDownTemp() {
    return &thermal_shut_down_temp_;
  }

  // Thermal capabilities
  ManagedAttribute* GetCapTemp() { return &thermal_cap_temp_; }
  ManagedAttribute* GetCapWarnThresh() { return &thermal_cap_warn_thresh_; }
  ManagedAttribute* GetCapErrThresh() { return &thermal_cap_err_thresh_; }
  ManagedAttribute* GetCapShutdownThresh()
    { return &thermal_cap_shutdown_thresh_; }

 private:
  OnlpThermalDataSource(int thermal_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const ThermalInfo& thermal_info);

  static ::util::Status ValidateOnlpThermalInfo(OnlpOid thermal_oid,
                                            OnlpInterface* onlp_interface) {
    return onlp_interface->GetOidInfo(thermal_oid).status();
  }

  ::util::Status UpdateValues() override;
  OnlpOid thermal_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<int> thermal_id_{this};
  TypedAttribute<std::string> thermal_desc_{this};
  EnumAttribute thermal_hw_state_{HwState_descriptor(), this};
  TypedAttribute<double> thermal_cur_temp_{this};
  TypedAttribute<double> thermal_warn_temp_{this};
  TypedAttribute<double> thermal_error_temp_{this};
  TypedAttribute<double> thermal_shut_down_temp_{this};
  TypedAttribute<bool> thermal_cap_temp_{this};
  TypedAttribute<bool> thermal_cap_warn_thresh_{this};
  TypedAttribute<bool> thermal_cap_err_thresh_{this};
  TypedAttribute<bool> thermal_cap_shutdown_thresh_{this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_THERMAL_DATASOURCE_H_
