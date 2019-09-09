/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_WRAPPER_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_WRAPPER_H_

#include <pthread.h>

#include <functional>
#include <string>
#include <memory>
#include <vector>
#include <set>

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
  ::util::Status StartDiagShellServer() override;
  ::util::Status StartLinkscan(int unit) override;
  ::util::Status StopLinkscan(int unit) override;
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
                                          uint64 smac,
                                          std::string* header) override;
  ::util::Status GetKnetHeaderForIngressPipelineTx(
      int unit, uint64 smac, std::string* header) override;
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
  ::util::Status ModifyAclFlow(
      int unit, int flow_id, const BcmFlowEntry& flow) override;
  ::util::Status RemoveAclFlow(int unit, int flow_id) override;
  ::util::Status GetAclUdfChunks(int unit, BcmUdfSet* udfs) override;
  ::util::Status GetAclTable(int unit, int table_id,
                             BcmAclTable* table) override;
  ::util::Status GetAclFlow(int unit, int flow_id, BcmFlowEntry* flow) override;
  ::util::StatusOr<std::string> MatchAclFlow(
      int unit, int flow_id, const BcmFlowEntry& flow) override;
  ::util::Status GetAclTableFlowIds(
      int unit, int table_id, std::vector<int>* flow_ids) override;
  ::util::Status AddAclStats(int unit, int table_id, int flow_id,
                             bool color_aware) override;
  ::util::Status RemoveAclStats(int unit, int flow_id) override;
  ::util::Status GetAclStats(int unit, int flow_id,
                             BcmAclStats* stats) override;
  ::util::Status SetAclPolicer(int unit, int flow_id,
                               const BcmMeterConfig& meter) override;

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

  // Pointer to BcmDiagShell singleton instance. Not owned by this class.
  BcmDiagShell* bcm_diag_shell_;

  // RW mutex lock for protecting the linkscan event Writers.
  mutable absl::Mutex linkscan_writers_lock_;

  // Writers to forward the linkscan events to. They are registered by
  // external manager classes to receive the SDK linkscan events. The managers
  // can be running in different threads. The is sorted based on the
  // the priority of the BcmLinkscanEventWriter intances.
  std::multiset<BcmLinkscanEventWriter, BcmLinkscanEventWriterComp>
      linkscan_event_writers_ GUARDED_BY(linkscan_writers_lock_);
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_WRAPPER_H_
