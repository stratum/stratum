// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_

#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace barefoot {

// TNA Extern types
constexpr uint32 kTnaExternActionProfileId = 129;
constexpr uint32 kTnaExternActionSelectorId = 130;
constexpr uint32 kTnaExternDirectCounter = 132;

// Built-in tables and fields
constexpr char kPreNodeTable[] = "$pre.node";
constexpr char kMcNodeId[] = "$MULTICAST_NODE_ID";
constexpr char kMcNodeDevPort[] = "$DEV_PORT";
constexpr char kPreMgidTable[] = "$pre.mgid";
constexpr char kMgid[] = "$MGID";
constexpr char kMcNodeL1XidValid[] = "$MULTICAST_NODE_L1_XID_VALID";
constexpr char kMcNodeL1Xid[] = "$MULTICAST_NODE_L1_XID";

constexpr int kDefaultSyncTimeout = 1;  // Second

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
