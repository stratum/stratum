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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_SFP_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_SFP_DATASOURCE_H_

#include <vector>
#include <memory>
#include <string>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
// FIXME remove when onlp_wrapper.h is stable
// #include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
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

class OnlpSfpDataSource : public DataSource {
  // Makes a shared_ptr to an SfpDataSource which manages an ONLP SFP object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpSfpDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpSfpDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpSfpDataSource>> Make(
      int id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);


  // Accessors for managed attributes.
  ManagedAttribute* GetSfpId() { return &sfp_id_; }
  ManagedAttribute* GetSfpDesc() { return &sfp_desc_; }
  ManagedAttribute* GetSfpHardwareState() { return &sfp_hw_state_; }
  ManagedAttribute* GetSfpMediaType() { return &media_type_; }
  ManagedAttribute* GetSfpType() { return &sfp_connector_type_; }
  ManagedAttribute* GetSfpModuleType() { return &sfp_module_type_; }
  // Module Capabilities
  ManagedAttribute* GetModCapF100() { return &sfp_module_cap_f_100_; }
  ManagedAttribute* GetModCapF1G() { return &sfp_module_cap_f_1g_; }
  ManagedAttribute* GetModCapF10G() { return &sfp_module_cap_f_10g_; }
  ManagedAttribute* GetModCapF40G() { return &sfp_module_cap_f_40g_; }
  ManagedAttribute* GetModCapF100G() { return &sfp_module_cap_f_100g_; }

  ManagedAttribute* GetSfpCableLength() { return &cable_length_; }
  ManagedAttribute* GetSfpCableLengthDesc() { return &cable_length_desc_; }
  ManagedAttribute* GetSfpVendor() { return &sfp_vendor_; }
  ManagedAttribute* GetSfpModel() { return &sfp_model_name_; }
  ManagedAttribute* GetSfpSerialNumber() { return &sfp_serial_number_; }
  ManagedAttribute* GetSfpTemperature() { return &temperature_; }
  ManagedAttribute* GetSfpVoltage() { return &vcc_; }
  ManagedAttribute* GetSfpChannelCount() { return &channel_count_; }
  ManagedAttribute* GetSfpRxPower(int channel_index) {
    return &rx_power_[channel_index];
  }
  ManagedAttribute* GetSfpTxPower(int channel_index) {
    return &tx_power_[channel_index];
  }
  ManagedAttribute* GetSfpTxBias(int channel_index) {
    return &tx_bias_[channel_index];
  }

 private:
  OnlpSfpDataSource(int id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const SfpInfo& sfp_info);

  static ::util::Status ValidateOnlpSfpInfo(OnlpOid sfp_oid,
                                            OnlpInterface* onlp_interface) {
    return onlp_interface->GetOidInfo(sfp_oid).status();
  }

  ::util::Status UpdateValues() override;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;

  OnlpOid sfp_oid_;

  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<int> sfp_id_{this};
  TypedAttribute<std::string> sfp_desc_{this};
  EnumAttribute sfp_hw_state_{HwState_descriptor(), this};
  TypedAttribute<std::string> sfp_vendor_{this};
  TypedAttribute<std::string> sfp_model_name_{this};
  TypedAttribute<std::string> sfp_serial_number_{this};

  // Media Type.
  EnumAttribute media_type_{MediaType_descriptor(), this};

  // SFP Type.
  EnumAttribute sfp_connector_type_{SfpType_descriptor(), this};

  // SFP Module Type.
  EnumAttribute sfp_module_type_{SfpModuleType_descriptor(), this};

  // SFP Capabilities.
  TypedAttribute<bool> sfp_module_cap_f_100_{this};
  TypedAttribute<bool> sfp_module_cap_f_1g_{this};
  TypedAttribute<bool> sfp_module_cap_f_10g_{this};
  TypedAttribute<bool> sfp_module_cap_f_40g_{this};
  TypedAttribute<bool> sfp_module_cap_f_100g_{this};

  // Cable Length.
  TypedAttribute<int> cable_length_{this};
  TypedAttribute<std::string> cable_length_desc_{this};

  // SFP temperature.
  TypedAttribute<double> temperature_{this};
  // SFP Voltage.
  TypedAttribute<double> vcc_{this};

  // Channel count
  TypedAttribute<int> channel_count_{this};

  // Channels info.
  std::vector<TypedAttribute<double>> rx_power_;
  std::vector<TypedAttribute<double>> tx_power_;
  std::vector<TypedAttribute<double>> tx_bias_;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_SFP_DATASOURCE_H_
