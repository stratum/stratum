// Copyright 2020 Open Networking Foundation
// Copyright 2020 PLVision
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
    int slot, int port, OpticalChannelInfo* oc_info) {
  if (slot <= 0 || port <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  std::vector<Path> paths = {
      {PathEntry("optical_modules", slot - 1),
       PathEntry("ports", port - 1, true, false, true)}};

  ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  CHECK_RETURN_IF_FALSE(phaldb->optical_modules_size() > slot - 1)
      << "optical module in slot " << slot - 1 << " not found!";

  auto optical_module = phaldb->optical_modules(slot - 1);

  CHECK_RETURN_IF_FALSE(optical_module.ports_size() > port - 1)
      << "optical port in port " << port - 1 << " not found";

  auto optical_port = optical_module.ports(port - 1);
  oc_info->set_frequency(optical_port.frequency());

  oc_info->mutable_input_power()->set_instant(optical_port.input_power());

  oc_info->mutable_output_power()->set_instant(optical_port.output_power());

  oc_info->set_target_output_power(
      optical_port.target_output_power());

  oc_info->set_operational_mode(optical_port.operational_mode());

  return ::util::OkStatus();
}

::util::Status OpticsAdapter::SetOpticalTransceiverInfo(
    int slot, int port, const OpticalChannelInfo& oc_info) {
  if (slot <= 0 || port <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  AttributeValueMap attrs;
  Path path;

  if (oc_info.frequency()) {
    path = {PathEntry("optical_modules", slot - 1),
            PathEntry("ports", port - 1),
            PathEntry("frequency")};
    attrs[path] = oc_info.frequency();
  }
  if (oc_info.target_output_power()) {
    path = {PathEntry("optical_modules", slot - 1),
            PathEntry("ports", port - 1),
            PathEntry("target_output_power")};
    attrs[path] = oc_info.target_output_power();
  }
  if (oc_info.operational_mode()) {
    path = {PathEntry("optical_modules", slot - 1),
            PathEntry("ports", port - 1),
            PathEntry("operational_mode")};
    attrs[path] = oc_info.operational_mode();
  }

  return Set(attrs);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
