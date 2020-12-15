// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2019 Broadcom. All rights reserved. The term "Broadcom" refers to
// Broadcom Inc. and/or its subsidiaries.
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_SDKLT_BCM_SDK_WRAPPER_H_
#define STRATUM_HAL_LIB_BCM_SDKLT_BCM_SDK_WRAPPER_H_

#include <pthread.h>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm_diag_shell.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/common/constants.h"

namespace stratum {
namespace hal {
namespace bcm {

// This struct encapsulates all the data required to handle a SOC device
// associated with a unit.
struct BcmSocDevice {
  // Internal BDE device number for a unit.
  int dev_num;
  // SDK checkpoint file descriptor.
  int sdk_checkpoint_fd;
  BcmSocDevice() : dev_num(-1), sdk_checkpoint_fd(-1) {}
  ~BcmSocDevice() {
    if (sdk_checkpoint_fd != -1) close(sdk_checkpoint_fd);
  }
};

// This struct wraps a linkscan event Writer and a priority. The priority is
// used to priotize invocation of the Writers whenever a linkscan event is
// received.
struct BcmLinkscanEventWriter {
  std::unique_ptr<ChannelWriter<BcmSdkInterface::LinkscanEvent>> writer;
  int priority;  // The priority of the Writer.
  int id;        // Unique ID of the Writer.
};

// The BcmLinkscanEventWriter comparator used for sorting the container
// holding the BcmLinkscanEventWriter instances.
struct BcmLinkscanEventWriterComp {
  bool operator()(const BcmLinkscanEventWriter& a,
                  const BcmLinkscanEventWriter& b) const {
    return a.priority > b.priority;
  }
};

// The "BcmSdkWrapper" is an implementation of BcmSdkInterface which is used
// on real hardware to talk to BCM ASIC.
class BcmSdkWrapper : public BcmSdkInterface {
 public:
  // ACL UDF chunk size in bytes.
  static constexpr int kUdfChunkSize = 2;
  // It is apparently not possible to mix colored and uncolored counters for a
  // single ACL. Additionally, it appears that at most 4 counters can be
  // specified and they must be in pairs of byte and packet counters.
  // Number of colored stat counters used.
  static constexpr int kColoredStatCount = 4;
  // Number of uncolored stat counters used.
  static constexpr int kUncoloredStatCount = 2;
  // Maximum number of stat counters used.
  static constexpr int kMaxStatCount = 4;
  // Index of first red counter (bytes) in colored stat entry array.
  static constexpr int kRedCounterIndex = 2;
  // Index of first green counter (bytes) in colored stat entry array.
  static constexpr int kGreenCounterIndex = 0;
  // Index of first total counter (bytes) in uncolored stat entry array.
  static constexpr int kTotalCounterIndex = 0;

  ~BcmSdkWrapper() override;

  // BcmSdkInterface public methods.
  ::util::Status InitializeSdk(
      const std::string& config_file_path,
      const std::string& config_flush_file_path,
      const std::string& bcm_shell_log_file_path) override;
  ::util::StatusOr<std::string> GenerateBcmConfigFile(
      const BcmChassisMap& base_bcm_chassis_map,
      const BcmChassisMap& target_bcm_chassis_map, OperationMode mode) override;
  ::util::Status FindUnit(int unit, int pci_bus, int pci_slot,
                          BcmChip::BcmChipType chip_type) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status InitializeUnit(int unit, bool warm_boot) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ShutdownUnit(int unit) override
      EXCLUSIVE_LOCKS_REQUIRED(data_lock_);
  ::util::Status ShutdownAllUnits() override LOCKS_EXCLUDED(data_lock_);
  ::util::Status SetModuleId(int unit, int module) override;
  ::util::Status InitializePort(int unit, int port) override;
  ::util::Status SetPortOptions(int unit, int port,
                                const BcmPortOptions& options) override;
  ::util::Status GetPortOptions(int unit, int port,
                                BcmPortOptions* options) override;
  ::util::Status GetPortCounters(int unit, int port, PortCounters* pc) override;
  ::util::Status StartDiagShellServer() override;
  ::util::Status StartLinkscan(int unit) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status StopLinkscan(int unit) override LOCKS_EXCLUDED(data_lock_);
  void OnLinkscanEvent(int unit, int port, PortState linkstatus) override;
  ::util::StatusOr<int> RegisterLinkscanEventWriter(
      std::unique_ptr<ChannelWriter<LinkscanEvent>> writer,
      int priority) override LOCKS_EXCLUDED(linkscan_writers_lock_);
  ::util::Status UnregisterLinkscanEventWriter(int id) override
      LOCKS_EXCLUDED(linkscan_writers_lock_);
  ::util::StatusOr<BcmPortOptions::LinkscanMode> GetPortLinkscanMode(
      int unit, int port) override;
  ::util::Status SetMtu(int unit, int mtu) override LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<int> FindOrCreateL3RouterIntf(int unit, uint64 router_mac,
                                                 int vlan) override;
  ::util::Status DeleteL3RouterIntf(int unit, int router_intf_id) override;
  ::util::StatusOr<int> FindOrCreateL3CpuEgressIntf(int unit) override;
  ::util::StatusOr<int> FindOrCreateL3PortEgressIntf(
      int unit, uint64 nexthop_mac, int port, int vlan,
      int router_intf_id) override;
  ::util::StatusOr<int> FindOrCreateL3TrunkEgressIntf(
      int unit, uint64 nexthop_mac, int trunk, int vlan,
      int router_intf_id) override;
  ::util::StatusOr<int> FindOrCreateL3DropIntf(int unit) override;
  ::util::Status ModifyL3CpuEgressIntf(int unit, int egress_intf_id) override;
  ::util::Status ModifyL3PortEgressIntf(int unit, int egress_intf_id,
                                        uint64 nexthop_mac, int port, int vlan,
                                        int router_intf_id) override;

  ::util::Status ModifyL3TrunkEgressIntf(int unit, int egress_intf_id,
                                         uint64 nexthop_mac, int trunk,
                                         int vlan, int router_intf_id) override;
  ::util::Status ModifyL3DropIntf(int unit, int egress_intf_id) override;
  ::util::Status DeleteL3EgressIntf(int unit, int egress_intf_id) override;
  ::util::StatusOr<int> FindRouterIntfFromEgressIntf(
      int unit, int egress_intf_id) override;
  ::util::StatusOr<int> FindOrCreateEcmpEgressIntf(
      int unit, const std::vector<int>& member_ids) override;
  ::util::Status ModifyEcmpEgressIntf(
      int unit, int egress_intf_id,
      const std::vector<int>& member_ids) override;
  ::util::Status DeleteEcmpEgressIntf(int unit, int egress_intf_id) override;
  ::util::Status AddL3RouteIpv4(int unit, int vrf, uint32 subnet, uint32 mask,
                                int class_id, int egress_intf_id,
                                bool is_intf_multipath) override;
  ::util::Status AddL3RouteIpv6(int unit, int vrf, const std::string& subnet,
                                const std::string& mask, int class_id,
                                int egress_intf_id,
                                bool is_intf_multipath) override;
  ::util::Status AddL3HostIpv4(int unit, int vrf, uint32 ipv4, int class_id,
                               int egress_intf_id) override;
  ::util::Status AddL3HostIpv6(int unit, int vrf, const std::string& ipv6,
                               int class_id, int egress_intf_id) override;
  ::util::Status ModifyL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                   uint32 mask, int class_id,
                                   int egress_intf_id,
                                   bool is_intf_multipath) override;
  ::util::Status ModifyL3RouteIpv6(int unit, int vrf, const std::string& subnet,
                                   const std::string& mask, int class_id,
                                   int egress_intf_id,
                                   bool is_intf_multipath) override;
  ::util::Status ModifyL3HostIpv4(int unit, int vrf, uint32 ipv4, int class_id,
                                  int egress_intf_id) override;
  ::util::Status ModifyL3HostIpv6(int unit, int vrf, const std::string& ipv6,
                                  int class_id, int egress_intf_id) override;
  ::util::Status DeleteL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                   uint32 mask) override;
  ::util::Status DeleteL3RouteIpv6(int unit, int vrf, const std::string& subnet,
                                   const std::string& mask) override;
  ::util::Status DeleteL3HostIpv4(int unit, int vrf, uint32 ipv4) override;
  ::util::Status DeleteL3HostIpv6(int unit, int vrf,
                                  const std::string& ipv6) override;
  ::util::StatusOr<int> AddMyStationEntry(int unit, int priority, int vlan,
                                          int vlan_mask, uint64 dst_mac,
                                          uint64 dst_mac_mask) override;
  ::util::Status DeleteMyStationEntry(int unit, int station_id) override;
  ::util::Status AddL2Entry(int unit, int _vlan, uint64 dst_mac,
                            int logical_port, int trunk_port,
                            int l2_mcast_group_id, int class_id,
                            bool copy_to_cpu, bool dst_drop) override;
  ::util::Status DeleteL2Entry(int unit, int vlan, uint64 dst_mac) override;
  ::util::Status AddL2MulticastEntry(int unit, int priority, int vlan,
                                     int vlan_mask, uint64 dst_mac,
                                     uint64 dst_mac_mask, bool copy_to_cpu,
                                     bool drop,
                                     uint8 l2_mcast_group_id) override;
  ::util::Status DeleteL2MulticastEntry(int unit, int vlan, int vlan_mask,
                                        uint64 dst_mac,
                                        uint64 dst_mac_mask) override;
  ::util::Status DeleteL2EntriesByVlan(int unit, int vlan) override;
  ::util::Status AddVlanIfNotFound(int unit, int vlan) override;
  ::util::Status DeleteVlanIfFound(int unit, int vlan) override;
  ::util::Status ConfigureVlanBlock(int unit, int vlan, bool block_broadcast,
                                    bool block_known_multicast,
                                    bool block_unknown_multicast,
                                    bool block_unknown_unicast) override;
  ::util::Status ConfigureL2Learning(int unit, int vlan,
                                     bool disable_l2_learning) override;
  ::util::Status SetL2AgeTimer(int unit, int l2_age_duration_sec) override;
  ::util::Status ConfigSerdesForPort(
      int unit, int port, uint64 speed_bps, int serdes_core, int serdes_lane,
      int serdes_num_lanes, const std::string& intf_type,
      const SerdesRegisterConfigs& serdes_register_configs,
      const SerdesAttrConfigs& serdes_attr_configs) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status CreateKnetIntf(int unit, int vlan, std::string* netif_name,
                                int* netif_id) override;
  ::util::Status DestroyKnetIntf(int unit, int netif_id) override;
  ::util::StatusOr<int> CreateKnetFilter(int unit, int netif_id,
                                         KnetFilterType type) override;
  ::util::Status DestroyKnetFilter(int unit, int filter_id) override;
  ::util::Status StartRx(int unit, const RxConfig& rx_config) override;
  ::util::Status StopRx(int unit) override;
  ::util::Status SetRateLimit(
      int unit, const RateLimitConfig& rate_limit_config) override;
  ::util::Status GetKnetHeaderForDirectTx(int unit, int port, int cos,
                                          uint64 smac, size_t packet_len,
                                          std::string* header) override;
  ::util::Status GetKnetHeaderForIngressPipelineTx(
      int unit, uint64 smac, size_t packet_len, std::string* header) override;
  size_t GetKnetHeaderSizeForRx(int unit) override;
  ::util::Status ParseKnetHeaderForRx(int unit, const std::string& header,
                                      int* ingress_logical_port,
                                      int* egress_logical_port,
                                      int* cos) override;
  ::util::Status InitAclHardware(int unit) override;
  ::util::Status SetAclControl(int unit,
                               const AclControl& acl_control) override;
  ::util::Status SetAclUdfChunks(int unit, const BcmUdfSet& udfs) override;
  ::util::StatusOr<int> CreateAclTable(int unit,
                                       const BcmAclTable& table) override;
  ::util::Status DestroyAclTable(int unit, int table_id) override;
  ::util::StatusOr<int> InsertAclFlow(int unit, const BcmFlowEntry& flow,
                                      bool add_stats,
                                      bool color_aware) override;
  ::util::Status ModifyAclFlow(int unit, int flow_id,
                               const BcmFlowEntry& flow) override;
  ::util::Status RemoveAclFlow(int unit, int flow_id) override;
  ::util::Status GetAclUdfChunks(int unit, BcmUdfSet* udfs) override;
  ::util::Status GetAclTable(int unit, int table_id,
                             BcmAclTable* table) override;
  ::util::Status GetAclFlow(int unit, int flow_id, BcmFlowEntry* flow) override;
  ::util::StatusOr<std::string> MatchAclFlow(int unit, int flow_id,
                                             const BcmFlowEntry& flow) override;
  ::util::Status GetAclTableFlowIds(int unit, int table_id,
                                    std::vector<int>* flow_ids) override;
  ::util::Status AddAclStats(int unit, int table_id, int flow_id,
                             bool color_aware) override;
  ::util::Status RemoveAclStats(int unit, int flow_id) override;
  ::util::Status GetAclStats(int unit, int flow_id,
                             BcmAclStats* stats) override;
  ::util::Status SetAclPolicer(int unit, int flow_id,
                               const BcmMeterConfig& meter) override;
  ::util::Status InsertPacketReplicationEntry(
      const BcmPacketReplicationEntry& entry) override;
  ::util::Status DeletePacketReplicationEntry(
      const BcmPacketReplicationEntry& entry) override;

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BcmSdkWrapper* CreateSingleton(BcmDiagShell* bcm_diag_shell)
      LOCKS_EXCLUDED(init_lock_);

  // The following public functions are specific to this class. They are to be
  // called by SDK callbacks only.

  // Return the singleton instance to be used in the SDK callbacks.
  static BcmSdkWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // Return the FD for the SDK checkpoint file.
  ::util::StatusOr<int> GetSdkCheckpointFd(int unit) LOCKS_EXCLUDED(data_lock_);

  // Thread id for the currently running diag shell thread.
  pthread_t GetDiagShellThreadId() const;

  // BcmSdkWrapper is neither copyable nor movable.
  BcmSdkWrapper(const BcmSdkWrapper&) = delete;
  BcmSdkWrapper& operator=(const BcmSdkWrapper&) = delete;

 protected:
  // Protected constructor. Will be called by the children of this class, i.e.
  // BcmSdkSim.
  explicit BcmSdkWrapper(BcmDiagShell* bcm_diag_shell);

  // Cleanup existing KNET filters and KNET intfs for a given unit. Can be
  // overloaded by children which do no support KNET.
  virtual ::util::Status CleanupKnet(int unit);

  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static BcmSdkWrapper* singleton_ GUARDED_BY(init_lock_);

 private:
  // Timeout for Write() operations on linkscan events.
  static constexpr absl::Duration kWriteTimeout = absl::InfiniteDuration();

  // Helpers to deal with SDK checkpoint file.
  ::util::Status OpenSdkCheckpointFile(int unit) LOCKS_EXCLUDED(data_lock_);
  ::util::Status CreateSdkCheckpointFile(int unit) LOCKS_EXCLUDED(data_lock_);
  ::util::Status RegisterSdkCheckpointFile(int unit);
  ::util::StatusOr<std::string> FindSdkCheckpointFilePath(int unit)
      LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<int> FindSdkCheckpointFileSize(int unit)
      LOCKS_EXCLUDED(data_lock_);

  ::util::StatusOr<BcmChip::BcmChipType> GetChipType(int unit);

  // Helper function called in ConfigSerdesForPort() to setup intf, autoneg,
  // and FEC and configure Phy for a port.
  ::util::Status SetIntfAndConfigurePhyForPort(int unit, int port,
                                               BcmChip::BcmChipType chip_type,
                                               uint64 speed_bps,
                                               const std::string& intf_type);

  // Helper function called in ConfigSerdesForPort() to set serdes register
  // values for a port.
  ::util::Status SetSerdesRegisterForPort(int unit, int port,
                                          BcmChip::BcmChipType chip_type,
                                          int serdes_lane, uint32 reg,
                                          uint32 value);

  // Helper function called in ConfigSerdesForPort() to set serdes attributes
  // for a port.
  ::util::Status SetSerdesAttributeForPort(int unit, int port,
                                           BcmChip::BcmChipType chip_type,
                                           const std::string& attr,
                                           uint32 value);

  // Helper function called in InitializeSdk() to spawn a SDKLT shell.
  ::util::Status InitCLI();

  // Helper function to create a Knet filter for software multicast.
  // Required because CreateKnetFilter does not allow setting a FP match filter.
  ::util::StatusOr<int> CreateKnetFilterForMulticast(int unit, uint8 acl_rule);

  // Helper function the get the front panel port from logical port.
  // This should work because PC_PHYS_PORT is a R/O table.
  ::util::StatusOr<int> GetPanelPort(int unit, int port);

  // Helper to check if a unit exists.
  int CheckIfUnitExists(int unit);

  // Helper to check if a port exists.
  int CheckIfPortExists(int unit, int port) LOCKS_EXCLUDED(data_lock_);

  // RW mutex lock for protecting the internal maps.
  mutable absl::Mutex data_lock_;

  // Map from unit number to the current MTU used for all the interfaces of
  // the unit.
  absl::flat_hash_map<int, int> unit_to_mtu_ GUARDED_BY(data_lock_);

  // Map from unit to chip type specified.
  absl::flat_hash_map<int, BcmChip::BcmChipType> unit_to_chip_type_
      GUARDED_BY(data_lock_);

  // Map from each unit to the BcmSocDevice data struct associated with that
  // unit.
  absl::flat_hash_map<int, BcmSocDevice*> unit_to_soc_device_
      GUARDED_BY(data_lock_);

  // Map from index to usage flag
  typedef std::map<int, bool> InUseMap;

  // Map from pair of Acl stage, correspoding logical table id, and
  // software maintained table id
  typedef std::map<std::pair<BcmAclStage, int>, int> AclIds;

  typedef AclIds AclGroupIds;
  typedef AclIds AclRuleIds;
  typedef AclIds AclPolicyIds;
  typedef AclIds AclMeterIds;

  // Map from unit number to logical ports and associated port macro id,
  // physical device port number
  absl::flat_hash_map<int, std::map<int, std::pair<int, int>>>
      unit_to_logical_ports_ GUARDED_BY(data_lock_);

  // This struct encapsulates all the data required to handle mystation
  // entries associated with a unit.
  struct MyStationEntry {
    int vlan;
    int vlan_mask;
    uint64 dst_mac;
    uint64 dst_mac_mask;
    MyStationEntry()
        : vlan(0), vlan_mask(0), dst_mac(0), dst_mac_mask(0xffffffffffffULL) {}
    MyStationEntry(int _vlan, int _vlan_mask, uint64 _dst_mac,
                   uint64 _dst_mac_mask)
        : vlan(_vlan),
          vlan_mask(_vlan_mask),
          dst_mac(_dst_mac),
          dst_mac_mask(_dst_mac_mask) {}
    bool operator<(const MyStationEntry& other) const {
      return (
          vlan < other.vlan ||
          (vlan == other.vlan && (vlan_mask < other.vlan_mask ||
                                  (vlan_mask == other.vlan_mask &&
                                   (dst_mac < other.dst_mac ||
                                    (dst_mac == other.dst_mac &&
                                     (dst_mac_mask < other.dst_mac_mask)))))));
    }
    bool operator==(const MyStationEntry& other) const {
      return (vlan == other.vlan && vlan_mask == other.vlan_mask &&
              dst_mac == other.dst_mac && dst_mac_mask == other.dst_mac_mask);
    }
    std::string ToString() const {
      return absl::StrCat("(vlan:", vlan, ", vlan_mask:", absl::Hex(vlan_mask),
                          ", dst_mac:", absl::Hex(dst_mac),
                          ", dst_mac_mask:", absl::Hex(dst_mac_mask), ")");
    }
  };

  // Map from unit number to mystation maximum entries
  absl::flat_hash_map<int, int> unit_to_my_station_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to mystation minimum entries
  absl::flat_hash_map<int, int> unit_to_my_station_min_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to mystation entries
  absl::flat_hash_map<int, std::map<MyStationEntry, int>> my_station_ids_
      GUARDED_BY(data_lock_);

  // This struct encapsulates all the data required to handle l3 interfaces
  // associated with a unit.
  struct L3Interfaces {
    uint64 mac;
    int vlan;
    L3Interfaces() : mac(0), vlan(0) {}
    L3Interfaces(uint64 _mac, int _vlan) : mac(_mac), vlan(_vlan) {}
    bool operator<(const L3Interfaces& other) const {
      return (mac < other.mac || (mac == other.mac && vlan < other.vlan));
    }
    bool operator==(const L3Interfaces& other) const {
      return (vlan == other.vlan && mac == other.mac);
    }
    std::string ToString() const {
      return absl::StrCat("(vlan:", vlan, ", mac:", absl::Hex(mac), ")");
    }
    template <typename H>
    friend H AbslHashValue(H h, const L3Interfaces& i) {
      return H::combine(std::move(h), i.mac, i.vlan);
    }
  };

  // This struct encapsulates all the data required to handle
  // UDF chunks associated with a unit.
  struct UdfDataQualifier {
    BcmUdfSet::PacketLayer packet_layer;
    uint64 offset;
    int length;
    std::vector<int> idxs;
  };

  // Map from unit number to UDF chunks
  typedef std::map<int, UdfDataQualifier> ChunkIds;

  // Map from unit number to l3 interfaces maximum entries
  absl::flat_hash_map<int, int> unit_to_l3_intf_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to l3 interfaces minimum entries
  absl::flat_hash_map<int, int> unit_to_l3_intf_min_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to l3 interfaces
  absl::flat_hash_map<int, std::map<L3Interfaces, int>> l3_interface_ids_
      GUARDED_BY(data_lock_);

  // Map from unit number to l3 egress interfaces
  absl::flat_hash_map<int, InUseMap> l3_egress_interface_ids_
      GUARDED_BY(data_lock_);

  // Map from unit number to ecmp interfaces
  absl::flat_hash_map<int, InUseMap> l3_ecmp_egress_interface_ids_
      GUARDED_BY(data_lock_);

  // Map from unit number to max ACL Groups supported
  absl::flat_hash_map<int, int> unit_to_fp_groups_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of IFP group
  absl::flat_hash_map<int, InUseMap> ifp_group_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of EFP group
  absl::flat_hash_map<int, InUseMap> efp_group_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of VFP group
  absl::flat_hash_map<int, InUseMap> vfp_group_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to ACL groups
  absl::flat_hash_map<int, AclGroupIds*> fp_group_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to maximum ACL Rules supported
  absl::flat_hash_map<int, int> unit_to_fp_rules_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of IFP rules
  absl::flat_hash_map<int, InUseMap> ifp_rule_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of EFP rules
  absl::flat_hash_map<int, InUseMap> efp_rule_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of VFP rules
  absl::flat_hash_map<int, InUseMap> vfp_rule_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to ACL rules
  absl::flat_hash_map<int, AclRuleIds*> fp_rule_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to maximum ACL Policies supported
  absl::flat_hash_map<int, int> unit_to_fp_policy_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of IFP policies
  absl::flat_hash_map<int, InUseMap> ifp_policy_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of EFP policies
  absl::flat_hash_map<int, InUseMap> efp_policy_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of VFP policies
  absl::flat_hash_map<int, InUseMap> vfp_policy_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to ACL policies
  absl::flat_hash_map<int, AclPolicyIds*> fp_policy_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to maximum ACL meters supported
  absl::flat_hash_map<int, int> unit_to_fp_meter_max_limit_
      GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of IFP meters
  absl::flat_hash_map<int, InUseMap> ifp_meter_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of EFP meters
  absl::flat_hash_map<int, InUseMap> efp_meter_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to ACL meters
  absl::flat_hash_map<int, AclMeterIds*> fp_meter_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to maximum ACLs supported
  absl::flat_hash_map<int, int> unit_to_fp_max_limit_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of IFP ACLs
  absl::flat_hash_map<int, InUseMap> ifp_acl_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of EFP ACLs
  absl::flat_hash_map<int, InUseMap> efp_acl_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to logical table indexes of VFP ACLs
  absl::flat_hash_map<int, InUseMap> vfp_acl_ids_ GUARDED_BY(data_lock_);

  // Map from unit number to ACLs
  absl::flat_hash_map<int, AclIds*> fp_acl_ids_ GUARDED_BY(data_lock_);

  // maximum number of UDFs
  static constexpr int kUdfMaxChunks = 16;

  // Map from unit number to logical table indexes of UDF
  absl::flat_hash_map<int, InUseMap> unit_to_udf_chunk_ids_
      GUARDED_BY(data_lock_);

  // Map from unit number to UDF chunks
  absl::flat_hash_map<int, ChunkIds*> unit_to_chunk_ids_ GUARDED_BY(data_lock_);

  // Pointer to BcmDiagShell singleton instance. Not owned by this class.
  BcmDiagShell* bcm_diag_shell_;

  // RW mutex lock for protecting the linkscan event Writers.
  mutable absl::Mutex linkscan_writers_lock_;

  // Writers to forward the linkscan events to. They are registered by
  // external manager classes to receive the SDK linkscan events. The managers
  // can be running in different threads. The is sorted based on the
  // the priority of the BcmLinkscanEventWriter instances.
  std::multiset<BcmLinkscanEventWriter, BcmLinkscanEventWriterComp>
      linkscan_event_writers_ GUARDED_BY(linkscan_writers_lock_);
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_SDKLT_BCM_SDK_WRAPPER_H_
