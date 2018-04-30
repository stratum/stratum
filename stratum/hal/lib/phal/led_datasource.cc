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


#include "third_party/stratum/hal/lib/phal/led_datasource.h"

#include <functional>
#include "third_party/stratum/hal/lib/phal/filepath_stringsource.h"
#include "third_party/stratum/hal/lib/phal/system_interface.h"
#include "third_party/stratum/lib/macros.h"
#include "util/gtl/map_util.h"
#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/glue/status/status_macros.h"
#include "third_party/stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {

// LED State and LED color together decide the value written to the control
// paths.
// LedMap maps the LedState and LedColor combination to the correct value that
// wrote to control paths. Each type of LED will have its own LedMap.
using LedKey = std::pair<LedState, LedColor>;
using LedMap = std::map<LedKey, std::vector<int>>;

// Reads the passed in LED config and generates attribute for each led control
// path.
// Assume LedConfig passed in should be a valid config. LED config should be
// verified before passed in.
LedDataSource::LedDataSource(const LedConfig& led_config,
                             const SystemInterface* system_interface,
                             CachePolicy* cache_type, const LedMap* led_map)
    : DataSource(cache_type),
      system_interface_(system_interface),
      led_map_(led_map) {
  for (const auto& led_control_path : led_config.led_control_path()) {
    led_control_paths_.emplace_back(led_control_path);
  }

  // Add setter for attributes.
  std::function<::util::Status(const google::protobuf::EnumValueDescriptor*)>
      set_color = [&](const google::protobuf::EnumValueDescriptor* value)
      -> ::util::Status {
    led_color_update_ = true;
    RETURN_IF_ERROR(led_color_.AssignValue(value));
    ASSIGN_OR_RETURN(
        auto led_state_index,
        led_state_.ReadValue<const protobuf::EnumValueDescriptor*>());
    RETURN_IF_ERROR(
        SetLedColorState(static_cast<LedState>(led_state_index->index()),
                         static_cast<LedColor>(value->index())));
    return ::util::OkStatus();
  };
  std::function<::util::Status(const google::protobuf::EnumValueDescriptor*)>
      set_state = [&](const google::protobuf::EnumValueDescriptor* value)
      -> ::util::Status {
    led_state_update_ = true;
    RETURN_IF_ERROR(led_state_.AssignValue(value));
    ASSIGN_OR_RETURN(
        auto led_color_index,
        led_color_.ReadValue<const protobuf::EnumValueDescriptor*>());
    RETURN_IF_ERROR(
        SetLedColorState(static_cast<LedState>(value->index()),
                         static_cast<LedColor>(led_color_index->index())));
    return ::util::OkStatus();
  };
  led_state_.AddSetter(set_state);
  led_color_.AddSetter(set_color);
}

::util::StatusOr<std::shared_ptr<LedDataSource>> LedDataSource::Make(
    const LedConfig& led_config, const SystemInterface* system_interface,
    CachePolicy* cache_policy) {
  ::util::Status status = VerifyLedConfig(led_config);
  RETURN_IF_ERROR(VerifyLedConfig(led_config))
      << "Invalid LED config, Failed to make LED DataSource.";
  ASSIGN_OR_RETURN(LedMap* led_map, GetLedMap(led_config.led_type()));
  return std::shared_ptr<LedDataSource>(
      new LedDataSource(led_config, system_interface, cache_policy, led_map));
}

::util::Status LedDataSource::VerifyLedConfig(const LedConfig& led_config) {
  // Check that there is LedMap corresponding to this type of LED.
  ASSIGN_OR_RETURN(LedMap* led_map, GetLedMap(led_config.led_type()));
  CHECK_RETURN_IF_FALSE(led_map != nullptr)
      << "Failed to find led map for led_type :"
      << LedType_Name(led_config.led_type());
  int led_control_path_in_map = led_map->begin()->second.size();
  int led_control_path_in_config = led_config.led_control_path_size();
  if (led_control_path_in_map != led_control_path_in_config) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Control path size mismatch. ";
  }
  return ::util::OkStatus();
}

::util::Status LedDataSource::SetLedColorState(LedState state, LedColor color) {
  // Set LED color and state when all of them are updated to new color.
  if (!led_color_update_ || !led_state_update_) {
    return ::util::OkStatus();
  }
  // Reset led_state/color update indicator when we get both values even
  // if the given state&color combination is not valid.
  led_color_update_ = false;
  led_state_update_ = false;
  const std::vector<int>* control_value_list =
      gtl::FindOrNull(*led_map_, LedKey(state, color));
  CHECK_RETURN_IF_FALSE(control_value_list != nullptr)
      << "LED does not support state :" << LedState_Name(state)
      << ", color: " << LedColor_Name(color);
  // Sets the attribute value according to the list of expected attribute
  // value.
  for (int i = 0; i < control_value_list->size(); i++) {
    RETURN_IF_ERROR(system_interface_->WriteStringToFile(
        std::to_string(control_value_list->at(i)), led_control_paths_[i]));
  }

  return ::util::OkStatus();
}

const ::util::StatusOr<LedMap*> LedDataSource::GetLedMap(LedType led_type) {
  switch (led_type) {
    // TODO : Add more LedType to this list.
    case LedType::BICOLOR_FPGA_G_R: {
      static LedMap* led_map =
          new LedMap{{LedKey(LedState::OFF, LedColor::GREEN), {1, 1}},
                     {LedKey(LedState::OFF, LedColor::RED), {1, 1}},
                     {LedKey(LedState::SOLID, LedColor::GREEN), {0, 1}},
                     {LedKey(LedState::SOLID, LedColor::RED), {1, 0}}};
      return led_map;
    }
    case LedType::TRICOLOR_FPGA_GR_GY: {
      static LedMap* led_map = new LedMap{
          {LedKey(LedState::OFF, LedColor::GREEN), {1, 1, 1, 1}},
          {LedKey(LedState::OFF, LedColor::RED), {1, 1, 1, 1}},
          {LedKey(LedState::OFF, LedColor::AMBER), {1, 1, 1, 1}},
          {LedKey(LedState::SOLID, LedColor::GREEN), {0, 1, 1, 1}},
          {LedKey(LedState::SOLID, LedColor::RED), {1, 0, 1, 1}},
          {LedKey(LedState::SOLID, LedColor::AMBER), {1, 1, 1, 0}},
      };
      return led_map;
    }
    case LedType::TRICOLOR_FPGA_GR_Y: {
      static LedMap* led_map = new LedMap{
          {LedKey(LedState::OFF, LedColor::GREEN), {0, 0}},
          {LedKey(LedState::OFF, LedColor::RED), {0, 0}},
          {LedKey(LedState::OFF, LedColor::AMBER), {0, 0}},
          {LedKey(LedState::SOLID, LedColor::GREEN), {1, 0}},
          {LedKey(LedState::SOLID, LedColor::RED), {2, 0}},
          {LedKey(LedState::SOLID, LedColor::AMBER), {0, 1}},
      };
      return led_map;
    }
    case LedType::TRICOLOR_FPGA_G_R_Y: {
      static LedMap* led_map = new LedMap{
          {LedKey(LedState::OFF, LedColor::GREEN), {1, 1, 1}},
          {LedKey(LedState::OFF, LedColor::RED), {1, 1, 1}},
          {LedKey(LedState::OFF, LedColor::AMBER), {1, 1, 1}},
          {LedKey(LedState::SOLID, LedColor::GREEN), {0, 1, 1}},
          {LedKey(LedState::SOLID, LedColor::RED), {1, 0, 1}},
          {LedKey(LedState::SOLID, LedColor::AMBER), {1, 1, 0}},
      };
      return led_map;
    }
    case LedType::BICOLOR_GPIO_G_R: {
      static LedMap* led_map = new LedMap{
          {LedKey(LedState::OFF, LedColor::GREEN), {0, 0}},
          {LedKey(LedState::OFF, LedColor::RED), {0, 0}},
          {LedKey(LedState::SOLID, LedColor::GREEN), {1, 0}},
          {LedKey(LedState::SOLID, LedColor::RED), {0, 1}},
      };
      return led_map;
    }
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Fail to initialize LED map for " << LedType_Name(led_type);
  }
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
