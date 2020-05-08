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
    int slot, int port, OpticalChannelInfo* oc_info) {
  if (slot <= 0 || port <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  std::vector<Path> paths = {
      {PathEntry("optical_cards", slot - 1, false, false, true)}};

  ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  CHECK_RETURN_IF_FALSE(phaldb->optical_cards_size() > slot - 1)
      << "optical card in slot " << slot - 1 << " not found!";

  oc_info->set_frequency(phaldb->optical_cards(slot - 1).frequency());

  oc_info->mutable_input_power()->set_instant(
      phaldb->optical_cards(slot - 1).input_power());

  oc_info->mutable_output_power()->set_instant(
      phaldb->optical_cards(slot - 1).output_power());

  oc_info->set_target_output_power(
      phaldb->optical_cards(slot - 1).target_output_power());

  oc_info->set_operational_mode(
      phaldb->optical_cards(slot - 1).operational_mode());

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
    path = {PathEntry("optical_cards", slot - 1), PathEntry("frequency")};
    attrs[path] = oc_info.frequency();
  }
  if (oc_info.target_output_power()) {
    path = {PathEntry("optical_cards", slot - 1),
            PathEntry("target_output_power")};
    attrs[path] = oc_info.target_output_power();
  }
  if (oc_info.operational_mode()) {
    path = {PathEntry("optical_cards", slot - 1),
            PathEntry("operational_mode")};
    attrs[path] = oc_info.operational_mode();
  }

  return Set(attrs);
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
