/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PSU_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PSU_DATASOURCE_H_

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

class OnlpPsuDataSource : public DataSource {
  // Makes a shared_ptr to an PsuDataSource which manages an ONLP PSU object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpPsuDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpPsuDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> Make(
      int psu_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  // Accessors for managed attributes.
  ManagedAttribute* GetPsuId() { return &psu_id_; }
  ManagedAttribute* GetPsuDesc() { return &psu_desc_; }
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
  // Psu Capabilities
  ManagedAttribute* GetCapGetType() { return &psu_cap_type_; }
  ManagedAttribute* GetCapGetVIn() { return &psu_cap_vin_; }
  ManagedAttribute* GetCapGetVOut() { return &psu_cap_vout_; }
  ManagedAttribute* GetCapGetIIn() { return &psu_cap_iin_; }
  ManagedAttribute* GetCapGetIOut() { return &psu_cap_iout_; }
  ManagedAttribute* GetCapGetPIn() { return &psu_cap_pin_; }
  ManagedAttribute* GetCapGetPOut() { return &psu_cap_pout_; }

 private:
  OnlpPsuDataSource(int psu_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const PsuInfo& psu_info);

  static ::util::Status ValidateOnlpPsuInfo(OnlpOid psu_oid,
                                            OnlpInterface* onlp_interface) {
    return onlp_interface->GetOidInfo(psu_oid).status();
  }

  ::util::Status UpdateValues() override;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  OnlpOid psu_oid_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<int> psu_id_{this};
  TypedAttribute<std::string> psu_desc_{this};
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
  // Psu Capabilities
  TypedAttribute<bool> psu_cap_type_{this};
  TypedAttribute<bool> psu_cap_vin_{this};
  TypedAttribute<bool> psu_cap_vout_{this};
  TypedAttribute<bool> psu_cap_iin_{this};
  TypedAttribute<bool> psu_cap_iout_{this};
  TypedAttribute<bool> psu_cap_pin_{this};
  TypedAttribute<bool> psu_cap_pout_{this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_PSU_DATASOURCE_H_
