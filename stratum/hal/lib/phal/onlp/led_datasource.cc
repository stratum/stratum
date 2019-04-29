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

#include "stratum/hal/lib/phal/onlp/led_datasource.h"

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


::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> OnlpLedDataSource::Make(
    OnlpOid led_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpLedInfo(led_id, onlp_interface))
      << "Failed to create LED datasource for OID: " << led_id;
  ASSIGN_OR_RETURN(LedInfo led_info, onlp_interface->GetLedInfo(led_id));
  std::shared_ptr<OnlpLedDataSource> led_data_source(
      new OnlpLedDataSource(led_id, onlp_interface, cache_policy, led_info));

  // Retrieve attributes' initial values.
  // TODO: Move the logic to Configurator later?
  // led_data_source->updateValues();
  led_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return led_data_source;
}

OnlpLedDataSource::OnlpLedDataSource(OnlpOid led_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const LedInfo& led_info)
    : DataSource(cache_policy), led_oid_(led_id), onlp_stub_(onlp_interface) {
  // Once the led present, the oid won't change. Do not add setter for id.
  led_id_.AssignValue(led_id);
  led_mode_.AddSetter(
          [&](const google::protobuf::EnumValueDescriptor* value)
      -> ::util::Status {
      return this->SetLedMode(static_cast<LedMode>(value->index())); });
  led_char_.AddSetter(
          [this](char val)
      -> ::util::Status { return this->SetLedCharacter(val); });
}

::util::Status OnlpLedDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(LedInfo led_info, onlp_stub_->GetLedInfo(led_oid_));

  // Onlp hw_state always populated.
  led_hw_state_ = led_info.GetHardwareState();

  // Other attributes are only valid if LED is present. Return if led not
  // present.
  CHECK_RETURN_IF_FALSE(led_info.Present()) << "LED is not present.";

  led_mode_ = led_info.GetLedMode();
  led_char_.AssignValue(led_info.GetLedChar());
  // TODO(Yi): store the caps in attribute

  return ::util::OkStatus();
}

::util::Status OnlpLedDataSource::IsCapable(LedCaps led_caps) {
  // TODO(Yi): we should not get LedInfo here
  ASSIGN_OR_RETURN(LedInfo led_info, onlp_stub_->GetLedInfo(led_oid_));
  CHECK_RETURN_IF_FALSE(led_info.Capable(led_caps))
      << "Expected LED capability is not present.";

  return ::util::OkStatus();
}

::util::Status OnlpLedDataSource::SetLedMode(LedMode value) {
  return onlp_stub_->SetLedMode(led_oid_, value);
}

::util::Status OnlpLedDataSource::SetLedCharacter(char val) {
  return onlp_stub_->SetLedCharacter(led_oid_, val);
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
