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

#include "stratum/hal/lib/phal/onlp/psu_datasource.h"

#include <cmath>
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
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

::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> OnlpPsuDataSource::Make(
    int psu_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  OnlpOid psu_oid = ONLP_PSU_ID_CREATE(psu_id);
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpPsuInfo(psu_oid, onlp_interface))
      << "Failed to create PSU datasource for ID: " << psu_id;
  ASSIGN_OR_RETURN(PsuInfo psu_info, onlp_interface->GetPsuInfo(psu_oid));
  std::shared_ptr<OnlpPsuDataSource> psu_data_source(
      new OnlpPsuDataSource(psu_id, onlp_interface, cache_policy, psu_info));

  // Retrieve attributes' initial values.
  // TODO(unknown): Move the logic to Configurator later?
  // psu_data_source->updateValues();
  psu_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return psu_data_source;
}

OnlpPsuDataSource::OnlpPsuDataSource(int psu_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const PsuInfo& psu_info)
    : DataSource(cache_policy), onlp_stub_(onlp_interface) {

  psu_oid_ = ONLP_PSU_ID_CREATE(psu_id);

  // NOTE: Following attributes aren't going to change through the lifetime
  // of this datasource, therefore no reason to put them in the UpdateValues
  // function call.

  // Once the psu present, the oid won't change. Do not add setter for id.
  psu_id_.AssignValue(psu_id);

  // Grab the OID header for the description
  auto oid_info = psu_info.GetHeader();
  psu_desc_.AssignValue(std::string(oid_info->description));

  // Get Psu capabilities
  PsuCaps caps;
  psu_info.GetCaps(&caps);
  psu_cap_type_.AssignValue(caps.get_type());
  psu_cap_vin_.AssignValue(caps.get_vin());
  psu_cap_vout_.AssignValue(caps.get_vout());
  psu_cap_iin_.AssignValue(caps.get_iin());
  psu_cap_iout_.AssignValue(caps.get_iout());
  psu_cap_pin_.AssignValue(caps.get_pin());
  psu_cap_pout_.AssignValue(caps.get_pout());
}

::util::Status OnlpPsuDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(PsuInfo psu_info, onlp_stub_->GetPsuInfo(psu_oid_));

  // Onlp hw_state always populated.
  psu_hw_state_ = psu_info.GetHardwareState();

  // Other attributes are only valid if PSU is present. Return if psu not
  // present.
  if (!psu_info.Present()) return ::util::OkStatus();

  ASSIGN_OR_RETURN(const onlp_psu_info_t *psu_onlp_info, psu_info.GetOnlpPsu());

  psu_model_name_.AssignValue(std::string(psu_onlp_info->model));
  psu_serial_number_.AssignValue(std::string(psu_onlp_info->serial));
  // Convert from 1mV(ONLP unit) to V(Google unit).
  psu_vin_.AssignValue(static_cast<double>(psu_onlp_info->mvin)/1000.0);
  psu_vout_.AssignValue(static_cast<double>(psu_onlp_info->mvout)/1000.0);
  // Convert from 1mA(ONLP unit) to A(Google unit).
  psu_iin_.AssignValue(static_cast<double>(psu_onlp_info->miin)/1000.0);
  psu_iout_.AssignValue(static_cast<double>(psu_onlp_info->miout)/1000.0);
  // Convert from 1mW(ONLP unit) to W(Google unit).
  psu_pin_.AssignValue(static_cast<double>(psu_onlp_info->mpin)/1000.0);
  psu_pout_.AssignValue(static_cast<double>(psu_onlp_info->mpout)/1000.0);
  psu_type_ = psu_info.GetPsuType();

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
