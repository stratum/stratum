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

#ifndef STRATUM_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_
#define STRATUM_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_

#include "openconfig/openconfig.pb.h"
#include "stratum/public/proto/openconfig-goog-bcm.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {

namespace hal {

class OpenconfigConverter {
 public:
  // Converts ChassisConfig to ::openconfig::Device.
  static ::util::StatusOr<openconfig::Device> ChassisConfigToOcDevice(
      const ChassisConfig& in);
  // Converts openconfig::Device to ChassisConfig.
  static ::util::StatusOr<ChassisConfig> OcDeviceToChassisConfig(
      const openconfig::Device& in);
  // Checks if oc:Device proto is consistent.
  static ::util::Status ValidateOcDeviceProto(const openconfig::Device& in);
};

}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_
