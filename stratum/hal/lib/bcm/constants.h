// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


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
constexpr int kSdkCheckpointFileSizeTomahawk = 86000000; // TODO(max): cleanup
// constexpr int kSdkCheckpointFileSizeTrident2 = 5000000;
constexpr int kSdkCheckpointFileSizeTrident2 = 50000000;
constexpr int kDefaultMaxFrameSize = 1518;
constexpr int kMaxEcmpGroupSize = 1024;
constexpr int kDefaultMtu = 1500;
constexpr int kCloneSessionId = 511;
constexpr int kDefaultVlanStgId = 1;

// The upper 16 bits of an Acl table entry's priority reflect the table
// priority. The lower 16 bits are the entry's relative priority within the
// table.
// TODO(max): ACL priority handling has changed in SDKLT. Manual partitioning is
// no longer necessary and the bitwidths of the fields have changed.
constexpr int kAclTablePriorityRange = 1 << 16;

// KNET related constants.
// TODO(max): 802.1Q says 0x8100 is used as TPID, switch Rx packets also use
// this value
// constexpr uint16 kRcpuVlanEthertype = 0x8101;
constexpr uint16 kRcpuVlanEthertype = 0x8100;
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
