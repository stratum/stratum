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


#ifndef STRATUM_HAL_LIB_PHAL_LED_DATASOURCE_H_
#define STRATUM_HAL_LIB_PHAL_LED_DATASOURCE_H_

#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "stratum/lib/macros.h"
#include "stratum/public/proto/hal.pb.h"
#include "absl/base/integral_types.h"
#include "absl/memory/memory.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {

// A general class to control one LED light's state and color.
class LedDataSource : public DataSource {
 public:
  // This factory function creates a new LedDataSource and returns a shared_ptr.
  // If the passed in LED config is not valid returns an error.
  static ::util::StatusOr<std::shared_ptr<LedDataSource>> Make(
      const LedConfig& led_config, const SystemInterface* system_interface,
      CachePolicy* cache_policy);

  ManagedAttribute* GetLedState() { return &led_state_; }
  ManagedAttribute* GetLedColor() { return &led_color_; }

 private:
  // Reads the LED config and generate attributes accordingly.
  LedDataSource(
      const LedConfig& led_config, const SystemInterface* system_interface,
      CachePolicy* cache_type,
      const std::map<std::pair<LedState, LedColor>, std::vector<int>>* led_map);
  // Static function to get LED Map for the given LED type.
  static const ::util::StatusOr<
      std::map<std::pair<LedState, LedColor>, std::vector<int>>*>
  GetLedMap(LedType led_type);

  // For LED we never read their state. Returns OK directely.
  ::util::Status UpdateValues() override { return ::util::OkStatus(); }
  // Private function to set LED color and state.
  ::util::Status SetLedColorState(LedState state, LedColor color);

  // Verify if the given LED config is valid or not.
  static ::util::Status VerifyLedConfig(const LedConfig& led_config);

  EnumAttribute led_state_{LedState_descriptor(), this};
  EnumAttribute led_color_{LedColor_descriptor(), this};

  // A boolean type indicate if the led_state_ or led_color_ got updated. The
  // system level update will be called only when both value updated.
  bool led_state_update_ = false;
  bool led_color_update_ = false;
  // System interface to execute file write request.
  const SystemInterface* system_interface_;
  // LED data source controls multiple number of led_control_path.
  std::vector<std::string> led_control_paths_;
  // Mapping LED state & color conbination to control path value.
  const std::map<std::pair<LedState, LedColor>, std::vector<int>>* led_map_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_LED_DATASOURCE_H_
