// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_fan_datasource.h"

#include <cmath>
#include <climits>
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


::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> OnlpFanDataSource::Make(
    int fan_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  OnlpOid fan_oid = ONLP_FAN_ID_CREATE(fan_id);
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpFanInfo(fan_oid, onlp_interface))
      << "Failed to create FAN datasource for ID: " << fan_id;
  ASSIGN_OR_RETURN(FanInfo fan_info, onlp_interface->GetFanInfo(fan_oid));
  std::shared_ptr<OnlpFanDataSource> fan_data_source(
      new OnlpFanDataSource(fan_id, onlp_interface, cache_policy, fan_info));

  // Retrieve attributes' initial values.
  // TODO(unknown): Move the logic to Configurator later?
  // fan_data_source->updateValues();
  fan_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return fan_data_source;
}

OnlpFanDataSource::OnlpFanDataSource(int fan_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const FanInfo& fan_info)
    : DataSource(cache_policy), onlp_stub_(onlp_interface) {
  // Once the fan present, the oid won't change. Do not add setter for id.
  fan_oid_ = ONLP_FAN_ID_CREATE(fan_id);

  // NOTE: Following attributes aren't going to change through the lifetime
  // of this datasource, therefore no reason to put them in the UpdateValues
  // function call.

  fan_id_.AssignValue(fan_id);

  // Grab the OID header for the description
  auto oid_info = fan_info.GetHeader();

  fan_desc_.AssignValue(std::string(oid_info->description));

  // Set Fan capabilities
  FanCaps caps;
  fan_info.GetCaps(&caps);
  fan_cap_set_dir_.AssignValue(caps.set_dir());
  fan_cap_get_dir_.AssignValue(caps.get_dir());
  fan_cap_set_rpm_.AssignValue(caps.set_rpm());
  fan_cap_set_percentage_.AssignValue(caps.set_percentage());
  fan_cap_get_rpm_.AssignValue(caps.get_rpm());
  fan_cap_get_percentage_.AssignValue(caps.get_percentage());

  // Add setters here
  fan_dir_.AddSetter(
          [&](const google::protobuf::EnumValueDescriptor* dir)
      -> ::util::Status {
      return this->SetFanDirection(static_cast<FanDir>(dir->index())); });
  fan_percentage_.AddSetter(
          [this](int value)
      -> ::util::Status { return this->SetFanPercentage(value); });
  fan_speed_rpm_.AddSetter(
          [this](double val)
      -> ::util::Status { return this->SetFanRpm(val); });
}

::util::Status OnlpFanDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(FanInfo fan_info, onlp_stub_->GetFanInfo(fan_oid_));

  // Onlp hw_state always populated.
  fan_hw_state_ = fan_info.GetHardwareState();

  // Other attributes are only valid if FAN is present. Return if fan not
  // present.
  if (!fan_info.Present()) return ::util::OkStatus();

  ASSIGN_OR_RETURN(const onlp_fan_info_t *fan_onlp_info, fan_info.GetOnlpFan());

  fan_model_name_.AssignValue(std::string(fan_onlp_info->model));
  fan_serial_number_.AssignValue(std::string(fan_onlp_info->serial));
  fan_percentage_.AssignValue(static_cast<int>(fan_onlp_info->percentage));
  fan_speed_rpm_.AssignValue(static_cast<double>(fan_onlp_info->rpm));
  fan_dir_ = fan_info.GetFanDir();

  return ::util::OkStatus();
}

::util::Status OnlpFanDataSource::SetFanPercentage(int value) {
  return onlp_stub_->SetFanPercent(fan_oid_, value);
}

::util::Status OnlpFanDataSource::SetFanRpm(double val) {
  // Onlp only supports an 'int' here so we're going to have
  // to squeeze a double into an 'int', with an error if
  // it doesn't fit.
  if (val > SHRT_MAX) {
    RETURN_ERROR() << "Set Fan RPM bigger than an integer";
  }
  int onlp_val = static_cast<int>(val);
  return onlp_stub_->SetFanRpm(fan_oid_, onlp_val);
}

::util::Status OnlpFanDataSource::SetFanDirection(FanDir dir) {
  return onlp_stub_->SetFanDir(fan_oid_, dir);
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
