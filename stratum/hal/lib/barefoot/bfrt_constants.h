// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
namespace barefoot {

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

// Tofino meters require delicate handling to implement the behavior mandated by
// the P4Runtime spec. Inside the ASIC all meter entries always "exist" and are
// set to a high, but variable and unspecified default value. To differentiate
// between these and user-given values, we use the canaries below. These values
// close to the SDE limits and well above port speed, thus should never conflict
// with user given values. Due to the internal floating point representation
// inside the SDE, any programmed value will get rounded to the nearest
// representation. Therefore we put the reset limit higher than the read limit.
constexpr uint64 kUnsetMeterThresholdRead = 1ull << 38;  // ~270 GB/s
constexpr uint64 kUnsetMeterThresholdReset = kUnsetMeterThresholdRead << 1;

// Maximum number of queues per (non-channelized 100G) port on Tofino.
constexpr int kMaxQueuesPerPort = 32;

constexpr absl::Duration kDefaultSyncTimeout = absl::Seconds(1);

// URIs for P4Runtime Translation.
constexpr int32 kTnaPortIdBitWidth = 9;
constexpr char kUriTnaPortId[] = "tna/PortId_t";
const absl::flat_hash_map<std::string, int32> kUriToBitWidth = {
    {kUriTnaPortId, kTnaPortIdBitWidth}};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_CONSTANTS_H_
