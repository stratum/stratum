// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_UTILS_H_
#define STRATUM_HAL_LIB_BCM_UTILS_H_

#include <string>

#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/glue/integral_types.h"
#include "absl/strings/str_cat.h"

namespace stratum {
namespace hal {
namespace bcm {

// Encapsulate the data required to uniquely identify a BCM port as needed by
// the BCM sdk.
struct SdkPort {
  int unit;
  int logical_port;
  SdkPort(int _unit, int _logical_port)
      : unit(_unit), logical_port(_logical_port) {}
  SdkPort() : unit(-1), logical_port(-1) {}
  bool operator<(const SdkPort& other) const {
    return (unit < other.unit ||
            (unit == other.unit && (logical_port < other.logical_port)));
  }
  bool operator==(const SdkPort& other) const {
    return (unit == other.unit && logical_port == other.logical_port);
  }
  std::string ToString() const {
    return absl::StrCat("(unit: ", unit, ", logical_port: ", logical_port, ")");
  }
};

// Encapsulate the data required to uniquely identify a BCM trunk port as needed
// by the BCM SDK.
struct SdkTrunk {
  int unit;
  int trunk_port;
  SdkTrunk(int _unit, int _trunk_port) : unit(_unit), trunk_port(_trunk_port) {}
  SdkTrunk() : unit(-1), trunk_port(-1) {}
  bool operator<(const SdkTrunk& other) const {
    return (unit < other.unit ||
            (unit == other.unit && (trunk_port < other.trunk_port)));
  }
  bool operator==(const SdkTrunk& other) const {
    return (unit == other.unit && trunk_port == other.trunk_port);
  }
  std::string ToString() const {
    return absl::StrCat("(unit: ", unit, ", trunk_port: ", trunk_port, ")");
  }
};

// Prints a BcmPort message in a consistent and readable format. There are two
// versions for this function, one taking port_id as well (if available).
std::string PrintBcmPort(const BcmPort& p);
std::string PrintBcmPort(uint64 port_id, const BcmPort& p);

// Prints BcmPortOptions message in a consistent and readable format.
std::string PrintBcmPortOptions(const BcmPortOptions& options);

// Returns the BCM chip number for a given chip. E.g. BCM56960 for Tomahawk.
std::string PrintBcmChipNumber(const BcmChip::BcmChipType& chip_type);

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_UTILS_H_
