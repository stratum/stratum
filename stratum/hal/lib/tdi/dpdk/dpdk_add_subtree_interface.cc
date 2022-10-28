// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Implements the DPDK-specific YangParseTreePaths::AddSubtreeInterface() method.
// The supporting functions are in other files.

#include "stratum/hal/lib/yang/yang_parse_tree_paths.h"

#include <string>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_publisher.h"
#include "stratum/hal/lib/tdi/dpdk/dpdk_parse_tree_interface.h"
#include "stratum/hal/lib/yang/yang_parse_tree.h"
#include "stratum/hal/lib/yang/yang_parse_tree_interface.h"

namespace stratum {
namespace hal {

using namespace ::stratum::hal::yang::interface;

// Paths of leaves created by this method are defined manually by analysing
// existing YANG model files. They are hard-coded, and, as the YANG language
// does not provide a means to express leaves' semantics, their mapping to code
// implementing their function is also done manually.
// TODO(b/70300012): Implement a tool that will help to generate this code.

TreeNode* YangParseTreePaths::AddSubtreeInterface(
    const std::string& name, uint64 node_id, uint32 port_id,
    const NodeConfigParams& node_config, YangParseTree* tree) {
  // No need to lock the mutex - it is locked by method calling this one.
  uint64 mac_address = kDummyMacAddress;
 
  TreeNode* node = tree->AddNode(
      GetPath("interfaces")("virtual-interface", name)("config")("ifindex")());
  SetUpInterfacesInterfaceStateIfindex(node_id, port_id, node, tree);

  node = tree->AddNode(
      GetPath("interfaces")("virtual-interface", name)("state")("name")());
  SetUpInterfacesInterfaceStateName(name, node);

  node = tree->AddNode(
      GetPath("interfaces")("virtual-interface", name)("state")("admin-status")());
  SetUpInterfacesInterfaceStateAdminStatus(node_id, port_id, node, tree);

  // In most cases the TARGET_DEFINED mode is changed into ON_CHANGE mode as
  // this mode is the least resource-hungry. But to make the gNMI demo more
  // realistic it is changed to SAMPLE with the period of 10s.
  // TODO(tmadejski) remove/update this functor once the support for reading
  // counters is implemented.
  tree->AddNode(GetPath("interfaces")("virtual-interface", name)("config")("counters")())
      ->SetTargetDefinedMode(tree->GetStreamSampleModeFunc());

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-octets")());
  SetUpInterfacesInterfaceStateCountersInOctets(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-octets")());
  SetUpInterfacesInterfaceStateCountersOutOctets(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-unicast-pkts")());
  SetUpInterfacesInterfaceStateCountersInUnicastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-unicast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutUnicastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-broadcast-pkts")());
  SetUpInterfacesInterfaceStateCountersInBroadcastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-broadcast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutBroadcastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-multicast-pkts")());
  SetUpInterfacesInterfaceStateCountersInMulticastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-multicast-pkts")());
  SetUpInterfacesInterfaceStateCountersOutMulticastPkts(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-discards")());
  SetUpInterfacesInterfaceStateCountersInDiscards(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-discards")());
  SetUpInterfacesInterfaceStateCountersOutDiscards(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-unknown-protos")());
  SetUpInterfacesInterfaceStateCountersInUnknownProtos(
      node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-errors")());
  SetUpInterfacesInterfaceStateCountersInErrors(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("out-errors")());
  SetUpInterfacesInterfaceStateCountersOutErrors(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("counters")("in-fcs-errors")());
  SetUpInterfacesInterfaceStateCountersInFcsErrors(node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("host-name")());
  SetUpInterfacesInterfaceConfigHost("", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("port-type")());
  SetUpInterfacesInterfaceConfigPortType(
      DpdkPortType::PORT_TYPE_NONE, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("device-type")());
  SetUpInterfacesInterfaceConfigDeviceType(
      DpdkPortType::PORT_TYPE_NONE, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("pipeline-name")());
  SetUpInterfacesInterfaceConfigPipelineName("", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("mempool-name")());
  SetUpInterfacesInterfaceConfigMempoolName("", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("control-port")());
  SetUpInterfacesInterfaceConfigControlPort("", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("pci-bdf")());
  SetUpInterfacesInterfaceConfigPciBdf("", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("mtu")());
  SetUpInterfacesInterfaceConfigMtuValue(0, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("queues")());
  SetUpInterfacesInterfaceConfigQueues(0, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("socket-path")());
  SetUpInterfacesInterfaceConfigSocket("/", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("packet-dir")());
  SetUpInterfacesInterfaceConfigPacketDir(
      PacketDirection::DIRECTION_NONE, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")("virtual-interface", name)
                       ("config")("qemu-socket-ip")());
  SetUpInterfacesInterfaceConfigQemuSocketIp("/", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-socket-port")());
  SetUpInterfacesInterfaceConfigQemuSocketPort(0, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-hotplug-mode")());
  SetUpInterfacesInterfaceConfigQemuHotplugMode(
      QemuHotplugMode::HOTPLUG_MODE_NONE, node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-vm-mac-address")());
  SetUpInterfacesInterfaceConfigQemuVmMacAddress(node_id, port_id, mac_address, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-vm-netdev-id")());
  SetUpInterfacesInterfaceConfigQemuVmNetdevId("/", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-vm-chardev-id")());
  SetUpInterfacesInterfaceConfigQemuVmChardevId("/", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("qemu-vm-device-id")());
  SetUpInterfacesInterfaceConfigQemuVmDeviceId("/", node_id, port_id, node, tree);

  node = tree->AddNode(GetPath("interfaces")(
      "virtual-interface", name)("config")("native-socket-path")());
  SetUpInterfacesInterfaceConfigNativeSocket("/", node_id, port_id, node, tree);

  node = tree->AddNode(
      GetPath("interfaces")("virtual-interface", name)("config")("tdi-portin-id")());
  SetUpInterfacesInterfaceConfigTdiPortinId(node_id, port_id, node, tree);

  node = tree->AddNode(
      GetPath("interfaces")("virtual-interface", name)("config")("tdi-portout-id")());
  SetUpInterfacesInterfaceConfigTdiPortoutId(node_id, port_id, node, tree);

  absl::flat_hash_map<uint32, uint32> internal_priority_to_q_num;
  absl::flat_hash_map<uint32, TrafficClass> q_num_to_trafic_class;
  for (const auto& e : node_config.qos_config().cosq_mapping()) {
    internal_priority_to_q_num[e.internal_priority()] = e.q_num();
  }
  for (const auto& e : node_config.qos_config().traffic_class_mapping()) {
    uint32* q_num =
        gtl::FindOrNull(internal_priority_to_q_num, e.internal_priority());
    if (q_num != nullptr) {
      gtl::InsertIfNotPresent(
          &q_num_to_trafic_class, *q_num, e.traffic_class());
    }
  }

  for (const auto& e : q_num_to_trafic_class) {
    // TODO(unknown): Use consistent names for queue numbers. Either q_num
    // or q_id or queue_id.
    uint32 queue_id = e.first;
    std::string queue_name = TrafficClass_Name(e.second);

    // Add output-qos-related leafs.
    node = tree->AddNode(GetPath("qos")("interfaces")("virtual-interface", name)(
        "output")("queues")("queue", queue_name)("state")("name")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateName(queue_name, node);

    node = tree->AddNode(GetPath("qos")("interfaces")("virtual-interface", name)(
        "output")("queues")("queue", queue_name)("state")("id")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateId(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("virtual-interface", name)(
        "output")("queues")("queue", queue_name)("state")("transmit-pkts")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitPkts(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("virtual-interface", name)(
        "output")("queues")("queue", queue_name)("state")("transmit-octets")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateTransmitOctets(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(GetPath("qos")("interfaces")("virtual-interface", name)(
        "output")("queues")("queue", queue_name)("state")("dropped-pkts")());
    SetUpQosInterfacesInterfaceOutputQueuesQueueStateDroppedPkts(
        node_id, port_id, queue_id, node, tree);

    node = tree->AddNode(
        GetPath("qos")("queues")("queue", queue_name)("config")("id")());
    SetUpQosQueuesQueueConfigId(queue_id, node, tree);

    node = tree->AddNode(
        GetPath("qos")("queues")("queue", queue_name)("state")("id")());
    SetUpQosQueuesQueueStateId(queue_id, node, tree);
  }

  return node;
}

}  // namespace hal
}  // namespace stratum
