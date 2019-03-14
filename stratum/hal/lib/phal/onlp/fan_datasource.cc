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

#include "stratum/hal/lib/phal/onlp/fan_datasource.h"

#include <cmath>
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/datasource.h"
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


::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> OnlpFanDataSource::Make(
    OnlpOid fan_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpFanInfo(fan_id, onlp_interface))
      << "Failed to create FAN datasource for OID: " << fan_id;
  ASSIGN_OR_RETURN(FanInfo fan_info, onlp_interface->GetFanInfo(fan_id));
  std::shared_ptr<OnlpFanDataSource> fan_data_source(
      new OnlpFanDataSource(fan_id, onlp_interface, cache_policy, fan_info));

  // Retrieve attributes' initial values.
  // TODO: Move the logic to Configurator later?
  // fan_data_source->updateValues();
  fan_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return fan_data_source;
}

OnlpFanDataSource::OnlpFanDataSource(OnlpOid fan_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const FanInfo& fan_info)
    : DataSource(cache_policy), fan_oid_(fan_id), onlp_stub_(onlp_interface) {
  // Once the fan present, the oid won't change. Do not add setter for id.
  fan_id_.AssignValue(fan_id);
}

::util::Status OnlpFanDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(FanInfo fan_info, onlp_stub_->GetFanInfo(fan_oid_));

  // Onlp hw_state always populated.
  fan_hw_state_ = fan_info.GetHardwareState();

  // Other attributes are only valid if FAN is present. Return if fan not
  // present.
  CHECK_RETURN_IF_FALSE(fan_info.Present()) << "FAN is not present.";

  ASSIGN_OR_RETURN(const onlp_fan_info_t *fan_onlp_info, fan_info.GetOnlpFan());

  fan_model_name_.AssignValue(std::string(fan_onlp_info->model));
  fan_serial_number_.AssignValue(std::string(fan_onlp_info->serial));
  fan_percentage_.AssignValue(static_cast<int>(fan_onlp_info->percentage));
  fan_speed_rpm_.AssignValue(static_cast<int>(fan_onlp_info->rpm));
  fan_dir_ = fan_info.GetFanDir();

  return ::util::OkStatus();
}

::util::Status OnlpFanDataSource::IsCapable(FanCaps fan_caps) {
  bool is_capable;

  ASSIGN_OR_RETURN(FanInfo fan_info, onlp_stub_->GetFanInfo(fan_oid_));
  is_capable = fan_info.Capable(fan_caps);
  CHECK_RETURN_IF_FALSE(is_capable)
      << "Expected FAN capability is not present.";

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
