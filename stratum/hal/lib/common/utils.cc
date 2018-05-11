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


#include "third_party/stratum/hal/lib/common/utils.h"

#include <sstream>  // IWYU pragma: keep

#include "third_party/stratum/lib/constants.h"
#include "third_party/stratum/lib/macros.h"
#include "third_party/stratum/public/lib/error.h"
#include "third_party/absl/strings/substitute.h"

namespace stratum {
namespace hal {

std::string PrintNode(const Node& n) {
  return PrintNode(n.id(), n.slot(), n.index());
}

std::string PrintNode(uint64 id, int slot, int index) {
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

std::string PrintSingletonPort(const SingletonPort& p) {
  return PrintSingletonPort(p.id(), p.slot(), p.port(), p.channel(),
                            p.speed_bps());
}

std::string PrintSingletonPort(uint64 id, int slot, int port, int channel,
                               uint64 speed_bps) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (id > 0) {
    buffer << sep << "id: " << id;
    sep = ", ";
  }
  buffer << sep << "slot: " << slot << ", port: " << port;
  sep = ", ";
  if (channel > 0) {
    buffer << ", channel: " << channel;
  }
  if (speed_bps > 0) {
    buffer << ", speed: " << speed_bps / kBitsPerGigabit << "G";
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

std::string PrintPhysicalPort(const PhysicalPort& physical_port) {
  return absl::Substitute("(slot: $0, port: $1)", physical_port.slot(),
                          physical_port.port());
}

SingletonPort PortUtils::BuildSingletonPort(int slot, int port, int channel,
                                            uint64 speed_bps) {
  SingletonPort singleton_port;
  singleton_port.set_slot(slot);
  singleton_port.set_port(port);
  singleton_port.set_channel(channel);
  singleton_port.set_speed_bps(speed_bps);
  return singleton_port;
}

PhysicalPort PortUtils::BuildPhysicalPort(int slot, int port) {
  PhysicalPort physical_port;
  physical_port.set_slot(slot);
  physical_port.set_port(port);
  return physical_port;
}

}  // namespace hal
}  // namespace stratum
