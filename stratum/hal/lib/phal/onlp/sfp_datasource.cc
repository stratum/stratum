// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
//#include "stratum/hal/lib/phal/onlp/onlp_wrapper_fake.h"
#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"
#include <cmath>

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

namespace {
double ConvertMicrowattsTodBm(double microwatts) {
  double milliwatts = microwatts / 1000;
  // Convert milliwatts to dBm: power_ration_in_decibels = 10*log10(milliwatts).
  double power_ratio_decibels = 10.0 * log10(milliwatts);
  return power_ratio_decibels;
}
}  // namespace

::util::StatusOr<std::shared_ptr<OnlpSfpDataSource>> OnlpSfpDataSource::Make(
    OnlpOid sfp_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  ::util::Status result = ValidateOnlpSfpInfo(sfp_id, onlp_interface);
  if (!result.ok()) {
    LOG(ERROR) << "Failed to create SFP datasource for OID: " << sfp_id;
    return result;
  }
  ASSIGN_OR_RETURN(SfpInfo sfp_info, onlp_interface->GetSfpInfo(sfp_id));
  std::shared_ptr<OnlpSfpDataSource> sfp_data_source(
      new OnlpSfpDataSource(sfp_id, onlp_interface, cache_policy, sfp_info));

  // Retrieve attributes' initial values.
  // TODO: Move the logic to Configurator later?
  //sfp_data_source->updateValues();
  sfp_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return sfp_data_source;
//  return std::shared_ptr<OnlpSfpDataSource>(
//      new OnlpSfpDataSource(sfp_id, onlp_interface, cache_policy, sfp_info));
}

OnlpSfpDataSource::OnlpSfpDataSource(OnlpOid sfp_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const SfpInfo& sfp_info)
    : DataSource(cache_policy), sfp_oid_(sfp_id), onlp_stub_(onlp_interface) {
  // Once the sfp present, the oid won't change. Do not add setter for id.
  sfp_id_.AssignValue(sfp_id);

  if (!sfp_info.GetSffInfo().ok()) {
    LOG(ERROR) << "Cannot get SFF info for the SFP with OID " << sfp_id << ".";
    return;
  }
  // Initialize sff dom info. Skip channel infos if we fail to get sff_dom_info.
  int channel_count = sfp_info.GetSffDomInfo()->nchannels;
  for (int i = 0; i < channel_count; ++i) {
    tx_power_.emplace_back(TypedAttribute<double>(this));
    rx_power_.emplace_back(TypedAttribute<double>(this));
    tx_bias_.emplace_back(TypedAttribute<double>(this));
  }
}

::util::Status OnlpSfpDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(SfpInfo sfp_info, onlp_stub_->GetSfpInfo(sfp_oid_));
  // Onlp hw_state always populated.
  sfp_hw_state_ = sfp_info.GetHardwareState();
  // Other attributes are only valid if SFP is present. Return if sfp not
  // present.
  CHECK_RETURN_IF_FALSE(sfp_info.Present()) << "SFP is not present.";

  ASSIGN_OR_RETURN(const SffInfo* sff_info, sfp_info.GetSffInfo());
  sfp_vendor_.AssignValue(std::string(sff_info->vendor));
  sfp_serial_number_.AssignValue(std::string(sff_info->serial));
  sfp_model_name_.AssignValue(std::string(sff_info->model));
  media_type_ = sfp_info.GetMediaType();
  sfp_connector_type_ = sfp_info.GetSfpType();
  sfp_module_type_ = sfp_info.GetSfpModuleType();
  sfp_module_caps_ = sfp_info.GetSfpModuleCaps();
  cable_length_.AssignValue(sff_info->length);
  cable_length_desc_.AssignValue(std::string(sff_info->length_desc));

  const SffDomInfo* sff_dom_info = sfp_info.GetSffDomInfo();
  // Convert from 1/256 Celsius(ONLP unit) to Celsius(Google unit).
  temperature_.AssignValue(static_cast<double>(sff_dom_info->temp) / 256.0);
  // Convert from 0.1mv(ONLP unit) to V(Google unit).
  vcc_.AssignValue(static_cast<double>(sff_dom_info->voltage) / 10000.0);
  int channel_count = sff_dom_info->nchannels;
  for (int i = 0; i < channel_count; ++i) {
    // Convert from 0.1uW(ONLP unit) to dBm(Google unit).
    tx_power_[i].AssignValue(ConvertMicrowattsTodBm(
        static_cast<double>(sff_dom_info->channels[i].tx_power) / 10.0));
    // Convert from 0.1uW(ONLP unit) to dBm(Google unit).
    rx_power_[i].AssignValue(ConvertMicrowattsTodBm(
        static_cast<double>(sff_dom_info->channels[i].rx_power) / 10.0));
    // Convert from 2uA(ONLP unit) to mA(Google unit).
    tx_bias_[i].AssignValue(
        static_cast<double>(sff_dom_info->channels[i].bias_cur) * 2.0 / 1000.0);
  }
  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
