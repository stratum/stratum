// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_FAN_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_FAN_DATASOURCE_H_

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpFanDataSource : public DataSource {
  // Makes a shared_ptr to an FanDataSource which manages an ONLP FAN object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpFanDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpFanDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> Make(
      int fan_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  // Function to set FAN percentage.
  ::util::Status SetFanPercentage(int value);

  // Function to set FAN rpm.
  ::util::Status SetFanRpm(double val);

  // Function to set FAN direction.
  ::util::Status SetFanDirection(FanDir dir);

  // Accessors for managed attributes.
  ManagedAttribute* GetFanId() { return &fan_id_; }
  ManagedAttribute* GetFanDesc() { return &fan_desc_; }
  ManagedAttribute* GetFanHardwareState() { return &fan_hw_state_; }
  ManagedAttribute* GetFanModel() { return &fan_model_name_; }
  ManagedAttribute* GetFanSerialNumber() { return &fan_serial_number_; }
  ManagedAttribute* GetFanPercentage() { return &fan_percentage_; }
  ManagedAttribute* GetFanRPM() { return &fan_speed_rpm_; }
  ManagedAttribute* GetFanDirection() { return &fan_dir_; }
  // Fan Capabilities
  ManagedAttribute* GetCapSetDir() { return &fan_cap_set_dir_; }
  ManagedAttribute* GetCapGetDir() { return &fan_cap_get_dir_; }
  ManagedAttribute* GetCapSetRpm() { return &fan_cap_set_rpm_; }
  ManagedAttribute* GetCapSetPercentage() { return &fan_cap_set_percentage_; }
  ManagedAttribute* GetCapGetRpm() { return &fan_cap_get_rpm_; }
  ManagedAttribute* GetCapGetPercentage() { return &fan_cap_get_percentage_; }

 private:
  OnlpFanDataSource(int fan_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const FanInfo& fan_info);

  static ::util::Status ValidateOnlpFanInfo(OnlpOid fan_oid,
                                            OnlpInterface* onlp_interface) {
    return onlp_interface->GetOidInfo(fan_oid).status();
  }

  ::util::Status UpdateValues() override;

  OnlpOid fan_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<int> fan_id_{this};
  TypedAttribute<std::string> fan_desc_{this};
  EnumAttribute fan_hw_state_{HwState_descriptor(), this};

  // Below attributes only set when present
  TypedAttribute<std::string> fan_model_name_{this};
  TypedAttribute<std::string> fan_serial_number_{this};
  TypedAttribute<int> fan_percentage_{this};
  TypedAttribute<double> fan_speed_rpm_{this};
  // Fan Direction.
  EnumAttribute fan_dir_{FanDir_descriptor(), this};
  TypedAttribute<bool> fan_cap_set_dir_{this};
  TypedAttribute<bool> fan_cap_get_dir_{this};
  TypedAttribute<bool> fan_cap_set_rpm_{this};
  TypedAttribute<bool> fan_cap_set_percentage_{this};
  TypedAttribute<bool> fan_cap_get_rpm_{this};
  TypedAttribute<bool> fan_cap_get_percentage_{this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_FAN_DATASOURCE_H_
