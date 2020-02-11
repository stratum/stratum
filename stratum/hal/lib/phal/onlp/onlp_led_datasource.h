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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_LED_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_LED_DATASOURCE_H_

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

class OnlpLedDataSource : public DataSource {
  // Makes a shared_ptr to an LedDataSource which manages an ONLP LED object.
  // Returns error if the OID object is not of the correct type or not present.
 public:
  // OnlpLedDataSource does not take ownership of onlp_interface. We expect
  // onlp_interface remains valid during OnlpLedDataSource's lifetime.
  static ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> Make(
      int led_id, OnlpInterface* onlp_interface, CachePolicy* cache_policy);

  // Function to set LED mode.
  ::util::Status SetLedMode(LedMode value);

  // Function to set LED character.
  ::util::Status SetLedCharacter(int32 val);

  // Accessors for managed attributes.
  ManagedAttribute* GetLedId() { return &led_id_; }
  ManagedAttribute* GetLedDesc() { return &led_desc_; }
  ManagedAttribute* GetLedHardwareState() { return &led_hw_state_; }
  ManagedAttribute* GetLedMode() { return &led_mode_; }
  ManagedAttribute* GetLedChar() { return &led_char_; }

  // Led Capabilities
  ManagedAttribute* GetCapOff() { return &led_cap_off_; }
  ManagedAttribute* GetCapAuto() { return &led_cap_auto_; }
  ManagedAttribute* GetCapAutoBlinking() { return &led_cap_auto_blinking_; }
  ManagedAttribute* GetCapChar() { return &led_cap_char_; }
  ManagedAttribute* GetCapRed() { return &led_cap_red_; }
  ManagedAttribute* GetCapRedBlinking() { return &led_cap_red_blinking_; }
  ManagedAttribute* GetCapOrange() { return &led_cap_orange_; }
  ManagedAttribute* GetCapOrangeBlinking() { return &led_cap_orange_blinking_; }
  ManagedAttribute* GetCapYellow() { return &led_cap_yellow_; }
  ManagedAttribute* GetCapYellowBlinking() { return &led_cap_yellow_blinking_; }
  ManagedAttribute* GetCapGreen() { return &led_cap_green_; }
  ManagedAttribute* GetCapGreenBlinking() { return &led_cap_green_blinking_; }
  ManagedAttribute* GetCapBlue() { return &led_cap_blue_; }
  ManagedAttribute* GetCapBlueBlinking() { return &led_cap_blue_blinking_; }
  ManagedAttribute* GetCapPurple() { return &led_cap_purple_; }
  ManagedAttribute* GetCapPurpleBlinking() { return &led_cap_purple_blinking_; }

 private:
  OnlpLedDataSource(int led_id, OnlpInterface* onlp_interface,
                    CachePolicy* cache_policy, const LedInfo& led_info);

  static ::util::Status ValidateOnlpLedInfo(OnlpOid led_oid,
                                            OnlpInterface* onlp_interface) {
    return onlp_interface->GetOidInfo(led_oid).status();
  }

  ::util::Status UpdateValues() override;

  OnlpOid led_oid_;

  // We do not own ONLP stub object. ONLP stub is created on PHAL creation and
  // destroyed when PHAL deconstruct. Do not delete onlp_stub_.
  OnlpInterface* onlp_stub_;


  // A list of managed attributes.
  // Hardware Info.
  TypedAttribute<int> led_id_{this};
  TypedAttribute<std::string> led_desc_{this};
  EnumAttribute led_hw_state_{HwState_descriptor(), this};
  TypedAttribute<int32> led_char_{this};

  // Led Mode
  EnumAttribute led_mode_{LedMode_descriptor(), this};

  // Led Capabilities
  TypedAttribute<bool> led_cap_off_{this};
  TypedAttribute<bool> led_cap_auto_{this};
  TypedAttribute<bool> led_cap_auto_blinking_{this};
  TypedAttribute<bool> led_cap_char_{this};
  TypedAttribute<bool> led_cap_red_{this};
  TypedAttribute<bool> led_cap_red_blinking_{this};
  TypedAttribute<bool> led_cap_orange_{this};
  TypedAttribute<bool> led_cap_orange_blinking_{this};
  TypedAttribute<bool> led_cap_yellow_{this};
  TypedAttribute<bool> led_cap_yellow_blinking_{this};
  TypedAttribute<bool> led_cap_green_{this};
  TypedAttribute<bool> led_cap_green_blinking_{this};
  TypedAttribute<bool> led_cap_blue_{this};
  TypedAttribute<bool> led_cap_blue_blinking_{this};
  TypedAttribute<bool> led_cap_purple_{this};
  TypedAttribute<bool> led_cap_purple_blinking_{this};
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_LED_DATASOURCE_H_
