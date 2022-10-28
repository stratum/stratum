// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_CONSTANTS_H_
#define STRATUM_HAL_LIB_TDI_TDI_CONSTANTS_H_

#include "absl/time/time.h"
#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace tdi {

// TNA Extern types
constexpr uint32 kTnaExternActionProfileId = 129;
constexpr uint32 kTnaExternActionSelectorId = 130;
constexpr uint32 kTnaExternDirectCounter = 132;

// Built-in table and field names.
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
constexpr char kMeterIndex[] = "$METER_INDEX";
constexpr char kMeterCirKbps[] = "$METER_SPEC_CIR_KBPS";
constexpr char kMeterCommitedBurstKbits[] = "$METER_SPEC_CBS_KBITS";
constexpr char kMeterPirKbps[] = "$METER_SPEC_PIR_KBPS";
constexpr char kMeterPeakBurstKbits[] = "$METER_SPEC_PBS_KBITS";
constexpr char kMeterCirPps[] = "$METER_SPEC_CIR_PPS";
constexpr char kMeterCommitedBurstPackets[] = "$METER_SPEC_CBS_PKTS";
constexpr char kMeterPirPps[] = "$METER_SPEC_PIR_PPS";
constexpr char kMeterPeakBurstPackets[] = "$METER_SPEC_PBS_PKTS";
constexpr char kCounterIndex[] = "$COUNTER_INDEX";
constexpr char kCounterBytes[] = "$COUNTER_SPEC_BYTES";
constexpr char kCounterPackets[] = "$COUNTER_SPEC_PKTS";
constexpr char kMirrorConfigTable[] = "$mirror.cfg";
constexpr char kMatchPriority[] = "$MATCH_PRIORITY";
constexpr char kActionMemberId[] = "$ACTION_MEMBER_ID";
constexpr char kSelectorGroupId[] = "$SELECTOR_GROUP_ID";
constexpr char kActionMemberStatus[] = "$ACTION_MEMBER_STATUS";

// TNA specific limits
constexpr uint16 kMaxCloneSessionId = 1015;
constexpr uint16 kMaxMulticastGroupId = 65535;
constexpr uint32 kMaxMulticastNodeId = 0x1000000;
constexpr uint64 kMaxPriority = (1u << 24) - 1;

constexpr absl::Duration kDefaultSyncTimeout = absl::Seconds(1);

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_CONSTANTS_H_
