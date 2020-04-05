// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <set>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmSdkInterface" class in HAL implements a shim layer around the BCM
// SDK. it is defined as an abstract class to allow multiple implementations:
// 1- BcmSdkWrapper: The real implementation which includes all the BCM API
//    calls.
// 2- BcmSdkMock: Mock class used for unit testing.
class BcmSdkInterface {
 public:
  // Map from BCM serdes register IDs for ports to their values.
  typedef std::map<uint32, uint32> SerdesRegisterConfigs;

  // Map from BCM serdes attributes for ports to their values.
  typedef std::map<std::string, uint32> SerdesAttrConfigs;

  // The type of KNET filter to add. Given to CreateKnetFilter API.
  enum class KnetFilterType {
    // Catch all packets.
    CATCH_ALL,
    // Catch all non-flow packets hit by an FP rule.
    CATCH_NON_SFLOW_FP_MATCH,
    // Catch all SFLOW samples from egress port.
    CATCH_SFLOW_FROM_EGRESS_PORT,
    // Catch all SFLOW samples from ingress port.
    CATCH_SFLOW_FROM_INGRESS_PORT,
  };

  // DmaChannelConfig encapsulates all the data required to configure an RX
  // DMA channel. This is used as part of RxConfig given to StartRx() API.
  struct DmaChannelConfig {
    // The number of chains (DVs). Must be > 0.
    int chains;
    // Strip CRC from packets?
    bool strip_crc;
    // Strip VLAN tag from packets?
    bool strip_vlan;
    // Accept packets larger than bufsize?
    bool oversized_packets_ok;
    // Do not parse received packets?
    bool no_pkt_parsing;
    // The set of COSes supported for the channel. Cannot be empty. Also all
    // the cos values must be valid.
    std::set<int> cos_set;
    DmaChannelConfig()
        : chains(0),
          strip_crc(false),
          strip_vlan(false),
          oversized_packets_ok(false),
          cos_set() {}
  };

  // RxConfig encapsulates all the data required to fully configure RX on a
  // unit. Given to StartRx() API.
  struct RxConfig {
    // The RX pool size in packets. Must be > 0.
    int rx_pool_pkt_count;
    // Bytes per packet in RX pool. Must be > 0.
    int rx_pool_bytes_per_pkt;
    // Max packet size in bytes. Must be > 0.
    int max_pkt_size_bytes;
    // Packets per chain. Must be > 0.
    int pkts_per_chain;
    // Global rate limit in pps. It can change later by giving a new
    // RateLimitConfig to SetRateLimit. If not given (default 0), we set
    // no limit.
    int max_rate_pps;
    // Max # of pakcet received in single burst. It can change later by giving
    // a new RateLimitConfig to SetRateLimit. If not given (default 0),
    // we set no limit.
    int max_burst_pkts;
    // Are we using interrupts to generate RX callback?
    bool use_interrupt;
    // Map from DMA channel (1-based) to DMA channel config given by an instance
    // of DmaChannelConfig. Must not be empty.
    std::map<int, DmaChannelConfig> dma_channel_configs;
    RxConfig()
        : rx_pool_pkt_count(0),
          rx_pool_bytes_per_pkt(0),
          max_pkt_size_bytes(0),
          pkts_per_chain(0),
          max_rate_pps(0),
          max_burst_pkts(0),
          use_interrupt(false),
          dma_channel_configs() {}
  };

  // BcmPerCosRateLimitConfig specifies rate limit settings for a COS. This
  // is used in RateLimitConfig given to SetRateLimit() API.
  struct PerCosRateLimitConfig {
    // Rate limit for this cos in pps. If not given (default of 0), we set no
    // limit.
    int max_rate_pps;
    // Max # of pakcet received in single burst for this cos. If not given
    // (default of 0), we set no limit.
    int max_burst_pkts;
    PerCosRateLimitConfig() : max_rate_pps(0), max_burst_pkts(0) {}
  };

  // RateLimitConfig specifies rate limit settings for a unit. This is
  // given to SetRateLimit() API.
  struct RateLimitConfig {
    // Global rate limit in pps. If not given (default of 0), we set no limit.
    int max_rate_pps;
    // Max # of packet received in single burst. If not given (default of 0),
    // we set no limit.
    int max_burst_pkts;
    // Map from cos to its rate limit config.
    std::map<int, PerCosRateLimitConfig> per_cos_rate_limit_configs;
    RateLimitConfig()
        : max_rate_pps(0), max_burst_pkts(0), per_cos_rate_limit_configs() {}
  };

  // AclControl contains values for ACL hardware control flags.
  struct AclControl {
    // AclStageEnable contains flags indicating whether ACL stages are enabled
    // for packets ingressing through a particular port.
    struct AclStageEnable {
      bool vfp_enable;  // Enable ACL Lookup stage.
      bool ifp_enable;  // Enable ACL Ingress stage.
      bool efp_enable;  // Enable ACL Egress stage.
      bool apply;       // Apply the flags in this struct.
    };
    // BoolFlag contains a boolean flag and whether or not to apply the flag.
    struct BoolFlag {
      bool enable;  // Enable flag.
      bool apply;   // Apply setting.
    };
    // ACL stage enable flags for external ports.
    AclStageEnable extern_port_flags;
    // ACL stage enable flags for internal ports.
    AclStageEnable intern_port_flags;
    // ACL stage enable flags for cpu ports.
    AclStageEnable cpu_port_flags;
    // Whether intra-slice double-wide configuration is enabled.
    BoolFlag intra_double_wide_enable;
    // Whether read through is enabled for stats collection.
    BoolFlag stats_read_through_enable;
  };

  // LinkscanEvent encapsulates the information received on a linkscan event.
  struct LinkscanEvent {
    int unit;
    int port;
    PortState state;
  };

  // A few predefined priority values that can be used by external functions
  // when calling RegisterLinkscanEventWriter.
  static constexpr int kLinkscanEventWriterPriorityHigh = 100;
  static constexpr int kLinkscanEventWriterPriorityMed = 10;
  static constexpr int kLinkscanEventWriterPriorityLow = 1;

  virtual ~BcmSdkInterface() {}

  // Initializes the SDK.
  virtual ::util::Status InitializeSdk(
      const std::string& config_file_path,
      const std::string& config_flush_file_path,
      const std::string& bcm_shell_log_file_path) = 0;

  // Generates the configuration file (content) for the SDK.
  virtual ::util::StatusOr<std::string> GenerateBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map, OperationMode mode) = 0;

  // Finds the BCM SOC device given PCI bus/PCI slot, creates a soc_cm_dev_t
  // entry that is the main internal SDK data structure that identifies the
  // given device and ensures that the given unit number can be used as the
  // handle to the SOC device. In other words, it checks that the given unit
  // can be "assigned" to the SOC device. Additionally, we pass the chip type
  // expressed in the config to save for future reference and to validate the
  // type of the chip found (based on the device info) matches the type given
  // in the config.
  virtual ::util::Status FindUnit(int unit, int pci_bus, int pci_slot,
                                  BcmChip::BcmChipType chip_type) = 0;

  // Fully initializes the unit. Supports both warmboot and coldboot.
  virtual ::util::Status InitializeUnit(int unit, bool warm_boot) = 0;

  // Fully uninitializes the given unit.
  virtual ::util::Status ShutdownUnit(int unit) = 0;

  // Fully uninitializes all the initialized units.
  virtual ::util::Status ShutdownAllUnits() = 0;

  // Set unit's module ID.
  virtual ::util::Status SetModuleId(int unit, int module) = 0;

  // Initialize (aka reset) the port.
  virtual ::util::Status InitializePort(int unit, int port) = 0;

  // Sets port options for a given logical port.
  virtual ::util::Status SetPortOptions(int unit, int port,
                                        const BcmPortOptions& options) = 0;

  // Gets port options for a given logical port.
  virtual ::util::Status GetPortOptions(int unit, int port,
                                        BcmPortOptions* options) = 0;

  // Gets the counters for a given logical port.
  virtual ::util::Status GetPortCounters(int unit, int port,
                                         PortCounters* pc) = 0;

  // Starts the diag shell server for listening to client telnet connections.
  virtual ::util::Status StartDiagShellServer() = 0;

  // Starts linkscan. If the callback is registered already by calling
  // RegisterLinkscanEventWriter, this will start forwarding the linscan events
  // to the callback.
  virtual ::util::Status StartLinkscan(int unit) = 0;

  // Stops linkscan.
  virtual ::util::Status StopLinkscan(int unit) = 0;

  // Create link scan event message
  virtual void OnLinkscanEvent(int unit, int port, PortState linkstatus) = 0;

  // Registers a Writer through which to send any linkscan events. The message
  // contains a tuple (unit, port, state), where port refers to the Broadcom SDK
  // logical port. The priority determines the relative priority of the Writer
  // as compared to other registered Writers. When a linkscan event is received,
  // the Writers are invoked in order of highest priority. The returned value is
  // the ID of the Writer. It can be used to unregister the Writer later.
  virtual ::util::StatusOr<int> RegisterLinkscanEventWriter(
      std::unique_ptr<ChannelWriter<LinkscanEvent>> writer, int priority) = 0;

  // Unregisters a linkscan callback given its ID.
  virtual ::util::Status UnregisterLinkscanEventWriter(int id) = 0;

  // Gets port linkscan mode
  virtual ::util::StatusOr<BcmPortOptions::LinkscanMode> GetPortLinkscanMode(
      int unit, int port) = 0;

  // Sets the MTU for all the L3 intf of a given unit. The MTU value will be
  // saved and used for all the L3 intf created later on.
  virtual ::util::Status SetMtu(int unit, int mtu) = 0;

  // Finds an L3 router intf given its (vlan, router_mac) and if it does not
  // exist tries to create it. In either case, returns the L3 intf ID of the
  // router intf. Packets sent out through this intf will be encapsulated with
  // (vlan, router_mac) given to this method. If vlan == 0, default VLAN will
  // be used.
  virtual ::util::StatusOr<int> FindOrCreateL3RouterIntf(int unit,
                                                         uint64 router_mac,
                                                         int vlan) = 0;

  // Deletes an L3 router intf given its ID from a given unit.
  virtual ::util::Status DeleteL3RouterIntf(int unit, int router_intf_id) = 0;

  // Finds an L3 egress intf for sending packets unchanged to CPU port on a
  // given unit. If it does not exist, tries to create it. In either case,
  // returns the ID of the egress intf.
  virtual ::util::StatusOr<int> FindOrCreateL3CpuEgressIntf(int unit) = 0;

  // Finds an L3 port egress intf defining the nexthop, given its (nexthop_mac,
  // port, vlan, router_intf_id). If it does not exist, tries to create it. In
  // either case, returns the ID of the egress intf. Packets sent to the intf
  // will be sent through the given port. DA will be the given nexthop_mac, and
  // SA will be found using the given l3_intf_id, created previously using
  // FindOrCreateL3RouterIntf(). The given port can be for CPU as well, in which
  // case nexthop_mac and router_intf_id are not used. If vlan == 0, default
  // VLAN will be used.
  virtual ::util::StatusOr<int> FindOrCreateL3PortEgressIntf(
      int unit, stratum::uint64 nexthop_mac, int port, int vlan,
      int router_intf_id) = 0;

  // Finds an L3 trunk/lag egress intf defining the nexthop, given its
  // (nexthop_mac, trunk, vlan, router_intf_id). If it does not exist, tries to
  // create it. In either case, returns the ID of the egress intf. Packets sent
  // to the intf will be sent through the given trunk/LAG. DA will be the given
  // nexthop_mac, and SA will be found using the given l3_intf_id, created
  // previously using FindOrCreateL3RouterIntf(). If vlan == 0, default VLAN
  // will be used.
  virtual ::util::StatusOr<int> FindOrCreateL3TrunkEgressIntf(
      int unit, stratum::uint64 nexthop_mac, int trunk, int vlan,
      int router_intf_id) = 0;

  // Finds an L3 drop egress intf on a given unit. If it does not exist, tries
  // to create it. In either case, returns the ID of the egress intf.
  virtual ::util::StatusOr<int> FindOrCreateL3DropIntf(int unit) = 0;

  // Modifies an already existing L3 intf on a unit given its ID to become an
  // L3 intf for sending packets unchanged to CPU port.
  virtual ::util::Status ModifyL3CpuEgressIntf(int unit,
                                               int egress_intf_id) = 0;

  // Modifies an already existing L3 intf on a unit given its ID to become an
  // L3 intf pointing to a regular port given its (nexthop_mac, port, vlan,
  // router_intf_id).
  virtual ::util::Status ModifyL3PortEgressIntf(int unit, int egress_intf_id,
                                                stratum::uint64 nexthop_mac,
                                                int port, int vlan,
                                                int router_intf_id) = 0;

  // Modifies an already existing L3 intf on a unit given its ID to become an
  // L3 intf pointing to a trunk/LAG given its (nexthop_mac, trunk, vlan,
  // router_intf_id).
  virtual ::util::Status ModifyL3TrunkEgressIntf(int unit, int egress_intf_id,
                                                 stratum::uint64 nexthop_mac,
                                                 int trunk, int vlan,
                                                 int router_intf_id) = 0;

  // Modifies an already existing L3 intf on a unit given its ID to become an
  // L3 drop intf.
  virtual ::util::Status ModifyL3DropIntf(int unit, int egress_intf_id) = 0;

  // Deletes an L3 egress intf given its ID from a given unit.
  virtual ::util::Status DeleteL3EgressIntf(int unit, int egress_intf_id) = 0;

  // Returns the ID of the L3 router intf that a given egress intf points to.
  virtual ::util::StatusOr<int> FindRouterIntfFromEgressIntf(
      int unit, int egress_intf_id) = 0;

  // Finds an ECMP/WCMP egress intf pointing to a list of L3 egress intfs
  // given by the list of egress intf IDs in member_ids. If it cannot be found,
  // tries to create it. In either case, return the egress intf ID corresponding
  // to the group.
  virtual ::util::StatusOr<int> FindOrCreateEcmpEgressIntf(
      int unit, const std::vector<int>& member_ids) = 0;

  // Modifies the members of an existing ECMP/WCMP egress intf on a unit given
  // its ID. Return error if ECMP/WCMP egress intf does not exist.
  virtual ::util::Status ModifyEcmpEgressIntf(
      int unit, int egress_intf_id, const std::vector<int>& member_ids) = 0;

  // Deletes an L3 ECMP/WCMP egress intf given its ID from a given unit.
  virtual ::util::Status DeleteEcmpEgressIntf(int unit, int egress_intf_id) = 0;

  // Adds an IPv4 L3 LPM route for given IPv4 subnet/mask and VRF. If vrf == 0,
  // default VRF is used. If class_id == 0, no class ID will be set. The egress
  // intf used is given by egress_intf_id and is assumed to be already created.
  // The function will return error if a route with the same (vrf, subnet, mask)
  // exists. The boolean is_intf_multipath needs to be set to true if the given
  // egress_intf_id corresponds to an ECMP/WCMP egress intf.
  virtual ::util::Status AddL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                        uint32 mask, int class_id,
                                        int egress_intf_id,
                                        bool is_intf_multipath) = 0;

  // Adds an IPv6 L3 LPM route for given IPv6 subnet/mask and VRF. If vrf == 0,
  // default VRF is used. If class_id == 0, no class ID will be set. The egress
  // intf used is given by egress_intf_id and is assumed to be already created.
  // The function will return error if a route with the same (vrf, subnet, mask)
  // exists. The boolean is_intf_multipath needs to be set to true if the given
  // egress_intf_id corresponds to an ECMP/WCMP egress intf.
  virtual ::util::Status AddL3RouteIpv6(int unit, int vrf,
                                        const std::string& subnet,
                                        const std::string& mask, int class_id,
                                        int egress_intf_id,
                                        bool is_intf_multipath) = 0;

  // Adds an IPv4 L3 host route for given IPv4 address and vrf. If vrf == 0,
  // default VRF is used. If class_id == 0, no class ID will be set. The egress
  // intf used is given by egress_intf_id and is assumed to be already created.
  // The function will return error if a host with the same (vrf, ipv4) exists.
  virtual ::util::Status AddL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                       int class_id, int egress_intf_id) = 0;

  // Adds an IPv6 L3 host route for given IPv6 address and VRF. If vrf == 0,
  // default VRF is used. If class_id ==0, no class_id will be set. The egress
  // intf used is given by egress_intf_id and is assumed to be already created.
  // The function will return error if a host with the same (vrf, ipv6) exists.
  virtual ::util::Status AddL3HostIpv6(int unit, int vrf,
                                       const std::string& ipv6, int class_id,
                                       int egress_intf_id) = 0;

  // Modifies class_id and/or egress_intf_id of an existing IPv4 L3 LPM route
  // with key (vrf, subnet, mask). If vrf == 0, default VRF is used. If
  // class_id == 0, class ID will not be modified. The new egress intf to use is
  // given by egress_intf_id and is assumed to be already created. The function
  // will return error if a route with key (vrf, subnet, mask) does not exist.
  // The boolean is_intf_multipath needs to be set to true if the given
  // egress_intf_id corresponds to an ECMP/WCMP egress intf.
  virtual ::util::Status ModifyL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                           uint32 mask, int class_id,
                                           int egress_intf_id,
                                           bool is_intf_multipath) = 0;

  // Modifies class_id and/or egress_intf_id of an existing IPv6 L3 LPM route
  // with key (vrf, subnet, mask). If vrf == 0, default VRF is used. If
  // class_id == 0, class ID will not be modified. The new egress intf to use is
  // given by egress_intf_id and is assumed to be already created. The function
  // will return error if a route with key (vrf, subnet, mask) does not exist.
  // The boolean is_intf_multipath needs to be set to true if the given
  // egress_intf_id corresponds to an ECMP/WCMP egress intf.
  virtual ::util::Status ModifyL3RouteIpv6(int unit, int vrf,
                                           const std::string& subnet,
                                           const std::string& mask,
                                           int class_id, int egress_intf_id,
                                           bool is_intf_multipath) = 0;

  // Modifies class_id and/or egress_intf_id of an existing IPv4 L3 host route
  // with key (vrf, ipv4). If vrf == 0, default VRF is used. If class_id == 0,
  // class ID will not be modified. The new egress intf to use is given by
  // egress_intf_id and is assumed to be already created. The function will
  // return error if a host with key (vrf, ipv4) does not exist.
  virtual ::util::Status ModifyL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                          int class_id, int egress_intf_id) = 0;

  // Modifies class_id and/or egress_intf_id of an existing IPv6 L3 host route
  // with key (vrf, ipv6). If vrf == 0, default VRF is used. If class_id == 0,
  // class ID will not be modified. The new egress intf to use is given by
  // egress_intf_id and is assumed to be already created. The function will
  // return error if a host with key (vrf, ipv6) does not exist.
  virtual ::util::Status ModifyL3HostIpv6(int unit, int vrf,
                                          const std::string& ipv6, int class_id,
                                          int egress_intf_id) = 0;

  // Deletes an IPv4 L3 LPM route given its (vrf, subnet, mask) key. Returns
  // error if the route with the given key does not exist.
  virtual ::util::Status DeleteL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                           uint32 mask) = 0;

  // Deletes an IPv6 L3 LPM route given its (vrf, subnet, mask) key. Returns
  // error if the route with the given key does not exist.
  virtual ::util::Status DeleteL3RouteIpv6(int unit, int vrf,
                                           const std::string& subnet,
                                           const std::string& mask) = 0;

  // Deletes an IPv4 L3 host route given its (vrf, ipv4) key. Returns error if
  // the host with the given key does not exist.
  virtual ::util::Status DeleteL3HostIpv4(int unit, int vrf, uint32 ipv4) = 0;

  // Deletes an IPv6 L3 host route given its (vrf, ipv6) key. Returns error if
  // the host with the given key does not exist.
  virtual ::util::Status DeleteL3HostIpv6(int unit, int vrf,
                                          const std::string& ipv6) = 0;

  // Adds an entry to match the given (vlan, vlan_mask, dst_mac, dst_mac_mask)
  // to the my station TCAM, with the given priority. NOOP if the entry already
  // exists. All the IPv4/IPv6 packets, independent of the src port, will be
  // matched against the entries in my station TCAM and if they do not match
  // any entry, no L3 forwarding action will be taken.
  virtual ::util::StatusOr<int> AddMyStationEntry(int unit, int priority,
                                                  int vlan, int vlan_mask,
                                                  uint64 dst_mac,
                                                  uint64 dst_mac_mask) = 0;

  // Removes a previously added entry to my station TCAM using its ID. Will
  // return error if the entry does not exist.
  virtual ::util::Status DeleteMyStationEntry(int unit, int station_id) = 0;

  // Adds an entry to match the given (vlan, dst_mac) to the L2 FDB hash table.
  // Failure if the entry already exists.
  virtual ::util::Status AddL2Entry(int unit, int vlan, uint64 dst_mac,
                                    int logical_port, int trunk_port,
                                    int l2_mcast_group_id, int class_id,
                                    bool copy_to_cpu, bool dst_drop) = 0;

  // Delete a previously added entry from the L2 FDB. Will return error if
  // entry does not exist.
  virtual ::util::Status DeleteL2Entry(int unit, int vlan, uint64 dst_mac) = 0;

  // Adds an entry to match the given (vlan, vlan_mask, dst_mac, dst_mac_mask)
  // to the my station TCAM. Matched packets are punted to the CPU and cast to
  // all ports of the l2_mcast_group_id. Once native L2 multicast becomes
  // availabe in SDKLT, this can be changed.
  virtual ::util::Status AddL2MulticastEntry(int unit, int priority, int vlan,
                                             int vlan_mask, uint64 dst_mac,
                                             uint64 dst_mac_mask,
                                             bool copy_to_cpu, bool drop,
                                             uint8 l2_mcast_group_id) = 0;

  // Removes a previously added entry from my station TCAM using the given
  // (vlan, vlan_mask, dst_mac, dst_mac_mask). Will return error if the entry
  // does not exist.
  virtual ::util::Status DeleteL2MulticastEntry(int unit, int vlan,
                                                int vlan_mask, uint64 dst_mac,
                                                uint64 dst_mac_mask) = 0;

  // Creates a packet replication entry.
  // Only multicast groups are supported for now. Creating clone sessions is
  // not necessary yet, as all packets arriving at the CPU are forwared to the
  // controller.
  virtual ::util::Status InsertPacketReplicationEntry(
      const BcmPacketReplicationEntry& entry) = 0;

  // Deletes an previously created packet replication entry. Will return error
  // if the entry does not exist.
  virtual ::util::Status DeletePacketReplicationEntry(
      const BcmPacketReplicationEntry& entry) = 0;

  // Deletes all the L2 addresses learnt for a given VLAN on a given unit.
  virtual ::util::Status DeleteL2EntriesByVlan(int unit, int vlan) = 0;

  // Adds a VLAN with a given ID if it does not exist (NOOP if the VLAN
  // already exists). If a new VLAN is created  all the ports including CPU
  // will be added to the regular member ports and all the ports excluding CPU
  // will be added to untagged member ports. Untagged member ports are referring
  // to the ports where VLAN tags for all egress packets are stripped before
  // sending the packet out.
  virtual ::util::Status AddVlanIfNotFound(int unit, int vlan) = 0;

  // Delete a VLAN given its ID if it exists (NOOP if the VLAN is already
  // deleted).
  virtual ::util::Status DeleteVlanIfFound(int unit, int vlan) = 0;

  // Configures VLAN traffic blocking behavior.
  virtual ::util::Status ConfigureVlanBlock(int unit, int vlan,
                                            bool block_broadcast,
                                            bool block_known_multicast,
                                            bool block_unknown_multicast,
                                            bool block_unknown_unicast) = 0;

  // Enables/disabled L2 learning for a VLAN.
  virtual ::util::Status ConfigureL2Learning(int unit, int vlan,
                                             bool disable_l2_learning) = 0;

  // Sets L2 aging duration for L2 entries on a unit.
  virtual ::util::Status SetL2AgeTimer(int unit, int l2_age_duration_sec) = 0;

  // Configures serdes setting for a given BCM port.
  virtual ::util::Status ConfigSerdesForPort(
      int unit, int port, uint64 speed_bps, int serdes_core, int serdes_lane,
      int serdes_num_lanes, const std::string& intf_type,
      const SerdesRegisterConfigs& serdes_register_configs,
      const SerdesAttrConfigs& serdes_attr_configs) = 0;

  // Creates a KNET intf on a given 'unit'. The VLAN used when creating the intf
  // is given by 'vlan', with 0 poiting to default VLAN. 'netif_name' is given
  // as a pointer to a string which includes the template of the netif name. The
  // SDK then fills up the string with the correct name returned from kernel.
  // Finally the ID of the netif is returned by 'netif_id'.
  virtual ::util::Status CreateKnetIntf(int unit, int vlan,
                                        std::string* netif_name,
                                        int* netif_id) = 0;

  // Destorys an already created KNET intf on a 'unit' (given by 'netif_id').
  virtual ::util::Status DestroyKnetIntf(int unit, int netif_id) = 0;

  // Creates a KNET filter for an already created KNET intf on a 'unit' (given
  // by 'netif_id'). We only support a set of KNET filters in our application.
  // The types of these filters are all given by enum KnetFilterType defined
  // above. The id of the filter is then returned for the application to save
  // and refer to later. This is supposed to be called upon initialization only.
  virtual ::util::StatusOr<int> CreateKnetFilter(int unit, int netif_id,
                                                 KnetFilterType type) = 0;

  // Destorys an already created KNET filter on a 'unit' (given by 'filter_id').
  // This is supposed to be called upon shutdown.
  virtual ::util::Status DestroyKnetFilter(int unit, int filter_id) = 0;

  // Configures and starts RX on a unit. The RX config is given by 'rx_config'
  // input. This is supposed to be called upon initialization only.
  virtual ::util::Status StartRx(int unit, const RxConfig& rx_config) = 0;

  // Stops RX on a given unit. This is supposed to be called upon shutdown.
  virtual ::util::Status StopRx(int unit) = 0;

  // Sets up RX rate limits. This can be called at any point to change rate
  // limits.
  virtual ::util::Status SetRateLimit(
      int unit, const RateLimitConfig& rate_limit_config) = 0;

  // Gets the KNET header for a TX packet directed to a port. The filled
  // 'header' will have a fixed size.
  virtual ::util::Status GetKnetHeaderForDirectTx(int unit, int port, int cos,
                                                  uint64 smac,
                                                  size_t packet_len,
                                                  std::string* header) = 0;

  // Gets the KNET header for a TX packet destined to ingress pipeline. The
  // filled 'header' will have a fixed size.
  virtual ::util::Status GetKnetHeaderForIngressPipelineTx(
      int unit, uint64 smac, size_t packet_len, std::string* header) = 0;

  // Returns the fixed size KNET header size for packets received from a port.
  virtual size_t GetKnetHeaderSizeForRx(int unit) = 0;

  // Parses the fixed-size KNET header from a port and determines where and
  // how the packet was received.
  virtual ::util::Status ParseKnetHeaderForRx(int unit,
                                              const std::string& header,
                                              int* ingress_logical_port,
                                              int* egress_logical_port,
                                              int* cos) = 0;

  // **************************************************************************
  // ACL Config Functions
  // **************************************************************************

  // Initialize ACL hardware for the given unit.
  virtual ::util::Status InitAclHardware(int unit) = 0;

  // Set hardware config flags related to ACL tables.
  virtual ::util::Status SetAclControl(int unit,
                                       const AclControl& acl_control) = 0;

  // Configure the set of user-defined field (UDF) chunks
  // <id, packet layer, byte offset> available for use as qualifiers in ACL
  // tables of the VFP and IFP stages on the given unit. We currently fix the
  // size of the UDF chunks to 2 bytes.
  virtual ::util::Status SetAclUdfChunks(int unit, const BcmUdfSet& udfs) = 0;

  // Get ACL UDF chunks.
  virtual ::util::Status GetAclUdfChunks(int unit, BcmUdfSet* udfs) = 0;

  // **************************************************************************
  // ACL Table Manipulation Functions
  // **************************************************************************

  // Create new ACL table (Field Processor group) on the given unit with the
  // given characteristics. Return generated table_id.
  virtual ::util::StatusOr<int> CreateAclTable(int unit,
                                               const BcmAclTable& table) = 0;

  // Destroy ACL table on given unit with given table id.
  virtual ::util::Status DestroyAclTable(int unit, int table_id) = 0;

  // Retrieve the configuration and qualifier set for the table with given id
  // from the given unit.
  virtual ::util::Status GetAclTable(int unit, int table_id,
                                     BcmAclTable* table) = 0;

  // **************************************************************************
  // ACL Flow Modification Functions
  // **************************************************************************

  // Insert ACL flow rule on the given unit. Returns the generated flow_id on
  // success, otherwise -1. Generates stat object if requested. If adding stats
  // and color_aware is true, enables red & green byte/packet counters,
  // otherwise enables total byte/packet counters.
  virtual ::util::StatusOr<int> InsertAclFlow(int unit,
                                              const BcmFlowEntry& flow,
                                              bool add_stats,
                                              bool color_aware) = 0;

  // Modify the specified flow rule to match the given BcmFlowEntry. This call
  // specifically will only modify the action set or the meter configuration.
  virtual ::util::Status ModifyAclFlow(int unit, int flow_id,
                                       const BcmFlowEntry& flow) = 0;

  // Remove ACL flow rule with given id from the given unit. Find and remove
  // stat object if there is one attached to the given flow.
  virtual ::util::Status RemoveAclFlow(int unit, int flow_id) = 0;

  // Retrieve the flow with given id from the given unit. Assumes flow is
  // populated with ACL stage.
  virtual ::util::Status GetAclFlow(int unit, int flow_id,
                                    BcmFlowEntry* flow) = 0;

  // **************************************************************************
  // ACL Flow Statistics Functions
  // **************************************************************************

  // Add stat object with either color-aware or non-color-aware counters to a
  // flow in a given table on a given unit.
  virtual ::util::Status AddAclStats(int unit, int table_id, int flow_id,
                                     bool color_aware) = 0;

  // Detach stat object from a flow on a given unit and then destroy the stat
  // object.
  virtual ::util::Status RemoveAclStats(int unit, int flow_id) = 0;

  // Obtain the stat counters associated with a flow on a given unit.
  virtual ::util::Status GetAclStats(int unit, int flow_id,
                                     BcmAclStats* stats) = 0;

  // **************************************************************************
  // ACL Flow Metering Functions
  // **************************************************************************

  // Modify policer attached to a flow if it exists, otherwise create a new one
  // with the given configuration.
  virtual ::util::Status SetAclPolicer(int unit, int flow_id,
                                       const BcmMeterConfig& meter) = 0;

  // **************************************************************************
  // ACL Verification Functions
  // **************************************************************************

  // Retrieve the list of all flow_ids in table given by table_id from given
  // unit.
  virtual ::util::Status GetAclTableFlowIds(int unit, int table_id,
                                            std::vector<int>* flow_ids) = 0;

  // Attempt to match the given flow against flows in the hardware. Only checks
  // fields, actions, and priority given in the input flow. Does not check flow
  // table_id. On unsuccessful match, returns error string detailing first
  // encountered diff. Otherwise returns empty string.
  virtual ::util::StatusOr<std::string> MatchAclFlow(
      int unit, int flow_id, const BcmFlowEntry& flow) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmSdkInterface() {}
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_INTERFACE_H_
