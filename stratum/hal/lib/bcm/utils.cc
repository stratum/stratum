// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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


#include "stratum/hal/lib/bcm/utils.h"

#include <sstream>  // IWYU pragma: keep

#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/constants.h"

namespace stratum {
namespace hal {
namespace bcm {

std::string PrintBcmPort(const BcmPort& p) {
  return PrintPortProperties(/*node_id=*/0, /*port_id=*/0, p.slot(), p.port(),
                             p.channel(), p.unit(), p.logical_port(),
                             p.speed_bps());
}

namespace {

std::string PrintTriState(TriState state) {
  return (state ? (state == TRI_STATE_TRUE ? "true" : "false") : "unknown");
}

}  // namespace

std::string PrintBcmPortOptions(const BcmPortOptions& options) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (options.enabled()) {
    buffer << sep << "enabled: " << PrintTriState(options.enabled());
    sep = ", ";
  }
  if (options.blocked()) {
    buffer << sep << "blocked: " << PrintTriState(options.blocked());
    sep = ", ";
  }
  if (options.flex()) {
    buffer << sep << "flex: " << PrintTriState(options.flex());
    sep = ", ";
  }
  if (options.autoneg()) {
    buffer << sep << "autoneg: " << PrintTriState(options.autoneg());
    sep = ", ";
  }
  if (options.speed_bps() > 0) {
    buffer << sep << "speed: " << options.speed_bps() / kBitsPerGigabit << "G";
    sep = ", ";
  }
  if (options.max_frame_size() > 0) {
    buffer << sep << "max_frame_size: " << options.max_frame_size();
    sep = ", ";
  }

  if (options.num_serdes_lanes() > 0) {
    buffer << sep << "num_serdes_lanes: " << options.num_serdes_lanes();
    sep = ", ";
  }
  if (options.linkscan_mode()) {
    buffer << sep << "linkscan_mode: "
           << BcmPortOptions::LinkscanMode_Name(options.linkscan_mode());
  }
  buffer << ")";

  return buffer.str();
}

std::string SpeedBpsToBcmPortSpeedStr(const uint64 speed_bps) {
  switch(speed_bps) {
    case 10000000000: return "PC_PORT_OPMODE_10G";
    case 12000000000: return "PC_PORT_OPMODE_12G";
    case 13000000000: return "PC_PORT_OPMODE_13G";
    case 20000000000: return "PC_PORT_OPMODE_20G";
    case 25000000000: return "PC_PORT_OPMODE_25G";
    case 21000000000: return "PC_PORT_OPMODE_21G";
    case 40000000000: return "PC_PORT_OPMODE_40G";
    case 42000000000: return "PC_PORT_OPMODE_42G";
    case 50000000000: return "PC_PORT_OPMODE_50G";
    case 100000000000: return "PC_PORT_OPMODE_100G";
    case 120000000000: return "PC_PORT_OPMODE_120G";
    case 127000000000: return "PC_PORT_OPMODE_127G";
    case 200000000000: return "PC_PORT_OPMODE_200G";
    case 400000000000: return "PC_PORT_OPMODE_400G";
    default: return "PC_PORT_OPMODE_ANY";
  }
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
