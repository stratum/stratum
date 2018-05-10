/*
 * Copyright 2018 Google LLC
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


#ifndef STRATUM_HAL_LIB_COMMON_CONSTANTS_H_
#define STRATUM_HAL_LIB_COMMON_CONSTANTS_H_

#include <stddef.h>

#include "absl/base/integral_types.h"

namespace stratum {
namespace hal {

// Invalid handle value for an event writer.
constexpr int kInvalidWriterId = -1;

// Special VRFs used by controller.
constexpr int kVrfDefault = 0;
constexpr int kVrfFallback = 0xffff;
constexpr int kVrfOverride = 0xfffe;
constexpr int kVrfMin = 0;
constexpr int kVrfMax = 0xffff;

// VLAN related constants.
constexpr size_t kVlanIdSize = 2;
constexpr size_t kVlanTagSize = 4;
constexpr uint16 kVlanIdMask = 0x0fff;
constexpr uint16 kDefaultVlan = 1;
constexpr uint16 kArpVlan = 4050;

// CPU port ID. This is a reserved port ID for CPU port (e.g. used as egress
// port by default for all the packets punted to CPU). This value cannot be
// used as ID for any other port.
// This value must match CPU_PORT in
// TODO(fix path to parser): p4/spec/parser.p4
constexpr uint64 kCpuPortId = 0xFFFFFFFD;

// Constant broadcast MAC.
constexpr uint64 kBroadcastMac = 0xFFFFFFFFFFFF;

// Names of the packet in and packet out controller metadata preambles in
// P4Info.
constexpr char kIngressMetadataPreambleName[] = "packet_in";
constexpr char kEgressMetadataPreambleName[] = "packet_out";

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_CONSTANTS_H_
