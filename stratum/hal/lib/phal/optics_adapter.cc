// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2019 Dell EMC
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

DEFINE_int32(max_num_transceiver_writers, 2,
             "Maximum number of channel writers for transceiver events.");

namespace stratum {
namespace hal {
namespace phal {

OpticsAdapter::OpticsAdapter(AttributeDatabaseInterface* attribute_db_interface)
    : Adapter(ABSL_DIE_IF_NULL(attribute_db_interface)) {}

::util::Status OpticsAdapter::GetOpticalTransceiverInfo(
    int slot, bool some_state) {
  if (slot <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot: " << slot;
  }

  // std::vector<Path> paths = {
  //     // PhalDb uses 0-based index, while BcmPort::slot is 1-based.
  //     {PathEntry("cards", card_id - 1), PathEntry("ports", port_id - 1),
  //      PathEntry("transceiver", -1, false, false, true)}};

  // // Get PhalDB entry for this port
  // ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  // // Get card
  // CHECK_RETURN_IF_FALSE(phaldb->cards_size() > card_id - 1)
  //     << "cards[" << card_id << "]"
  //     << " not found!";

  // auto card = phaldb->cards(card_id - 1);

  // // Get port
  // CHECK_RETURN_IF_FALSE(card.ports_size() > port_id - 1)
  //     << "cards[" << card_id << "]/ports[" << port_id << "]"
  //     << " not found!";

  // auto phal_port = card.ports(port_id - 1);

  // // Get the OPTICS (transceiver)
  // if (!phal_port.has_transceiver()) {
  //   RETURN_ERROR() << "cards[" << card_id << "]/ports[" << port_id
  //                  << "] has no transceiver";
  // }
  // auto optics = phal_port.transceiver();

  // // Convert HW state and don't continue if not present
  // fp_port_info->set_hw_state(optics.hardware_state());
  // if (fp_port_info->hw_state() == HW_STATE_NOT_PRESENT) {
  //   return ::util::OkStatus();
  // }

  // // Need to map connector_type to PhysicalPortType
  // PhysicalPortType actual_val;
  // switch (optics.connector_type()) {
  //   case OPTICS_TYPE_OPTICS:
  //     actual_val = PHYSICAL_PORT_TYPE_OPTICS_CAGE;
  //     break;
  //   case OPTICS_TYPE_QOPTICS_PLUS:
  //   case OPTICS_TYPE_QOPTICS:
  //   case OPTICS_TYPE_QOPTICS28:
  //     actual_val = PHYSICAL_PORT_TYPE_QOPTICS_CAGE;
  //     break;
  //   default:
  //     RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid opticstype. ";
  // }
  // fp_port_info->set_physical_port_type(actual_val);

  // fp_port_info->set_media_type(optics.media_type());

  // if (optics.has_info()) {
  //   fp_port_info->set_vendor_name(optics.info().mfg_name());
  //   fp_port_info->set_part_number(optics.info().part_no());
  //   fp_port_info->set_serial_number(optics.info().serial_no());
  // }

  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
