#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_

#include "stratum/glue/openconfig/proto/old_openconfig.pb.h"
#include "stratum/glue/openconfig/proto/old_openconfig_google_bcm.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace google {
namespace hercules {
namespace hal {

class OpenconfigConverter {
 public:
  // Converts ChassisConfig to ::oc::Device.
  static ::util::StatusOr<oc::Device> ChassisConfigToOcDevice(
      const ChassisConfig& in);
  // Converts oc::Device to ChassisConfig.
  static ::util::StatusOr<ChassisConfig> OcDeviceToChassisConfig(
      const oc::Device& in);
  // Checks if oc:Device proto is consistent.
  static ::util::Status ValidateOcDeviceProto(const oc::Device& in);
};

}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_OPENCONFIG_CONVERTER_H_
