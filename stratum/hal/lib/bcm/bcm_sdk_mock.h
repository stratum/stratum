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


#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmSdkMock : public BcmSdkInterface {
 public:
  MOCK_METHOD3(InitializeSdk,
               ::util::Status(const std::string& config_file_path,
                              const std::string& config_flush_file_path,
                              const std::string& bcm_shell_log_file_path));
  MOCK_METHOD4(FindUnit, ::util::Status(int unit, int pci_bus, int pci_slot,
                                        BcmChip::BcmChipType chip_type));
  MOCK_METHOD2(InitializeUnit, ::util::Status(int unit, bool warm_boot));
  MOCK_METHOD1(ShutdownUnit, ::util::Status(int unit));
  MOCK_METHOD0(ShutdownAllUnits, ::util::Status());
  MOCK_METHOD2(SetModuleId, ::util::Status(int unit, int module));
  MOCK_METHOD2(InitializePort, ::util::Status(int unit, int port));
  MOCK_METHOD3(SetPortOptions, ::util::Status(int unit, int port,
                                              const BcmPortOptions& options));
  MOCK_METHOD3(GetPortOptions,
               ::util::Status(int unit, int port, BcmPortOptions* options));
  MOCK_METHOD0(StartDiagShellServer, ::util::Status());
  MOCK_METHOD1(StartLinkscan, ::util::Status(int unit));
  MOCK_METHOD1(StopLinkscan, ::util::Status(int unit));
  MOCK_METHOD2(
      RegisterLinkscanEventWriter,
      ::util::StatusOr<int>(std::unique_ptr<ChannelWriter<LinkscanEvent>>,
                            int priority));
  MOCK_METHOD1(UnregisterLinkscanEventWriter, ::util::Status(int id));
  MOCK_METHOD2(GetPortLinkscanMode,
               ::util::StatusOr<BcmPortOptions::LinkscanMode>(int unit,
                                                              int port));
  MOCK_METHOD2(SetMtu, ::util::Status(int unit, int mtu));
  MOCK_METHOD3(FindOrCreateL3RouterIntf,
               ::util::StatusOr<int>(int unit, uint64 router_mac, int vlan));
  MOCK_METHOD2(DeleteL3RouterIntf,
               ::util::Status(int unit, int router_intf_id));
  MOCK_METHOD1(FindOrCreateL3CpuEgressIntf, ::util::StatusOr<int>(int unit));
  MOCK_METHOD5(FindOrCreateL3PortEgressIntf,
               ::util::StatusOr<int>(int unit, uint64 nexthop_mac, int port,
                                     int vlan, int router_intf_id));
  MOCK_METHOD5(FindOrCreateL3TrunkEgressIntf,
               ::util::StatusOr<int>(int unit, uint64 nexthop_mac, int trunk,
                                     int vlan, int router_intf_id));
  MOCK_METHOD1(FindOrCreateL3DropIntf, ::util::StatusOr<int>(int unit));
  MOCK_METHOD2(ModifyL3CpuEgressIntf,
               ::util::Status(int unit, int egress_intf_id));
  MOCK_METHOD6(ModifyL3PortEgressIntf,
               ::util::Status(int unit, int egress_intf_id, uint64 nexthop_mac,
                              int port, int vlan, int router_intf_id));
  MOCK_METHOD6(ModifyL3TrunkEgressIntf,
               ::util::Status(int unit, int egress_intf_id, uint64 nexthop_mac,
                              int trunk, int vlan, int router_intf_id));
  MOCK_METHOD2(ModifyL3DropIntf, ::util::Status(int unit, int egress_intf_id));
  MOCK_METHOD2(DeleteL3EgressIntf,
               ::util::Status(int unit, int egress_intf_id));
  MOCK_METHOD2(FindRouterIntfFromEgressIntf,
               ::util::StatusOr<int>(int unit, int egress_intf_id));
  MOCK_METHOD2(FindOrCreateEcmpEgressIntf,
               ::util::StatusOr<int>(int unit,
                                     const std::vector<int>& member_ids));
  MOCK_METHOD3(ModifyEcmpEgressIntf,
               ::util::Status(int unit, int egress_intf_id,
                              const std::vector<int>& member_ids));
  MOCK_METHOD2(DeleteEcmpEgressIntf,
               ::util::Status(int unit, int egress_intf_id));
  MOCK_METHOD7(AddL3RouteIpv4,
               ::util::Status(int unit, int vrf, uint32 subnet, uint32 mask,
                              int class_id, int egress_intf_id,
                              bool is_intf_multipath));
  MOCK_METHOD7(AddL3RouteIpv6,
               ::util::Status(int unit, int vrf, const std::string& subnet,
                              const std::string& mask, int class_id,
                              int egress_intf_id, bool is_intf_multipath));
  MOCK_METHOD5(AddL3HostIpv4, ::util::Status(int unit, int vrf, uint32 ipv4,
                                             int class_id, int egress_intf_id));
  MOCK_METHOD5(AddL3HostIpv6,
               ::util::Status(int unit, int vrf, const std::string& ipv6,
                              int class_id, int egress_intf_id));
  MOCK_METHOD7(ModifyL3RouteIpv4,
               ::util::Status(int unit, int vrf, uint32 subnet, uint32 mask,
                              int class_id, int egress_intf_id,
                              bool is_intf_multipath));
  MOCK_METHOD7(ModifyL3RouteIpv6,
               ::util::Status(int unit, int vrf, const std::string& subnet,
                              const std::string& mask, int class_id,
                              int egress_intf_id, bool is_intf_multipath));
  MOCK_METHOD5(ModifyL3HostIpv4,
               ::util::Status(int unit, int vrf, uint32 ipv4, int class_id,
                              int egress_intf_id));
  MOCK_METHOD5(ModifyL3HostIpv6,
               ::util::Status(int unit, int vrf, const std::string& ipv6,
                              int class_id, int egress_intf_id));
  MOCK_METHOD4(DeleteL3RouteIpv4,
               ::util::Status(int unit, int vrf, uint32 subnet, uint32 mask));
  MOCK_METHOD4(DeleteL3RouteIpv6,
               ::util::Status(int unit, int vrf, const std::string& subnet,
                              const std::string& mask));
  MOCK_METHOD3(DeleteL3HostIpv4,
               ::util::Status(int unit, int vrf, uint32 ipv4));
  MOCK_METHOD3(DeleteL3HostIpv6,
               ::util::Status(int unit, int vrf, const std::string& ipv6));
  MOCK_METHOD6(AddMyStationEntry,
               ::util::StatusOr<int>(int unit, int priority, int vlan,
                                     int vlan_mask, uint64 dst_mac,
                                     uint64 dst_mac_mask));
  MOCK_METHOD2(DeleteMyStationEntry, ::util::Status(int unit, int station_id));
  MOCK_METHOD2(DeleteL2EntriesByVlan, ::util::Status(int unit, int vlan));
  MOCK_METHOD2(AddVlanIfNotFound, ::util::Status(int unit, int vlan));
  MOCK_METHOD2(DeleteVlanIfFound, ::util::Status(int unit, int vlan));
  MOCK_METHOD6(ConfigureVlanBlock,
               ::util::Status(int unit, int vlan, bool block_broadcast,
                              bool block_known_multicast,
                              bool block_unknown_multicast,
                              bool block_unknown_unicast));
  MOCK_METHOD3(ConfigureL2Learning,
               ::util::Status(int unit, int vlan, bool disable_l2_learning));
  MOCK_METHOD2(SetL2AgeTimer,
               ::util::Status(int unit, int l2_age_duration_sec));
  MOCK_METHOD9(
      ConfigSerdesForPort,
      ::util::Status(int unit, int port, uint64 speed_bps, int serdes_core,
                     int serdes_lane, int serdes_num_lanes,
                     const std::string& intf_type,
                     const SerdesRegisterConfigs& serdes_register_configs,
                     const SerdesAttrConfigs& serdes_attr_configs));
  MOCK_METHOD4(CreateKnetIntf,
               ::util::Status(int unit, int vlan, std::string* netif_name,
                              int* netif_id));
  MOCK_METHOD2(DestroyKnetIntf, ::util::Status(int unit, int netif_id));
  MOCK_METHOD3(CreateKnetFilter, ::util::StatusOr<int>(int unit, int netif_id,
                                                       KnetFilterType type));
  MOCK_METHOD2(DestroyKnetFilter, ::util::Status(int unit, int filter_id));
  MOCK_METHOD2(StartRx, ::util::Status(int unit, const RxConfig& rx_config));
  MOCK_METHOD1(StopRx, ::util::Status(int unit));
  MOCK_METHOD2(SetRateLimit,
               ::util::Status(int unit,
                              const RateLimitConfig& rate_limit_config));
  MOCK_METHOD5(GetKnetHeaderForDirectTx,
               ::util::Status(int unit, int port, int cos, uint64 smac,
                              std::string* header));
  MOCK_METHOD3(GetKnetHeaderForIngressPipelineTx,
               ::util::Status(int unit, uint64 smac, std::string* header));
  MOCK_METHOD1(GetKnetHeaderSizeForRx, size_t(int unit));
  MOCK_METHOD5(ParseKnetHeaderForRx,
               ::util::Status(int unit, const std::string& header,
                              int* ingress_logical_port,
                              int* egress_logical_port, int* cos));
  MOCK_METHOD1(InitAclHardware, ::util::Status(int unit));
  MOCK_METHOD2(SetAclControl,
               ::util::Status(int unit, const AclControl& acl_control));
  MOCK_METHOD2(SetAclUdfChunks,
               ::util::Status(int unit, const BcmUdfSet& udfs));
  MOCK_METHOD2(CreateAclTable,
               ::util::StatusOr<int>(int unit, const BcmAclTable& table));
  MOCK_METHOD2(DestroyAclTable, ::util::Status(int unit, int table_id));
  MOCK_METHOD4(InsertAclFlow,
               ::util::StatusOr<int>(int unit, const BcmFlowEntry& flow,
                                     bool add_stats, bool color_aware));
  MOCK_METHOD3(ModifyAclFlow,
               ::util::Status(int unit, int flow_id, const BcmFlowEntry& flow));
  MOCK_METHOD2(RemoveAclFlow, ::util::Status(int unit, int flow_id));
  MOCK_METHOD2(GetAclUdfChunks, ::util::Status(int unit, BcmUdfSet* udfs));
  MOCK_METHOD3(GetAclTable,
               ::util::Status(int unit, int table_id, BcmAclTable* table));
  MOCK_METHOD3(GetAclFlow,
               ::util::Status(int unit, int flow_id, BcmFlowEntry* flow));
  MOCK_METHOD3(MatchAclFlow,
               ::util::StatusOr<std::string>(int unit, int flow_id,
                                             const BcmFlowEntry& flow));
  MOCK_METHOD3(GetAclTableFlowIds, ::util::Status(int unit, int flow_id,
                                                  std::vector<int>* flow_ids));
  MOCK_METHOD4(AddAclStats, ::util::Status(int unit, int table_id, int flow_id,
                                           bool color_aware));
  MOCK_METHOD2(RemoveAclStats, ::util::Status(int unit, int flow_id));
  MOCK_METHOD3(GetAclStats,
               ::util::Status(int unit, int flow_id, BcmAclStats* stats));
  MOCK_METHOD3(SetAclPolicer, ::util::Status(int unit, int flow_id,
                                             const BcmMeterConfig& meter));
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_MOCK_H_
