// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_sfp_datasource.h"

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
    int sfp_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  OnlpOid sfp_oid = ONLP_SFP_ID_CREATE(sfp_id);
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpSfpInfo(sfp_oid, onlp_interface))
      << "Failed to create SFP datasource for ID: " << sfp_id;
  ASSIGN_OR_RETURN(SfpInfo sfp_info, onlp_interface->GetSfpInfo(sfp_oid));
  std::shared_ptr<OnlpSfpDataSource> sfp_data_source(
      new OnlpSfpDataSource(sfp_id, onlp_interface, cache_policy, sfp_info));

  // Retrieve attributes' initial values.
  // TODO(unknown): Move the logic to Configurator later?
  // sfp_data_source->updateValues();
  sfp_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return sfp_data_source;
}

OnlpSfpDataSource::OnlpSfpDataSource(int sfp_id, OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const SfpInfo& sfp_info)
    : DataSource(cache_policy), onlp_stub_(onlp_interface) {
  sfp_oid_ = ONLP_SFP_ID_CREATE(sfp_id);

  // NOTE: Following attributes aren't going to change through the lifetime
  // of this datasource, therefore no reason to put them in the UpdateValues
  // function call.

  // Once the sfp present, the oid won't change. Do not add setter for id.
  sfp_id_.AssignValue(sfp_id);

  // Set Sfp Module Caps
  SfpModuleCaps caps;
  sfp_info.GetModuleCaps(&caps);
  sfp_module_cap_f_100_.AssignValue(caps.f_100());
  sfp_module_cap_f_1g_.AssignValue(caps.f_1g());
  sfp_module_cap_f_10g_.AssignValue(caps.f_10g());
  sfp_module_cap_f_40g_.AssignValue(caps.f_40g());
  sfp_module_cap_f_100g_.AssignValue(caps.f_100g());

  if (!sfp_info.GetSffInfo().ok()) {
    LOG(ERROR) << "Cannot get SFF info for the SFP with ID " << sfp_id << ".";
    return;
  }
  // Initialize sff dom info. Skip channel infos if we fail to get sff_dom_info.
  channel_count_.AssignValue(sfp_info.GetSffDomInfo()->nchannels);
  for (int i = 0; i < sfp_info.GetSffDomInfo()->nchannels; ++i) {
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
  if (!sfp_info.Present()) return ::util::OkStatus();

  // Grab the OID header for the description
  auto oid_info = sfp_info.GetHeader();
  sfp_desc_.AssignValue(std::string(oid_info->description));

  ASSIGN_OR_RETURN(const SffInfo* sff_info, sfp_info.GetSffInfo());
  sfp_vendor_.AssignValue(sfp_info.GetSfpVendor());
  sfp_serial_number_.AssignValue(sfp_info.GetSfpSerialNumber());
  sfp_model_name_.AssignValue(sfp_info.GetSfpModel());
  media_type_ = sfp_info.GetMediaType();
  sfp_connector_type_ = sfp_info.GetSfpType();
  sfp_module_type_ = sfp_info.GetSfpModuleType();

  cable_length_.AssignValue(sff_info->length);
  cable_length_desc_.AssignValue(std::string(sff_info->length_desc));

  const SffDomInfo* sff_dom_info = sfp_info.GetSffDomInfo();
  // Convert from 1/256 Celsius(ONLP unit) to Celsius(Google unit).
  temperature_.AssignValue(static_cast<double>(sff_dom_info->temp) / 256.0);
  // Convert from 0.1mv(ONLP unit) to V(Google unit).
  vcc_.AssignValue(static_cast<double>(sff_dom_info->voltage) / 10000.0);
  channel_count_.AssignValue(sff_dom_info->nchannels);
  for (int i = 0; i < sff_dom_info->nchannels; ++i) {
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
