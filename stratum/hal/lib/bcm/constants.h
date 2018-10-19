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


#ifndef STRATUM_HAL_LIB_BCM_CONSTANTS_H_
#define STRATUM_HAL_LIB_BCM_CONSTANTS_H_

#include <stddef.h>

#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace bcm {

// Port related constants.
constexpr int kCpuLogicalPort = 0;  // Fixed for all units.

// CoS related constants.
constexpr int kDefaultCpuQueue = 0;
constexpr int kMaxCpuQueue = 7;
constexpr int kMaxCos = 7;
constexpr int kDefaultCos = 4;

// Misc constants.
constexpr int kSdkCheckpointFileSize = 50000000;
constexpr int kDefaultMaxFrameSize = 1518;
constexpr int kMaxEcmpGroupSize = 1024;
constexpr int kDefaultMtu = 1500;

// The upper 16 bits of an Acl table entry's priority reflect the table
// priority. The lower 16 bits are the entry's relative priority within the
// table.
constexpr int kAclTablePriorityRange = 1 << 16;

// KNET related constants.
constexpr uint16 kRcpuVlanEthertype = 0x8101;
constexpr uint16 kRcpuVlanId = 1;  // fixed for all RCPU headers
constexpr uint16 kRcpuEthertype = 0xde08;
constexpr uint8 kRcpuOpcodeToCpuPkt = 0x10;
constexpr uint8 kRcpuOpcodeFromCpuPkt = 0x20;
constexpr uint8 kRcpuFlagModhdr = 0x04;
constexpr size_t kRcpuRxMetaSize = 64;
constexpr size_t kRcpuTxMetaSize = 32;

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_CONSTANTS_H_
