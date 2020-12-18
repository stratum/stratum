// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_

#include "absl/time/time.h"
#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace barefoot {

// TNA Extern types
constexpr uint32 kTnaExternActionProfileId = 129;
constexpr uint32 kTnaExternActionSelectorId = 130;
constexpr uint32 kTnaExternDirectCounter = 132;

// Built-in tables and fields
constexpr char kMcNodeDevPort[] = "$DEV_PORT";
constexpr char kMcNodeId[] = "$MULTICAST_NODE_ID";
constexpr char kMcNodeL1Xid[] = "$MULTICAST_NODE_L1_XID";
constexpr char kMcNodeL1XidValid[] = "$MULTICAST_NODE_L1_XID_VALID";
constexpr char kMcNodeLagId[] = "$MULTICAST_LAG_ID";
constexpr char kMcReplicationId[] = "$MULTICAST_RID";
constexpr char kMgid[] = "$MGID";
constexpr char kPreMgidTable[] = "$pre.mgid";
constexpr char kPreNodeTable[] = "$pre.node";
constexpr char kRegisterIndex[] = "$REGISTER_INDEX";

// TNA specific limits
constexpr uint16 kMaxCloneSessionId = 1015;
constexpr uint16 kMaxMulticastGroupId = 65535;
constexpr uint32 kMaxMulticastNodeId = 0x1000000;
constexpr uint64 kMaxPriority = (1u << 24) - 1;

constexpr absl::Duration kDefaultSyncTimeout = absl::Seconds(1);

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
