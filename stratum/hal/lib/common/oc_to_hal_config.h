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


#ifndef STRATUM_HAL_LIB_COMMON_OC_TO_HAL_CONFIG_H_
#define STRATUM_HAL_LIB_COMMON_OC_TO_HAL_CONFIG_H_

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/public/proto/hal.pb.h"
#include "stratum/public/proto/openconfig-goog-bcm.pb.h"
#include "stratum/public/proto/openconfig.pb.h"

namespace stratum {
namespace hal {

class OpenConfigToHalConfigProtoConverter {
 public:
  // Converts oc::Device into google::hercules::ChassisConfig.
  ::util::StatusOr<google::hercules::ChassisConfig> DeviceToChassisConfig(
      const oc::Device &in);
  // Checks if oc:Device proto is consistent.
  bool IsCorrectProtoDevice(const oc::Device &in);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_OC_TO_HAL_CONFIG_H_
