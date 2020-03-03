// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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
