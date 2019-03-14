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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_PSU_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_PSU_DATASOURCE_H_

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

class OnlpPsuDataSource : public DataSource {
  // Makes a shared_ptr to an PsuDataSource which manages an ONLP PSU object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpPsuDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpPsuDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> Make(
      OnlpOid psu_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  ::util::Status IsCapable(PsuCaps psu_caps);

  // Accessors for managed attributes.
  ManagedAttribute* GetPsuId() { return &psu_id_; }
  ManagedAttribute* GetPsuHardwareState() { return &psu_hw_state_; }
  ManagedAttribute* GetPsuModel() { return &psu_model_name_; }
  ManagedAttribute* GetPsuSerialNumber() { return &psu_serial_number_; }
  ManagedAttribute* GetPsuInputVoltage() { return &psu_vin_; }
  ManagedAttribute* GetPsuOutputVoltage() { return &psu_vout_; }
  ManagedAttribute* GetPsuInputCurrent() { return &psu_iin_; }
  ManagedAttribute* GetPsuOutputCurrent() { return &psu_iout_; }
  ManagedAttribute* GetPsuInputPower() { return &psu_pin_; }
  ManagedAttribute* GetPsuOutputPower() { return &psu_pout_; }
  ManagedAttribute* GetPsuType() { return &psu_type_; }

 private:
  OnlpPsuDataSource(OnlpOid psu_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const PsuInfo& psu_info);

  static ::util::Status ValidateOnlpPsuInfo(OnlpOid oid,
                                            OnlpInterface* onlp_interface) {
    ASSIGN_OR_RETURN(OidInfo oid_info, onlp_interface->GetOidInfo(oid));
    CHECK_RETURN_IF_FALSE(oid_info.Present())
        << "The PSU with OID " << oid << " is not currently present.";
    return ::util::OkStatus();
  }

  ::util::Status UpdateValues() override;

  OnlpOid psu_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<OnlpOid> psu_id_{this};
  EnumAttribute psu_hw_state_{HwState_descriptor(), this};
  TypedAttribute<std::string> psu_model_name_{this};
  TypedAttribute<std::string> psu_serial_number_{this};
  TypedAttribute<double> psu_vin_{this};
  TypedAttribute<double> psu_vout_{this};
  TypedAttribute<double> psu_iin_{this};
  TypedAttribute<double> psu_iout_{this};
  TypedAttribute<double> psu_pin_{this};
  TypedAttribute<double> psu_pout_{this};
  // Psu Type.
  EnumAttribute psu_type_{PsuType_descriptor(), this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_PSU_DATASOURCE_H_
