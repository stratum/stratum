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


#ifndef STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_H_

#include <net/ethernet.h>
#include <pthread.h>
#include <signal.h>

#include <functional>
#include <map>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/bcm/constants.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/hal.grpc.pb.h"
#include "stratum/glue/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "sandblaze/p4lang/p4/p4runtime.grpc.pb.h"
#include "util/gtl/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmPacketioManager;
class BcmKnetIntf;

// Encapsulates the data passed to the RX thread for each KNET interface.
struct KnetIntfRxThreadData {
  // Node ID of the node hosting the KNET interface.
  uint64 node_id;
  // The purpose for the KNET interface which this thread is serving.
  GoogleConfig::BcmKnetIntfPurpose purpose;
  // Pointer to the BcmPacketioManager class.
  BcmPacketioManager* mgr;  // not owned
  KnetIntfRxThreadData(uint64 _node_id,
                       GoogleConfig::BcmKnetIntfPurpose _purpose,
                       BcmPacketioManager* _mgr)
      : node_id(_node_id), purpose(_purpose), mgr(CHECK_NOTNULL(_mgr)) {}
};

// All the TX stats we collect for each KNET interface.
struct BcmKnetTxStats {
  // All TX packets (accepted + dropped + error)
  uint64 all_tx;
  // All accepted packets sent to ingress pipeline.
  uint64 tx_accepts_ingress_pipeline;
  // All accepted packets sent directly to a port/trunk.
  uint64 tx_accepts_direct;
  // TX packets encountered internal socket send errors.
  uint64 tx_errors_internal_send_failures;
  // TX packets that could not be sent completely.
  uint64 tx_errors_incomplete_send;
  // (Probably valid) TX packets dropped due to metadata parse failure.
  uint64 tx_drops_metadata_parse_error;
  // (Probably valid) TX packets dropped due to unknown port id.
  uint64 tx_drops_unknown_port;
  // (Probably valid) TX packets dropped due to egress port being down.
  uint64 tx_drops_down_port;
  // (Probably valid) TX packets dropped due to egress trunk being down (i.e.
  // all the ports in the trunk were down or trunk was empty).
  uint64 tx_drops_down_trunk;
  BcmKnetTxStats()
      : all_tx(0),
        tx_accepts_ingress_pipeline(0),
        tx_accepts_direct(0),
        tx_errors_internal_send_failures(0),
        tx_errors_incomplete_send(0),
        tx_drops_metadata_parse_error(0),
        tx_drops_unknown_port(0),
        tx_drops_down_port(0),
        tx_drops_down_trunk(0) {}
  std::string ToString() const {
    return absl::StrCat(
        "(all_tx:", all_tx,
        ", tx_accepts_ingress_pipeline:", tx_accepts_ingress_pipeline,
        ", tx_accepts_direct:", tx_accepts_direct,
        ", tx_errors_internal_send_failures:", tx_errors_internal_send_failures,
        ", tx_errors_incomplete_send:", tx_errors_incomplete_send,
        ", tx_drops_metadata_parse_error:", tx_drops_metadata_parse_error,
        ", tx_drops_unknown_port:", tx_drops_unknown_port,
        ", tx_drops_down_port:", tx_drops_down_port,
        ", tx_drops_down_trunk:", tx_drops_down_trunk, ")");
  }
};

// All the RX stats we collect for each KNET interface.
struct BcmKnetRxStats {
  // All RX packets (accepted + dropped + error), excluding the following:
  // - rx_errors_epoll_wait_failures
  // - rx_errors_internal_read_failures
  // - rx_errors_sock_shutdown
  uint64 all_rx;
  // All accepted packets read and sent to controller.
  uint64 rx_accepts;
  // Num of epoll_wait failures.
  uint64 rx_errors_epoll_wait_failures;
  // RX packets which could not be read due to internal socket read errors.
  uint64 rx_errors_internal_read_failures;
  // RX packets that could not be received due to socket shutdown.
  uint64 rx_errors_sock_shutdown;
  // RX packets that could not be read completely.
  uint64 rx_errors_incomplete_read;
  // RX packets with invalid format.
  uint64 rx_errors_invalid_packet;
  // RX packets dropped due to KNET header parse errors.
  uint64 rx_drops_knet_header_parse_error;
  // (Probably valid) RX packets dropped due to metadata deparse failure.
  uint64 rx_drops_metadata_deparse_error;
  // (Probably valid) RX packets dropped due to unknown ingress port.
  uint64 rx_drops_unknown_ingress_port;
  // (Probably valid) RX packets dropped due to unknown egress port.
  uint64 rx_drops_unknown_egress_port;
  BcmKnetRxStats()
      : all_rx(0),
        rx_accepts(0),
        rx_errors_epoll_wait_failures(0),
        rx_errors_internal_read_failures(0),
        rx_errors_sock_shutdown(0),
        rx_errors_incomplete_read(0),
        rx_errors_invalid_packet(0),
        rx_drops_knet_header_parse_error(0),
        rx_drops_metadata_deparse_error(0),
        rx_drops_unknown_ingress_port(0),
        rx_drops_unknown_egress_port(0) {}
  std::string ToString() const {
    return absl::StrCat(
        "(all_rx:", all_rx, ", rx_accepts:", rx_accepts,
        ", rx_errors_epoll_wait_failures:", rx_errors_epoll_wait_failures,
        ", rx_errors_internal_read_failures:", rx_errors_internal_read_failures,
        ", rx_errors_sock_shutdown:", rx_errors_sock_shutdown,
        ", rx_errors_incomplete_read:", rx_errors_incomplete_read,
        ", rx_errors_invalid_packet:", rx_errors_invalid_packet,
        ", rx_drops_knet_header_parse_error:", rx_drops_knet_header_parse_error,
        ", rx_drops_metadata_deparse_error:", rx_drops_metadata_deparse_error,
        ", rx_drops_unknown_ingress_port:", rx_drops_unknown_ingress_port,
        ", rx_drops_unknown_egress_port:", rx_drops_unknown_egress_port, ")");
  }
};

// This struct encapsulates all the settings for a KNET interface corresponding
// to a (node_id, purpose) pair, where purpose identifies which application
// will use the interface (controller, sflow, etc.). Each KNET interface on a
// node can have 'only' one purpose. These settings are NOT supposed to change
// after the first config is pushed successfully.
struct BcmKnetIntf {
  // The CPU queue for the netif.
  int cpu_queue;
  // MTU set for the netif.
  int mtu;
  // VLAN set for the netif. This VLAN will also be used to tag packets that
  // are supposed to go to ingress pipeline but are received without a VLAN tag.
  int vlan;
  // The name given to the netif.
  std::string netif_name;
  // The index of the netif as returned by the kernel.
  int netif_index;
  // The id for netif as returned by BCM SDK.
  int netif_id;
  // Source MAC address, to be used when setting up TX.
  uint64 smac;
  // The ids of all KNET filters setup for this interface.
  std::set<int> filter_ids;
  // TX socket fd.
  int tx_sock;
  // RX socket fd.
  int rx_sock;
  // The ID of the RX thread which is in charge of receiving the packets.
  pthread_t rx_thread_id;
  BcmKnetIntf()
      : cpu_queue(-1),
        mtu(0),
        vlan(kDefaultVlan),
        netif_name(""),
        netif_index(-1),
        netif_id(-1),
        smac(0),
        filter_ids(),
        tx_sock(-1),
        rx_sock(-1),
        rx_thread_id(0) {}
};

// Metadata we need to parse from each packet received from controller to
// understand where/how to transmit the packet.
struct PacketOutMetadata {
  // ID of the port to which we want to send the packet. Will be ignored if
  // use_ingress_pipeline = true or if egress_trunk_id > 0.
  uint64 egress_port_id;
  // ID of the trunk to which we want to send the packet. Will be ignored if
  // use_ingress_pipeline = true. If non-zero, we will ignore any given
  // egress_port_id and use one port from the given trunk randomly and send
  // the packet to it.
  uint64 egress_trunk_id;
  // CoS to for the egress packet. Not required if send to ingress pipeline.
  // If not given, we will let SDK to use the default CoS.
  int cos;
  // Determines if we need to send the packet to ingress pipeline.
  bool use_ingress_pipeline;
  PacketOutMetadata()
      : egress_port_id(0),
        egress_trunk_id(0),
        cos(kDefaultCos),
        use_ingress_pipeline(true) {}
};

// Metadata that we need to attach to each packet we send to controller to
// determine how/where the packet was received.
struct PacketInMetadata {
  // ID of the singleton port from which the packet was received. If the port is
  // also part of a trunk, the ID of the trunk will be given in ingress_trunk_id
  // below.
  uint64 ingress_port_id;
  // ID of the trunk to which ingress_port_id is part of. If ingress_port_id is
  // not part of any trunk, we will leave this field as zero.
  uint64 ingress_trunk_id;
  // ID of the port to which the packet copied to CPU was destined.
  uint64 egress_port_id;
  // The CoS for the received packet.
  int cos;
  // TODO: How about reason bit. Should we capture that as well?
  PacketInMetadata()
      : ingress_port_id(0),
        ingress_trunk_id(0),
        egress_port_id(0),
        cos(kDefaultCos) {}
};

// The "BcmPacketioManager" class is in charge of setting up and managing KNET
// interface(s) for packet I/O.
// TODO: Add stats collection functionality.
class BcmPacketioManager {
 public:
  virtual ~BcmPacketioManager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about. If the class is not initialized (i.e. if config is pushed for the
  // first time), this function also initializes class. As part of
  // initialization, this function will initialize the KNET interfaces for this
  // node and starts RX/TX. After initialization is done, as part of config
  // push KNET will not be re-initialized and only the parts of the config which
  // do not need KNET restart will be applied. The given node_id is used to
  // understand which part of the ChassisConfig is intended for this class.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about.
  // The given node_id is used to understand which part of the ChassisConfig is
  // intended for this class.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Registers a writer to be invoked when we capture a packet on a KNET
  // interface which is created for a specific application (given by 'purpose')
  // on the node which this class is mapped to.
  virtual ::util::Status RegisterPacketReceiveWriter(
      GoogleConfig::BcmKnetIntfPurpose purpose,
      const std::shared_ptr<WriterInterface<::p4::PacketIn>>& writer);

  virtual ::util::Status UnregisterPacketReceiveWriter(
      GoogleConfig::BcmKnetIntfPurpose purpose);

  // Transmits a packet to the KNET interface which is created for a specific
  // application (given by 'purpose') on the node which this class is mapped to.
  virtual ::util::Status TransmitPacket(
      GoogleConfig::BcmKnetIntfPurpose purpose, const ::p4::PacketOut& packet);

  // Return copies of BcmKnetTxStats/BcmKnetRxStats for a given purpose
  // respectively. Returns error if the given purpose is not found in the
  // corresponding map (which may mean no stats has been collected from that
  // KNET intf).
  virtual ::util::StatusOr<BcmKnetTxStats> GetTxStats(
      GoogleConfig::BcmKnetIntfPurpose purpose) const
      LOCKS_EXCLUDED(tx_stats_lock_);
  virtual ::util::StatusOr<BcmKnetRxStats> GetRxStats(
      GoogleConfig::BcmKnetIntfPurpose purpose) const
      LOCKS_EXCLUDED(rx_stats_lock_);

  // Returns the RX/TX stats for all KNET intfs as string. It also dumps the
  // string to stdout.
  virtual std::string DumpStats() const
      LOCKS_EXCLUDED(tx_stats_lock_, rx_stats_lock_);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmPacketioManager> CreateInstance(
      OperationMode mode, BcmChassisManager* bcm_chassis_manager,
      P4TableMapper* p4_table_mapper, BcmSdkInterface* bcm_sdk_interface,
      int unit);

  // BcmPacketioManager is neither copyable nor movable.
  BcmPacketioManager(const BcmPacketioManager&) = delete;
  BcmPacketioManager& operator=(const BcmPacketioManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmPacketioManager();

 private:
  static constexpr int kDefaultKnetIntfMtu = 3000;
  static constexpr char kNetifNameTemplate[] = "knet-%d-$0";
  static constexpr int kDefaultRxPoolPktCount = 256;
  static constexpr int kDefaultRxPoolBytesPerPkt = 2048;
  static constexpr int kDefaultMaxPktSizeBytes = 2048;
  static constexpr int kDefaultPktsPerChain = 4;
  static constexpr int kDefaultDmaChannel = 1;
  static constexpr int kDefaultDmaChannelChains = 4;
  static constexpr int kDefaultMaxRatePps = 1600;
  static constexpr int kDefaultBurstPps = 512;
  static constexpr size_t kMaxRxBufferSize = 32768;

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmPacketioManager(OperationMode mode, BcmChassisManager* bcm_chassis_manager,
                     P4TableMapper* p4_table_mapper,
                     BcmSdkInterface* bcm_sdk_interface, int unit);

  // Helper to parse the config and return any RX/TX/KNET/rate limit config for
  // a given node ID.
  void ParseConfig(
      const ChassisConfig& config, uint64 node_id,
      GoogleConfig::BcmRxConfig* bcm_rx_config,
      GoogleConfig::BcmTxConfig* bcm_tx_config,
      GoogleConfig::BcmKnetConfig* bcm_knet_config,
      GoogleConfig::BcmRateLimitConfig* bcm_rate_limit_config) const;

  // Start RX on given unit. The RX parameters are given by 'bcm_rx_config'.
  ::util::Status StartRx(const GoogleConfig::BcmRxConfig& bcm_rx_config) const;

  // Starts TX on a given unit. The RX parameters are given by 'bcm_tx_config'.
  ::util::Status StartTx(const GoogleConfig::BcmTxConfig& bcm_tx_config) const;

  // Returns the name template for the KNET interfaces.
  std::string GetKnetIntfNameTemplate(GoogleConfig::BcmKnetIntfPurpose purpose,
                                      int cpu_queue) const;

  // Sets up the KNET interface(s) for a given unit (aka node). Called in
  // PushConfig(). The function parses the given 'bcm_knet_config' and fills up
  // the given 'bcm_knet_node'.
  ::util::Status SetupKnetIntfs(
      const GoogleConfig::BcmKnetConfig& bcm_knet_config);

  // Helper to setup KNET interface for a given purpose on a unit. Called in
  // SetupKnetIntfs().
  ::util::Status SetupSingleKnetIntf(GoogleConfig::BcmKnetIntfPurpose purpose,
                                     BcmKnetIntf* intf) const;

  // Sets up RX rate limits. The rate limit parameters are given by
  // 'bcm_rate_limit_config'.
  ::util::Status SetRateLimit(
      const GoogleConfig::BcmRateLimitConfig& bcm_rate_limit_config) const;

  // Returns a pointer to an already existing  BcmKnetIntf instance which
  // corresponds to the given purpose the node this class is mapped to. Returns
  // error if it cannot find the instance.
  ::util::StatusOr<BcmKnetIntf*> GetBcmKnetIntf(
      GoogleConfig::BcmKnetIntfPurpose purpose);

  // Called in the context of the KNET interface RX thread. Includes a loop to
  // receive the packets from a given KNET interface and forward it to the
  // registered callback (if any).
  ::util::Status HandleKnetIntfPacketRx(
      GoogleConfig::BcmKnetIntfPurpose purpose)
      LOCKS_EXCLUDED(chassis_lock, rx_writer_lock_);

  // Helper called by HandleKnetIntfPacketRx() to read one single full message
  // from a socket. Returns true if we need to retry the receive and false if
  // otherwise. If any non-recoverable error is encountered, returns error.
  ::util::StatusOr<bool> RxPacket(GoogleConfig::BcmKnetIntfPurpose purpose,
                                  int sock, int netif_index,
                                  std::string* header, std::string* payload);

  // Deparses the given PacketInMetadata to the a set of ::p4::PacketMetadata
  // protos in the given ::p4::PacketIn which is then sent to the controller.
  ::util::Status DeparsePacketInMetadata(const PacketInMetadata& meta,
                                         ::p4::PacketIn* packet);

  // Helper called by TransmitPacket() to send packet (KNET headers + payload).
  ::util::Status TxPacket(GoogleConfig::BcmKnetIntfPurpose purpose, int sock,
                          int vlan, int netif_index, bool direct_tx,
                          const std::string& header,
                          const std::string& payload);

  // Parses the ::p4::PacketMetadata protos in the given ::p4::PacketOut and
  // fills in the given PacketOutMetadata proto, which is then used to transmit
  // the packet (directly to a port or to ingress pipeline).
  ::util::Status ParsePacketOutMetadata(const ::p4::PacketOut& packet,
                                        PacketOutMetadata* meta);

  // KNET interface RX thread function.
  static void* KnetIntfRxThreadFunc(void* arg);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Hercules stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Hercules stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Hercules stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Mutex lock for protecting the node_id_purpose_to_rx_writer_ map.
  mutable absl::Mutex rx_writer_lock_;

  // Mutex lock for protecting the purpose_to_tx_stats_ map.
  mutable absl::Mutex tx_stats_lock_;

  // Mutex lock for protecting the purpose_to_rx_stats_ map.
  mutable absl::Mutex rx_stats_lock_;

  // Map from KNET interface purpose (specifying which application will use the
  // interface, e.g. controller, sflow, etc.) to the BcmKnetIntf instance
  // encapsulating the settings for that KNET interface. Each node can only
  // have one KNET interface for each purpose.
  std::map<GoogleConfig::BcmKnetIntfPurpose, BcmKnetIntf> purpose_to_knet_intf_;

  // Maps from logical ports on the node this class is mapped to its
  // corresponding port ID, as well as its reverse counterpart. Used to
  // translate the port where a packet is received from to port_id as well as
  // to translate the port_id received on a TX packet to logical port to
  // transmit the packet.
  gtl::flat_hash_map<int, uint64> logical_port_to_port_id_;
  gtl::flat_hash_map<uint64, int> port_id_to_logical_port_;

  // Map from trunk ID to the set of member port IDs on the node this class is
  // mapped to. Used to pick a member randomly when packet is sent to a trunk.
  gtl::flat_hash_map<uint64, std::set<uint64>> trunk_id_to_member_port_ids_;

  // Map from port ID to its parent trunk ID on the node this class is mapped
  // to. A port exists as a key iff it is part of a trunk.
  gtl::flat_hash_map<uint64, uint64> port_id_to_parent_trunk_id_;

  // Map from node ID to a copy of BcmRxConfig received from pushed config.
  // Updated only after the config push is successful to make sure at any point
  // this map includes the last successfully pushed RX config.
  std::unique_ptr<GoogleConfig::BcmRxConfig> bcm_rx_config_;

  // Map from node ID to a copy of BcmTxConfig received from pushed config.
  // Updated only after the config push is successful to make sure at any point
  // this map includes the last successfully pushed TX config.
  std::unique_ptr<GoogleConfig::BcmTxConfig> bcm_tx_config_;

  // Map from node ID to a copy of BcmKnetConfig received from pushed config.
  // Updated only after the config push is successful to make sure at any point
  // this map includes the last successfully pushed KNET config.
  std::unique_ptr<GoogleConfig::BcmKnetConfig> bcm_knet_config_;

  // Map from node ID to a copy of BcmRateLimitConfig received from pushed
  // config. Updated only after the config push is successful to make sure at
  // any point this map includes the last successfully pushed RX rate limit
  // config.
  std::unique_ptr<GoogleConfig::BcmRateLimitConfig> bcm_rate_limit_config_;

  // Map from purpose for a KNET interface to the RX packet handler. This map
  // is updated every time a controller is connected.
  std::map<GoogleConfig::BcmKnetIntfPurpose,
           std::shared_ptr<WriterInterface<::p4::PacketIn>>>
      purpose_to_rx_writer_ GUARDED_BY(rx_writer_lock_);

  // A vector of KnetIntfRxThreadData pointers.
  std::vector<KnetIntfRxThreadData*> knet_intf_rx_thread_data_;

  // Map from purpose of a KNET intf to its TX stats. The map entries are
  // created when there is a packet transmitted for the first time from a KNET
  // intf mapped and are updated continuously till class is shutdown.
  std::map<GoogleConfig::BcmKnetIntfPurpose, BcmKnetTxStats>
      purpose_to_tx_stats_ GUARDED_BY(tx_stats_lock_);

  // Map from purpose of a KNET intf to its RX stats. The map entries are
  // created when there is a packet received for the first time from a KNET
  // intf mapped and are updated continuously till class is shutdown.
  std::map<GoogleConfig::BcmKnetIntfPurpose, BcmKnetRxStats>
      purpose_to_rx_stats_ GUARDED_BY(rx_stats_lock_);

  // Pointer to BcmChassisManager class to get the most updated node & port
  // maps after the config is pushed. THIS CLASS MUST NOT CALL ANY METHOD
  // WHICH CAN MODIFY THE STATE OF BcmChassisManager OBJECT.
  BcmChassisManager* bcm_chassis_manager_;  // not owned by this class.

  // Pointer to P4TableMapper for parsing/deparsing PacketIn/PacketOut metadata.
  P4TableMapper* p4_table_mapper_;  // not owned by this class.

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  friend class BcmPacketioManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_PACKETIO_MANAGER_H_
