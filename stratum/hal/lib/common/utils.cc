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


#include "stratum/hal/lib/common/utils.h"

#include <sstream>  // IWYU pragma: keep

#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

std::string PrintNode(const Node& n) {
  return PrintNodeProperties(n.id(), n.slot(), n.index());
}

std::string PrintSingletonPort(const SingletonPort& p) {
  return PrintPortProperties(p.node(), p.id(), p.slot(), p.port(), p.channel(),
                             /*unit=*/-1, /*logical_port=*/-1, p.speed_bps());
}

std::string PrintTrunkPort(const TrunkPort& p) {
  return PrintTrunkProperties(p.node(), p.id(),
                              /*unit=*/-1, /*trunk_port=*/-1, /*speed_bps*/ 0);
}

std::string PrintNodeProperties(uint64 id, int slot, int index) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (id > 0) {
    buffer << sep << "id: " << id;
    sep = ", ";
  }
  buffer << sep << "slot: " << slot;
  if (index > 0) {
    buffer << ", index: " << index;
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintPortProperties(uint64 node_id, uint32 port_id, int slot,
                                int port, int channel, int unit,
                                int logical_port, uint64 speed_bps) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (node_id > 0) {
    buffer << sep << "node_id: " << node_id;
    sep = ", ";
  }
  if (port_id > 0) {
    buffer << sep << "port_id: " << port_id;
    sep = ", ";
  }
  buffer << sep << "slot: " << slot << ", port: " << port;
  sep = ", ";
  if (channel > 0) {
    buffer << ", channel: " << channel;
  }
  if (unit >= 0) {
    buffer << ", unit: " << unit;
  }
  if (logical_port >= 0) {
    buffer << ", logical_port: " << logical_port;
  }
  if (speed_bps > 0) {
    buffer << ", speed: " << speed_bps / kBitsPerGigabit << "G";
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintTrunkProperties(uint64 node_id, uint32 trunk_id, int unit,
                                 int trunk_port, uint64 speed_bps) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (node_id > 0) {
    buffer << sep << "node_id: " << node_id;
    sep = ", ";
  }
  if (trunk_id > 0) {
    buffer << sep << "trunk_id: " << trunk_id;
    sep = ", ";
  }
  if (unit >= 0) {
    buffer << sep << "unit: " << unit;
    sep = ", ";
  }
  if (trunk_port >= 0) {
    buffer << sep << "trunk_port: " << trunk_port;
    sep = ", ";
  }
  if (speed_bps > 0) {
    buffer << sep << "speed: " << speed_bps / kBitsPerGigabit << "G";
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintPortState(PortState state) {
  switch (state) {
    case PORT_STATE_UP:
      return "UP";
    case PORT_STATE_DOWN:
      return "DOWN";
    case PORT_STATE_FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

SingletonPort BuildSingletonPort(int slot, int port, int channel,
                                 uint64 speed_bps) {
  SingletonPort singleton_port;
  singleton_port.set_slot(slot);
  singleton_port.set_port(port);
  singleton_port.set_channel(channel);
  singleton_port.set_speed_bps(speed_bps);
  return singleton_port;
}

PortLedConfig FindPortLedColorAndState(AdminState admin_state,
                                       PortState oper_state,
                                       HealthState health_state,
                                       TrunkMemberBlockState block_state) {
  if (admin_state != ADMIN_STATE_ENABLED) {
    // Admin disabled overrides other states.
    return {LED_COLOR_AMBER, LED_STATE_SOLID};
  } else if (oper_state != PORT_STATE_UP) {
    // A port which is admin enabled but oper down. We turn off the LEDs in this
    // case.
    return {LED_COLOR_GREEN, LED_STATE_OFF};
  } else if (block_state == TRUNK_MEMBER_BLOCK_STATE_BLOCKED) {
    // A port which is admin enabled, oper up, part of a trunk, and blocked
    // (e.g., as part of LACP protocol). Note that if the port is not part of a
    // trunk block_state will be TRUNK_MEMBER_BLOCK_STATE_UNKNOWN.
    return {LED_COLOR_GREEN, LED_STATE_BLINKING_SLOW};
  } else if (health_state == HEALTH_STATE_GOOD) {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and healthy (e.g. no neighbor
    // mismatch detected).
    return {LED_COLOR_GREEN, LED_STATE_SOLID};
  } else if (health_state == HEALTH_STATE_BAD) {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and unhealthy (e.g. there is a
    // neighbor mismatch).
    return {LED_COLOR_AMBER, LED_STATE_BLINKING_FAST};
  } else {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and has unknown health state
    // (e.g. when the neighbor status of the port is not known to controller).
    return {LED_COLOR_GREEN, LED_STATE_BLINKING_FAST};
  }
}

PortLedConfig AggregatePortLedColorsStatePairs(
    const std::vector<PortLedConfig>& color_state_pairs) {
  if (color_state_pairs.empty()) {
    return {LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN};
  }

  auto it = color_state_pairs.begin();
  LedColor aggregate_color = it->first;
  LedState aggregate_state = it->second;
  it++;
  while (it != color_state_pairs.end()) {
    if (aggregate_color != it->first || aggregate_state != it->second) {
      // If we have a conflict, show blinking amber if there is at least one
      // blinking amber and show solid amber otherwise.
      if ((aggregate_color == LED_COLOR_AMBER &&
           (aggregate_state == LED_STATE_BLINKING_SLOW ||
            aggregate_state == LED_STATE_BLINKING_FAST)) ||
          (it->first == LED_COLOR_AMBER &&
           (it->second == LED_STATE_BLINKING_SLOW ||
            it->second == LED_STATE_BLINKING_FAST))) {
        aggregate_state = LED_STATE_BLINKING_SLOW;
      } else {
        aggregate_state = LED_STATE_SOLID;
      }
      aggregate_color = LED_COLOR_AMBER;
    }
    it++;
  }

  return {aggregate_color, aggregate_state};
}

}  // namespace hal
}  // namespace stratum
