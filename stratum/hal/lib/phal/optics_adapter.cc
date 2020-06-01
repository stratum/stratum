// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/optics_adapter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/utils.h"

namespace stratum {
namespace hal {
namespace phal {

OpticsAdapter::OpticsAdapter(AttributeDatabaseInterface* attribute_db_interface)
    : Adapter(ABSL_DIE_IF_NULL(attribute_db_interface)) {}

// PhalDb is 0-based indexed, while arguments are 1-based.
::util::Status OpticsAdapter::GetOpticalTransceiverInfo(
    int module, int network_interface,
    OpticalTransceiverInfo* ot_info) {
  if (module <= 0 || network_interface <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  std::vector<Path> paths = {
    {
      PathEntry("optical_modules", module - 1),
      PathEntry("network_interfaces", network_interface - 1, true, false, true),
    }
  };

  ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  CHECK_RETURN_IF_FALSE(phaldb->optical_modules_size() > module - 1)
      << "optical module in module " << module - 1 << " not found!";

  auto optical_module = phaldb->optical_modules(module - 1);

  CHECK_RETURN_IF_FALSE(
      optical_module.network_interfaces_size() > network_interface - 1)
      << "optical port in port " << network_interface - 1 << " not found";

  auto optical_port = optical_module.network_interfaces(network_interface - 1);

  ot_info->set_frequency(optical_port.frequency());
  ot_info->mutable_input_power()->set_instant(optical_port.input_power());
  ot_info->mutable_output_power()->set_instant(optical_port.output_power());
  ot_info->set_target_output_power(optical_port.target_output_power());
  ot_info->set_operational_mode(optical_port.operational_mode());

  return ::util::OkStatus();
}

::util::Status OpticsAdapter::SetOpticalTransceiverInfo(
    int module, int network_interface,
    const OpticalTransceiverInfo& ot_info) {
  if (module <= 0 || network_interface <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  AttributeValueMap attrs;
  Path path;

  if (ot_info.frequency()) {
    path = {PathEntry("optical_modules", module - 1),
            PathEntry("network_interfaces", network_interface - 1),
            PathEntry("frequency")};
    attrs[path] = ot_info.frequency();
  }
  if (ot_info.target_output_power()) {
    path = {PathEntry("optical_modules", module - 1),
            PathEntry("network_interfaces", network_interface - 1),
            PathEntry("target_output_power")};
    attrs[path] = ot_info.target_output_power();
  }
  if (ot_info.operational_mode()) {
    path = {PathEntry("optical_modules", module - 1),
            PathEntry("network_interfaces", network_interface - 1),
            PathEntry("operational_mode")};
    attrs[path] = ot_info.operational_mode();
  }

  return Set(attrs);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
