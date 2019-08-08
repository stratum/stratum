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
    int led_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy) {
  OnlpOid led_oid = ONLP_LED_ID_CREATE(led_id);
  RETURN_IF_ERROR_WITH_APPEND(ValidateOnlpLedInfo(led_oid, onlp_interface))
      << "Failed to create LED datasource for ID: " << led_id;
  ASSIGN_OR_RETURN(LedInfo led_info, onlp_interface->GetLedInfo(led_oid));
  std::shared_ptr<OnlpLedDataSource> led_data_source(
      new OnlpLedDataSource(led_id, onlp_interface, cache_policy, led_info));

  // Retrieve attributes' initial values.
  // TODO(unknown): Move the logic to Configurator later?
  // led_data_source->updateValues();
  led_data_source->UpdateValuesUnsafelyWithoutCacheOrLock();
  return led_data_source;
}

OnlpLedDataSource::OnlpLedDataSource(int led_id,
                                     OnlpInterface* onlp_interface,
                                     CachePolicy* cache_policy,
                                     const LedInfo& led_info)
    : DataSource(cache_policy), onlp_stub_(onlp_interface) {
  led_oid_ = ONLP_LED_ID_CREATE(led_id);

  // NOTE: Following attributes aren't going to change through the lifetime
  // of this datasource, therefore no reason to put them in the UpdateValues
  // function call.

  // Once the led present, the oid won't change. Do not add setter for id.
  led_id_.AssignValue(led_id);

  // Grab the OID header for the description
  auto oid_info = led_info.GetHeader();
  led_desc_.AssignValue(std::string(oid_info->description));

  // Add led Capabiliities
  LedCaps caps;
  led_info.GetCaps(&caps);
  led_cap_off_.AssignValue(caps.off());
  led_cap_auto_.AssignValue(caps.auto_());
  led_cap_auto_blinking_.AssignValue(caps.auto_blinking());
  led_cap_char_.AssignValue(caps.char_());
  led_cap_red_.AssignValue(caps.red());
  led_cap_red_blinking_.AssignValue(caps.red_blinking());
  led_cap_orange_.AssignValue(caps.orange());
  led_cap_orange_blinking_.AssignValue(caps.orange_blinking());
  led_cap_yellow_.AssignValue(caps.yellow());
  led_cap_yellow_blinking_.AssignValue(caps.yellow_blinking());
  led_cap_green_.AssignValue(caps.green());
  led_cap_green_blinking_.AssignValue(caps.green_blinking());
  led_cap_blue_.AssignValue(caps.blue());
  led_cap_blue_blinking_.AssignValue(caps.blue_blinking());
  led_cap_purple_.AssignValue(caps.purple());
  led_cap_purple_blinking_.AssignValue(caps.purple_blinking());

  // Add setters here
  led_mode_.AddSetter(
          [&](const google::protobuf::EnumValueDescriptor* value)
      -> ::util::Status {
      return this->SetLedMode(static_cast<LedMode>(value->index())); });
  led_char_.AddSetter(
          [this](int32 val)
      -> ::util::Status { return this->SetLedCharacter(val); });
}

::util::Status OnlpLedDataSource::UpdateValues() {
  ASSIGN_OR_RETURN(LedInfo led_info, onlp_stub_->GetLedInfo(led_oid_));

  // Onlp hw_state always populated.
  led_hw_state_ = led_info.GetHardwareState();

  // Other attributes are only valid if LED is present. Return if led not
  // present.
  if (!led_info.Present()) return ::util::OkStatus();

  led_mode_ = led_info.GetLedMode();
  led_char_.AssignValue(led_info.GetLedChar());

  return ::util::OkStatus();
}

::util::Status OnlpLedDataSource::SetLedMode(LedMode value) {
  return onlp_stub_->SetLedMode(led_oid_, value);
}

::util::Status OnlpLedDataSource::SetLedCharacter(int32 val) {
  char ch = val & 0xff;
  return onlp_stub_->SetLedCharacter(led_oid_, ch);
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
