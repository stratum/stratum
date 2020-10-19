// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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

std::string PrintBcmPort(uint64 port_id, const BcmPort& p) {
  return PrintBcmPort(p);
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
  if (options.loopback_mode()) {
    buffer << sep
           << "loopback_mode: " << LoopbackState_Name(options.loopback_mode());
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintBcmChipNumber(const BcmChip::BcmChipType& chip_type) {
  switch (chip_type) {
    case BcmChip::TRIDENT_PLUS:
      return "BCM56846";
    case BcmChip::TRIDENT2:
      return "BCM56850";
    case BcmChip::TOMAHAWK:
      return "BCM56960";
    case BcmChip::TOMAHAWK_PLUS:
      return "BCM56965";
    default:
      return "UNKNOWN";
  }
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
