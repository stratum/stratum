/*
 * Copyright 2018 Google LLC
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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_FAN_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_FAN_DATASOURCE_H_

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
// FIXME remove when onlp_wrapper.h is stable
// #include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

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
      OnlpOid fan_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  // Accessors for managed attributes.
  ManagedAttribute* GetFanId() { return &fan_id_; }
  ManagedAttribute* GetFanHardwareState() { return &fan_hw_state_; }
  ManagedAttribute* GetFanModel() { return &fan_model_name_; }
  ManagedAttribute* GetFanSerialNumber() { return &fan_serial_number_; }
  ManagedAttribute* GetFanPercentage() { return &fan_percentage_; }
  ManagedAttribute* GetFanRPM() { return &fan_speed_rpm_; }
  ManagedAttribute* GetFanDirection() { return &fan_dir_; }
  ManagedAttribute* GetFanCapabilities() { return &fan_caps_; }

 private:
  OnlpFanDataSource(OnlpOid fan_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const FanInfo& fan_info);

  static ::util::Status ValidateOnlpFanInfo(OnlpOid oid,
                                            OnlpInterface* onlp_interface) {
    ASSIGN_OR_RETURN(OidInfo oid_info, onlp_interface->GetOidInfo(oid));
    CHECK_RETURN_IF_FALSE(oid_info.Present())
        << "The FAN with OID " << oid << " is not currently present.";
    return ::util::OkStatus();
  }

  ::util::Status UpdateValues() override;

  OnlpOid fan_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<OnlpOid> fan_id_{this};
  EnumAttribute fan_hw_state_{HwState_descriptor(), this};
  TypedAttribute<std::string> fan_model_name_{this};
  TypedAttribute<std::string> fan_serial_number_{this};
  TypedAttribute<int> fan_percentage_{this};
  TypedAttribute<int> fan_speed_rpm_{this};
  // Fan Direction.
  EnumAttribute fan_dir_{FanDir_descriptor(), this};
  // Fan Capabilities.
  EnumAttribute fan_caps_{FanCaps_descriptor(), this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_FAN_DATASOURCE_H_
