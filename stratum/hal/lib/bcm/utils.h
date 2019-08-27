/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


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

// Prints a BcmPort message in a consistent and readable format.
std::string PrintBcmPort(const BcmPort& p);

// Prints BcmPortOptions message in a consistent and readable format.
std::string PrintBcmPortOptions(const BcmPortOptions& options);

std::string SpeedBpsToBcmPortSpeedStr(const uint64 speed_bps);

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_UTILS_H_
