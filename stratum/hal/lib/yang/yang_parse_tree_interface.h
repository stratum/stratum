// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_INTERFACE_H_
#define STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_INTERFACE_H_

#include <string>

#include "stratum/glue/integral_types.h"

namespace stratum { namespace hal { class TreeNode; } }
namespace stratum { namespace hal { class YangParseTree; } }

namespace stratum {
namespace hal {
namespace yang {
namespace interface {

void SetUpInterfacesInterfaceStateLastChange(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateIfindex(
    uint32 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateName(
    const std::string& name, TreeNode* node);

void SetUpInterfacesInterfaceStateOperStatus(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateAdminStatus(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateLoopbackMode(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateHardwarePort(
    const std::string& name, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceEthernetStatePortSpeed(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceEthernetStateNegotiatedPortSpeed(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInOctets(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutOctets(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInUnicastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutUnicastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInBroadcastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutBroadcastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInMulticastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutMulticastPkts(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInDiscards(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutDiscards(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInUnknownProtos(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersOutErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateCountersInFcsErrors(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpLacpInterfacesInterfaceStateSystemPriority(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceConfigHealthIndicator(
    const std::string& state, uint64 node_id, uint64 port_id,
    TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceStateHealthIndicator(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceEthernetConfigForwardingViability(
    uint64 node_id, uint32 port_id, bool forwarding_viability, TreeNode* node,
    YangParseTree* tree);

void SetUpInterfacesInterfaceEthernetStateForwardingViability(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceEthernetStateAutoNegotiate(
    uint64 node_id, uint32 port_id, TreeNode* node, YangParseTree* tree);

void SetUpQosInterfacesInterfaceOutputQueuesQueueStateName(
    const std::string& name, TreeNode* node);

void SetUpQosInterfacesInterfaceOutputQueuesQueueStateId(
    uint64 node_id, uint32 port_id, uint32 queue_id,
    TreeNode* node, YangParseTree* tree);

void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree);

void SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitOctets(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree);

void SetUpQosInterfacesInterfaceOutputQueuesQueueStateDroppedPkts(
    uint64 node_id, uint32 port_id, uint32 queue_id, TreeNode* node,
    YangParseTree* tree);

void SetUpQosQueuesQueueConfigId(
    uint32 queue_id, TreeNode* node, YangParseTree* tree);

void SetUpQosQueuesQueueStateId(
    uint32 queue_id, TreeNode* node, YangParseTree* tree);

}  // namespace interface
}  // namespace yang
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_YANG_YANG_PARSE_TREE_INTERFACE_H_
