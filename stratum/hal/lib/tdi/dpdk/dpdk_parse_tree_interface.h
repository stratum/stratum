// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_DPDK_DPDK_PARSE_TREE_INTERFACE_H_
#define STRATUM_HAL_LIB_TDI_DPDK_DPDK_PARSE_TREE_INTERFACE_H_

#include <string>

#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {
class TreeNode;
}
}  // namespace stratum
namespace stratum {
namespace hal {
class YangParseTree;
}
}  // namespace stratum

namespace stratum {
namespace hal {
namespace yang {
namespace interface {

void SetUpInterfacesInterfaceConfigHost(const char* host_val, uint64 node_id,
                                        uint64 port_id, TreeNode* node,
                                        YangParseTree* tree);

void SetUpInterfacesInterfaceConfigPortType(uint64 type, uint64 node_id,
                                            uint64 port_id, TreeNode* node,
                                            YangParseTree* tree);

void SetUpInterfacesInterfaceConfigDeviceType(uint64 type, uint64 node_id,
                                              uint64 port_id, TreeNode* node,
                                              YangParseTree* tree);

void SetUpInterfacesInterfaceConfigPipelineName(const char* pipeline_name,
                                                uint64 node_id, uint64 port_id,
                                                TreeNode* node,
                                                YangParseTree* tree);

void SetUpInterfacesInterfaceConfigMempoolName(const char* mempool_name,
                                               uint64 node_id, uint64 port_id,
                                               TreeNode* node,
                                               YangParseTree* tree);

void SetUpInterfacesInterfaceConfigPacketDir(uint64 packet_dir, uint64 node_id,
                                             uint64 port_id, TreeNode* node,
                                             YangParseTree* tree);

void SetUpInterfacesInterfaceConfigControlPort(const char* control_port,
                                               uint64 node_id, uint64 port_id,
                                               TreeNode* node,
                                               YangParseTree* tree);

void SetUpInterfacesInterfaceConfigPciBdf(const char* pci_bdf, uint64 node_id,
                                          uint64 port_id, TreeNode* node,
                                          YangParseTree* tree);

void SetUpInterfacesInterfaceConfigMtuValue(uint64 mtu, uint64 node_id,
                                            uint64 port_id, TreeNode* node,
                                            YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQueues(uint64 queues_count, uint64 node_id,
                                          uint64 port_id, TreeNode* node,
                                          YangParseTree* tree);

void SetUpInterfacesInterfaceConfigSocket(const char* default_path,
                                          uint64 node_id, uint64 port_id,
                                          TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuSocketIp(const char* default_socket_ip,
                                                uint64 node_id, uint64 port_id,
                                                TreeNode* node,
                                                YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuSocketPort(uint64 default_socket_port,
                                                  uint64 node_id,
                                                  uint64 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuHotplugMode(uint64 status,
                                                   uint64 node_id,
                                                   uint64 port_id,
                                                   TreeNode* node,
                                                   YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuVmMacAddress(uint64 node_id,
                                                    uint32 port_id,
                                                    uint64 mac_address,
                                                    TreeNode* node,
                                                    YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuVmNetdevId(const char* default_netdev_id,
                                                  uint64 node_id,
                                                  uint64 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuVmChardevId(
    const char* default_chardev_id, uint64 node_id, uint64 port_id,
    TreeNode* node, YangParseTree* tree);

void SetUpInterfacesInterfaceConfigQemuVmDeviceId(const char* default_device_id,
                                                  uint64 node_id,
                                                  uint64 port_id,
                                                  TreeNode* node,
                                                  YangParseTree* tree);

void SetUpInterfacesInterfaceConfigNativeSocket(const char* default_native_path,
                                                uint64 node_id, uint64 port_id,
                                                TreeNode* node,
                                                YangParseTree* tree);

void SetUpInterfacesInterfaceConfigTdiPortinId(uint32 node_id, uint32 port_id,
                                               TreeNode* node,
                                               YangParseTree* tree);

void SetUpInterfacesInterfaceConfigTdiPortoutId(uint32 node_id, uint32 port_id,
                                                TreeNode* node,
                                                YangParseTree* tree);

}  // namespace interface
}  // namespace yang
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_DPDK_DPDK_PARSE_TREE_INTERFACE_H_
