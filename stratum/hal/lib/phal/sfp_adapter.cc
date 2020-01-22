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

#include "stratum/hal/lib/phal/sfp_adapter.h"

#include <vector>

#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {
namespace phal {

SfpAdapter::SfpAdapter(AttributeDatabaseInterface* attribute_db_interface)
    : Adapter(ABSL_DIE_IF_NULL(attribute_db_interface)) {}

::util::Status SfpAdapter::GetFrontPanelPortInfo(
    int card_id, int port_id, FrontPanelPortInfo* fp_port_info) {
  if (card_id <= 0 || port_id <= 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid Slot/Port value. ";
  }

  std::vector<Path> paths = {
      // PhalDb uses 0-based index, while BcmPort::slot is 1-based.
      {PathEntry("cards", card_id - 1), PathEntry("ports", port_id - 1),
       PathEntry("transceiver", -1, false, false, true)}};

  // Get PhalDB entry for this port
  ASSIGN_OR_RETURN(auto phaldb, Get(paths));

  // Get card
  CHECK_RETURN_IF_FALSE(phaldb->cards_size() > card_id - 1)
    << "cards[" << card_id << "]" << " not found!";

  auto card = phaldb->cards(card_id - 1);

  // Get port
  CHECK_RETURN_IF_FALSE(card.ports_size() > port_id - 1)
      << "cards[" << card_id << "]/ports[" << port_id << "]"
      << " not found!";

  auto phal_port = card.ports(port_id - 1);

  // Get the SFP (transceiver)
  if (!phal_port.has_transceiver()) {
    RETURN_ERROR() << "cards[" << card_id << "]/ports[" << port_id
                   << "] has no transceiver";
  }
  auto sfp = phal_port.transceiver();

  // Convert HW state and don't continue if not present
  fp_port_info->set_hw_state(sfp.hardware_state());
  if (fp_port_info->hw_state() == HW_STATE_NOT_PRESENT) {
    return ::util::OkStatus();
  }

  // Need to map connector_type to PhysicalPortType
  PhysicalPortType actual_val;
  switch (sfp.connector_type()) {
    case SFP_TYPE_SFP:
      actual_val = PHYSICAL_PORT_TYPE_SFP_CAGE;
      break;
    case SFP_TYPE_QSFP_PLUS:
    case SFP_TYPE_QSFP:
    case SFP_TYPE_QSFP28:
      actual_val = PHYSICAL_PORT_TYPE_QSFP_CAGE;
      break;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid sfptype. ";
  }
  fp_port_info->set_physical_port_type(actual_val);

  fp_port_info->set_media_type(sfp.media_type());

  if (sfp.has_info()) {
    fp_port_info->set_vendor_name(sfp.info().mfg_name());
    fp_port_info->set_part_number(sfp.info().part_no());
    fp_port_info->set_serial_number(sfp.info().serial_no());
  }

  return ::util::OkStatus();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
