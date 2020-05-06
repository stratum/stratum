// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/filter.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"

DEFINE_int32(knet_rx_buf_size, 512 * 1024,
             "KNET RX socket buffer size (0 = kernel default).");
DEFINE_int32(knet_rx_poll_timeout_ms, 100,
             "Polling timeout to check incoming packets from KNET RX sockets.");
DEFINE_int32(knet_max_num_packets_to_read_at_once, 8,
             "Determines the number of packets we try to read at once as soon "
             "as the socket FD becomes available.");

// TODO(unknown): I really really wish we could use google3 thread libraries.
namespace stratum {
namespace hal {
namespace bcm {

constexpr int BcmPacketioManager::kDefaultKnetIntfMtu;
constexpr char BcmPacketioManager::kNetifNameTemplate[];
constexpr int BcmPacketioManager::kDefaultRxPoolPktCount;
constexpr int BcmPacketioManager::kDefaultRxPoolBytesPerPkt;
constexpr int BcmPacketioManager::kDefaultMaxPktSizeBytes;
constexpr int BcmPacketioManager::kDefaultPktsPerChain;
constexpr int BcmPacketioManager::kDefaultDmaChannel;
constexpr int BcmPacketioManager::kDefaultDmaChannelChains;
constexpr size_t BcmPacketioManager::kMaxRxBufferSize;

namespace {

// Macros to increment the RX/TX counters for a KNET intf. MUST be called inside
// the class methods only as it accesses class member variables.
#define INCREMENT_TX_COUNTER(purpose, counter) \
  do {                                         \
    absl::WriterMutexLock l(&tx_stats_lock_);  \
    purpose_to_tx_stats_[purpose].counter++;   \
  } while (0)

#define INCREMENT_RX_COUNTER(purpose, counter) \
  do {                                         \
    absl::WriterMutexLock l(&rx_stats_lock_);  \
    purpose_to_rx_stats_[purpose].counter++;   \
  } while (0)

}  // namespace

BcmPacketioManager::BcmPacketioManager(
    OperationMode mode, BcmChassisRoInterface* bcm_chassis_ro_interface,
    P4TableMapper* p4_table_mapper, BcmSdkInterface* bcm_sdk_interface,
    int unit)
    : mode_(mode),
      purpose_to_knet_intf_(),
      logical_port_to_port_id_(),
      port_id_to_logical_port_(),
      bcm_rx_config_(nullptr),
      bcm_tx_config_(nullptr),
      bcm_knet_config_(nullptr),
      bcm_rate_limit_config_(nullptr),
      purpose_to_rx_writer_(),
      knet_intf_rx_thread_data_(),
      purpose_to_tx_stats_(),
      purpose_to_rx_stats_(),
      bcm_chassis_ro_interface_(ABSL_DIE_IF_NULL(bcm_chassis_ro_interface)),
      p4_table_mapper_(ABSL_DIE_IF_NULL(p4_table_mapper)),
      bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      node_id_(0),
      unit_(unit) {}

// Default constructor is called by the mock class only.
BcmPacketioManager::BcmPacketioManager()
    : mode_(OPERATION_MODE_STANDALONE),
      purpose_to_knet_intf_(),
      logical_port_to_port_id_(),
      port_id_to_logical_port_(),
      bcm_rx_config_(nullptr),
      bcm_tx_config_(nullptr),
      bcm_knet_config_(nullptr),
      bcm_rate_limit_config_(nullptr),
      purpose_to_rx_writer_(),
      knet_intf_rx_thread_data_(),
      purpose_to_tx_stats_(),
      purpose_to_rx_stats_(),
      bcm_chassis_ro_interface_(nullptr),
      p4_table_mapper_(nullptr),
      bcm_sdk_interface_(nullptr),
      node_id_(0),
      unit_(-1) {}

BcmPacketioManager::~BcmPacketioManager() {}

::util::Status BcmPacketioManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  node_id_ = node_id;  // Save node_id ASAP to ensure all the methods can refer
                       // to correct ID in the messages/errors.

  // Simulation mode does not support KNET.
  // TODO(unknown): Find a way to do packet I/O in sim mode.
  if (mode_ == OPERATION_MODE_SIM) {
    LOG(WARNING) << "Skipped pushing config to BcmPacketioManager for node "
                 << node_id_ << " in sim mode.";
    return ::util::OkStatus();
  }

  // Now go over all the nodes in node_id_to_unit and node_id_to_knet_node_.
  // We have the following cases:
  // 1- If the config is pushed for the first time, purpose_to_knet_intf_
  //    will be empty and bcm_rx_config_, bcm_tx_config_, bcm_tx_config_, and
  //    bcm_rate_limit_config_ will all be nullptr. In this case, configure and
  //    start RX/TX and create the KNET interface(s) for this node. At any
  //    stage, if the operation goes OK update the internal state.
  // 2- For configs that are pushed later on, we only retry the operations which
  //    did not go well before.

  // First see if the pushed config includes RX, TX, KNET, and RATE LIMIT
  // configs. The unique_ptrs below will be untouched if the corresponding
  // config is missing.
  std::unique_ptr<GoogleConfig::BcmRxConfig> bcm_rx_config(
      new GoogleConfig::BcmRxConfig());
  std::unique_ptr<GoogleConfig::BcmTxConfig> bcm_tx_config(
      new GoogleConfig::BcmTxConfig());
  std::unique_ptr<GoogleConfig::BcmKnetConfig> bcm_knet_config(
      new GoogleConfig::BcmKnetConfig());
  std::unique_ptr<GoogleConfig::BcmRateLimitConfig> bcm_rate_limit_config(
      new GoogleConfig::BcmRateLimitConfig());
  ParseConfig(config, node_id, bcm_rx_config.get(), bcm_tx_config.get(),
              bcm_knet_config.get(), bcm_rate_limit_config.get());

  // Now try to start RX and TX before setting up KNET interfaces. Save the
  // configs only after the operations were successful. Note that in case of
  // coupled mode starting RX/TX is NOOP.
  if (bcm_rx_config_ == nullptr) {
    RETURN_IF_ERROR(StartRx(*bcm_rx_config));
    bcm_rx_config_ = std::move(bcm_rx_config);
  }
  if (bcm_tx_config_ == nullptr) {
    RETURN_IF_ERROR(StartTx(*bcm_tx_config));
    bcm_tx_config_ = std::move(bcm_tx_config);
  }

  // Now setup the KNET interfaces for this node and spawn the RX thread(s).
  // Save both the KNET node and KNET config after the operation was successful.
  if (bcm_knet_config_ == nullptr) {
    RETURN_IF_ERROR(SetupKnetIntfs(*bcm_knet_config));
    bcm_knet_config_ = std::move(bcm_knet_config);
  }

  // In all cases, try to set rate limiters for RX. This is not considered
  // disruptive and can be setup at any time. If the rate limit config is empty,
  // do nothing.
  RETURN_IF_ERROR(SetRateLimit(*bcm_rate_limit_config));
  bcm_rate_limit_config_ = std::move(bcm_rate_limit_config);

  // The last step is to update the port_id_to_logical_port_ and
  // logical_port_to_port_id_ (reverse of port_id_to_logical_port_) maps using
  // the last updated maps from BcmChassisRoInterface. This is done after each
  // push and is not disruptive. This way BcmPacketioManager will always have
  // the most updated port maps.
  ASSIGN_OR_RETURN(const auto& port_id_to_sdk_port,
                   bcm_chassis_ro_interface_->GetPortIdToSdkPortMap(node_id));
  absl::flat_hash_map<int, uint32> logical_port_to_port_id;
  absl::flat_hash_map<uint32, int> port_id_to_logical_port;
  for (const auto& e : port_id_to_sdk_port) {
    if (e.second.unit != unit_) {
      // Any error here is an internal error. Must not happen.
      return MAKE_ERROR(ERR_INTERNAL)
             << "Something is wrong: " << e.second.unit << " != " << unit_
             << " for a singleton port " << e.first << ".";
    }
    logical_port_to_port_id[e.second.logical_port] = e.first;
    port_id_to_logical_port[e.first] = e.second.logical_port;
  }
  logical_port_to_port_id_ = logical_port_to_port_id;
  port_id_to_logical_port_ = port_id_to_logical_port;

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  if (node_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid node ID.";
  }
  if (node_id_ > 0 && node_id_ != node_id) {
    return MAKE_ERROR(ERR_REBOOT_REQUIRED)
           << "Detected a change in the node_id (" << node_id_ << " vs "
           << node_id << ").";
  }

  // If the node has been configureed, make sure there is no change in the
  // RX/TX/KNET configs in the pushed config. If a change is detected we will
  // report reboot required.
  GoogleConfig::BcmRxConfig bcm_rx_config;
  GoogleConfig::BcmTxConfig bcm_tx_config;
  GoogleConfig::BcmKnetConfig bcm_knet_config;
  if (config.has_vendor_config() &&
      config.vendor_config().has_google_config()) {
    const auto& google_config = config.vendor_config().google_config();
    for (const auto& e : google_config.node_id_to_rx_config()) {
      if (e.first == node_id) {
        bcm_rx_config = e.second;
        break;
      }
    }
    for (const auto& e : google_config.node_id_to_tx_config()) {
      if (e.first == node_id) {
        bcm_tx_config = e.second;
        break;
      }
    }
    for (const auto& e : google_config.node_id_to_knet_config()) {
      if (e.first == node_id) {
        bcm_knet_config = e.second;
        break;
      }
    }
  }

  ::util::Status status = ::util::OkStatus();
  if (bcm_rx_config_ != nullptr &&
      !ProtoEqual(*bcm_rx_config_, bcm_rx_config)) {
    ::util::Status error = MAKE_ERROR(ERR_REBOOT_REQUIRED)
                           << "Detected a change in BcmRxConfig for node_id: "
                           << node_id;
    APPEND_STATUS_IF_ERROR(status, error);
  }
  if (bcm_tx_config_ != nullptr &&
      !ProtoEqual(*bcm_tx_config_, bcm_tx_config)) {
    ::util::Status error = MAKE_ERROR(ERR_REBOOT_REQUIRED)
                           << "Detected a change in BcmTxConfig for node_id: "
                           << node_id;
    APPEND_STATUS_IF_ERROR(status, error);
  }
  if (bcm_knet_config_ != nullptr &&
      !ProtoEqual(*bcm_knet_config_, bcm_knet_config)) {
    ::util::Status error = MAKE_ERROR(ERR_REBOOT_REQUIRED)
                           << "Detected a change in BcmKnetConfig for node_id: "
                           << node_id;
    APPEND_STATUS_IF_ERROR(status, error);
  }

  return status;
}

::util::Status BcmPacketioManager::Shutdown() {
  // Simulation mode does not support KNET.
  // TODO(unknown): Find a way to do packet I/O in sim mode.
  if (mode_ == OPERATION_MODE_SIM) {
    LOG(WARNING) << "Skipped shutting down BcmPacketioManager for node "
                 << node_id_ << " in sim mode.";
    node_id_ = 0;
    return ::util::OkStatus();
  }

  ::util::Status status = ::util::OkStatus();
  // Wait for all the threads to join. All threads exit once shutdown has
  // been set true.
  for (const auto& entry : purpose_to_knet_intf_) {
    if (entry.second.rx_thread_id > 0 &&
        pthread_join(entry.second.rx_thread_id, nullptr) != 0) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Failed to join thread "
                             << entry.second.rx_thread_id;
      APPEND_STATUS_IF_ERROR(status, error);
    }
  }
  // Perform the rest of the shutdown. First close the TX/RX sockets and
  // destroy all the KNET filters and KNET interfaces.
  for (const auto& entry : purpose_to_knet_intf_) {
    if (entry.second.tx_sock != -1) {
      close(entry.second.tx_sock);
    }
    if (entry.second.rx_sock != -1) {
      close(entry.second.rx_sock);
    }
    for (int id : entry.second.filter_ids) {
      APPEND_STATUS_IF_ERROR(status,
                             bcm_sdk_interface_->DestroyKnetFilter(unit_, id));
    }
    if (entry.second.netif_id != -1) {
      APPEND_STATUS_IF_ERROR(status, bcm_sdk_interface_->DestroyKnetIntf(
                                         unit_, entry.second.netif_id));
    }
  }
  // Then stop RX only in the standalone mode. In the coupled mode we let
  // Sandcastle HAL take care of the stop, the call in BcmSdkInterface is
  // NOOP.
  APPEND_STATUS_IF_ERROR(status, bcm_sdk_interface_->StopRx(unit_));

  // Finally the state cleanup.
  purpose_to_knet_intf_.clear();
  logical_port_to_port_id_.clear();
  port_id_to_logical_port_.clear();
  bcm_rx_config_.reset(nullptr);
  bcm_tx_config_.reset(nullptr);
  bcm_knet_config_.reset(nullptr);
  bcm_rate_limit_config_.reset(nullptr);
  {
    absl::WriterMutexLock l(&rx_writer_lock_);
    purpose_to_rx_writer_.clear();
  }
  gtl::STLDeleteElements(&knet_intf_rx_thread_data_);
  {
    absl::WriterMutexLock l(&tx_stats_lock_);
    purpose_to_tx_stats_.clear();
  }
  {
    absl::WriterMutexLock l(&rx_stats_lock_);
    purpose_to_rx_stats_.clear();
  }
  node_id_ = 0;

  return status;
}

::util::Status BcmPacketioManager::RegisterPacketReceiveWriter(
    GoogleConfig::BcmKnetIntfPurpose purpose,
    const std::shared_ptr<WriterInterface<::p4::v1::PacketIn>>& writer) {
  if (mode_ == OPERATION_MODE_SIM) {
    LOG(WARNING) << "Skipped registering packet RX writer in "
                 << "BcmPacketioManager in sim mode for node with ID "
                 << node_id_ << " mapped to unit " << unit_ << ".";
    return ::util::OkStatus();
  }

  // Used only to check the validity of the given purpose. Note that purpose is
  // already known (after config is pushed), we do not expect any more change
  // in the corresponding BcmKnetIntf. Any change by later config pushes will
  // be rejected.
  ASSIGN_OR_RETURN(const BcmKnetIntf* intf, GetBcmKnetIntf(purpose));
  {
    // If it is a valid purpose, update the internal map.
    absl::WriterMutexLock l(&rx_writer_lock_);
    purpose_to_rx_writer_[purpose] = writer;
  }
  LOG(INFO) << "Registered packet RX writer for KNET interface "
            << intf->netif_name << " with purpose "
            << GoogleConfig::BcmKnetIntfPurpose_Name(purpose)
            << " on node with ID " << node_id_ << " mapped to unit " << unit_
            << ".";

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::UnregisterPacketReceiveWriter(
    GoogleConfig::BcmKnetIntfPurpose purpose) {
  if (mode_ == OPERATION_MODE_SIM) {
    LOG(WARNING) << "Skipped unregistering packet RX writer in "
                 << "BcmPacketioManager in sim mode for node with ID "
                 << node_id_ << " mapped to unit " << unit_ << ".";
    return ::util::OkStatus();
  }

  // Used only to check the validity of the given purpose. Note that purpose is
  // already known (after config is pushed), we do not expect any more change
  // in the corresponding BcmKnetIntf. Any change by later config pushes will
  // be rejected.
  ASSIGN_OR_RETURN(const BcmKnetIntf* intf, GetBcmKnetIntf(purpose));
  {
    // If it is a valid purpose, update the internal map.
    absl::WriterMutexLock l(&rx_writer_lock_);
    purpose_to_rx_writer_.erase(purpose);
  }
  LOG(INFO) << "Unregistered packet RX writer for KNET interface "
            << intf->netif_name << " with purpose "
            << GoogleConfig::BcmKnetIntfPurpose_Name(purpose)
            << " on node with ID " << node_id_ << " mapped to unit " << unit_
            << ".";
  return ::util::OkStatus();
}

// TODO(max): check if callers handle errors
::util::Status BcmPacketioManager::TransmitPacket(
    GoogleConfig::BcmKnetIntfPurpose purpose,
    const ::p4::v1::PacketOut& packet) {
  ASSIGN_OR_RETURN(const BcmKnetIntf* intf, GetBcmKnetIntf(purpose));
  CHECK_RETURN_IF_FALSE(intf->tx_sock > 0)  // MUST NOT HAPPEN!
      << "KNET interface with purpose "
      << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << " on node with ID "
      << node_id_ << " mapped to unit " << unit_
      << " does not have a TX socket.";

  INCREMENT_TX_COUNTER(purpose, all_tx);

  // Try to find the port/cos to send the packet to. Also find out if we need to
  // send the packet to ingress pipeline.
  PacketOutMetadata meta;
  ::util::Status status = ParsePacketOutMetadata(packet, &meta);
  if (!status.ok()) {
    INCREMENT_TX_COUNTER(purpose, tx_drops_metadata_parse_error);
  }
  VLOG(1) << "PacketOutMetadata.egress_port_id: " << meta.egress_port_id << "\n"
          << "PacketOutMetadata.egress_trunk_id: " << meta.egress_trunk_id
          << "\n"
          << "PacketOutMetadata.cos: " << meta.cos << "\n"
          << "PacketOutMetadata.use_ingress_pipeline: "
          << meta.use_ingress_pipeline;

  // Now try to send the packet. There are several cases:
  // 1- Direct packet to physical port.
  // 2- Direct packet to trunk port. In this case we send the packet to the
  //    first member of the trunk which is up.
  // 3- Packet to ingress pipeline.
  if (!meta.use_ingress_pipeline) {
    uint32 port_id = 0;
    if (meta.egress_trunk_id > 0) {
      // TX to trunk. Select the first member of the trunk which is up.
      ASSIGN_OR_RETURN(const std::set<uint32>& members,
                       bcm_chassis_ro_interface_->GetTrunkMembers(
                           node_id_, meta.egress_trunk_id));
      if (!members.empty()) {
        for (uint32 member : members) {
          ASSIGN_OR_RETURN(
              PortState port_state,
              bcm_chassis_ro_interface_->GetPortState(node_id_, member));
          if (port_state == PORT_STATE_UP) {
            port_id = member;
            break;
          }
        }
      }
      if (port_id == 0) {
        INCREMENT_TX_COUNTER(purpose, tx_drops_down_trunk);
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Trunk with ID " << meta.egress_trunk_id
               << " does not have any UP port.";
      }
    } else {
      // TX to regular port. If the port is not up we should discard it.
      ASSIGN_OR_RETURN(PortState port_state,
                       bcm_chassis_ro_interface_->GetPortState(
                           node_id_, meta.egress_port_id));
      if (port_state != PORT_STATE_UP) {
        INCREMENT_TX_COUNTER(purpose, tx_drops_down_port);
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Port with ID " << meta.egress_port_id << " is not UP.";
      }
      port_id = meta.egress_port_id;
    }
    int* logical_port = gtl::FindOrNull(port_id_to_logical_port_, port_id);
    if (logical_port == nullptr) {
      INCREMENT_TX_COUNTER(purpose, tx_drops_unknown_port);
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Port ID " << port_id
             << " not found in port_id_to_logical_port_.";
    }
    std::string header = "";
    RETURN_IF_ERROR(bcm_sdk_interface_->GetKnetHeaderForDirectTx(
        unit_, *logical_port, meta.cos, intf->smac, packet.payload().size(),
        &header));
    RETURN_IF_ERROR(TxPacket(purpose, intf->tx_sock, intf->vlan,
                             intf->netif_index, true, header,
                             packet.payload()));
    INCREMENT_TX_COUNTER(purpose, tx_accepts_direct);
  } else {
    std::string header = "";
    RETURN_IF_ERROR(bcm_sdk_interface_->GetKnetHeaderForIngressPipelineTx(
        unit_, intf->smac, packet.payload().size(), &header));
    RETURN_IF_ERROR(TxPacket(purpose, intf->tx_sock, intf->vlan,
                             intf->netif_index, false, header,
                             packet.payload()));
    INCREMENT_TX_COUNTER(purpose, tx_accepts_ingress_pipeline);
  }

  return ::util::OkStatus();
}

::util::StatusOr<BcmKnetTxStats> BcmPacketioManager::GetTxStats(
    GoogleConfig::BcmKnetIntfPurpose purpose) const {
  absl::ReaderMutexLock l(&tx_stats_lock_);
  const BcmKnetTxStats* stats = gtl::FindOrNull(purpose_to_tx_stats_, purpose);
  CHECK_RETURN_IF_FALSE(stats != nullptr)
      << "TX stats for KNET intf "
      << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << " not found on node "
      << node_id_ << ".";

  return *stats;
}

::util::StatusOr<BcmKnetRxStats> BcmPacketioManager::GetRxStats(
    GoogleConfig::BcmKnetIntfPurpose purpose) const {
  absl::ReaderMutexLock l(&rx_stats_lock_);
  const BcmKnetRxStats* stats = gtl::FindOrNull(purpose_to_rx_stats_, purpose);
  CHECK_RETURN_IF_FALSE(stats != nullptr)
      << "RX stats for KNET intf "
      << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << " not found on node "
      << node_id_ << ".";

  return *stats;
}

::util::Status BcmPacketioManager::InsertPacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {
  return bcm_sdk_interface_->InsertPacketReplicationEntry(entry);
}

::util::Status BcmPacketioManager::DeletePacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {
  return bcm_sdk_interface_->DeletePacketReplicationEntry(entry);
}

std::string BcmPacketioManager::DumpStats() const {
  std::string msg = "";
  {
    absl::ReaderMutexLock l(&tx_stats_lock_);
    for (const auto& e : purpose_to_tx_stats_) {
      absl::StrAppend(&msg, "\nTX stats for KNET intf ",
                      GoogleConfig::BcmKnetIntfPurpose_Name(e.first), ": ",
                      e.second.ToString());
    }
  }
  {
    absl::ReaderMutexLock l(&rx_stats_lock_);
    for (const auto& e : purpose_to_rx_stats_) {
      absl::StrAppend(&msg, "\nRX stats for KNET intf ",
                      GoogleConfig::BcmKnetIntfPurpose_Name(e.first), ": ",
                      e.second.ToString());
    }
  }

  LOG(INFO) << msg;
  return msg;
}

std::unique_ptr<BcmPacketioManager> BcmPacketioManager::CreateInstance(
    OperationMode mode, BcmChassisRoInterface* bcm_chassis_ro_interface,
    P4TableMapper* p4_table_mapper, BcmSdkInterface* bcm_sdk_interface,
    int unit) {
  return std::unique_ptr<BcmPacketioManager>(
      new BcmPacketioManager(mode, bcm_chassis_ro_interface, p4_table_mapper,
                             bcm_sdk_interface, unit));
}

void BcmPacketioManager::ParseConfig(
    const ChassisConfig& config, uint64 node_id,
    GoogleConfig::BcmRxConfig* bcm_rx_config,
    GoogleConfig::BcmTxConfig* bcm_tx_config,
    GoogleConfig::BcmKnetConfig* bcm_knet_config,
    GoogleConfig::BcmRateLimitConfig* bcm_rate_limit_config) const {
  if (config.has_vendor_config() &&
      config.vendor_config().has_google_config()) {
    const auto& node_id_to_rx_config =
        config.vendor_config().google_config().node_id_to_rx_config();
    const auto& node_id_to_tx_config =
        config.vendor_config().google_config().node_id_to_tx_config();
    const auto& node_id_to_knet_config =
        config.vendor_config().google_config().node_id_to_knet_config();
    const auto& node_id_to_rate_limit_config =
        config.vendor_config().google_config().node_id_to_rate_limit_config();
    if (bcm_rx_config != nullptr) {
      auto it = node_id_to_rx_config.find(node_id);
      if (it != node_id_to_rx_config.end()) {
        *bcm_rx_config = it->second;
      }
    }
    if (bcm_tx_config != nullptr) {
      auto it = node_id_to_tx_config.find(node_id);
      if (it != node_id_to_tx_config.end()) {
        *bcm_tx_config = it->second;
      }
    }
    if (bcm_knet_config != nullptr) {
      auto it = node_id_to_knet_config.find(node_id);
      if (it != node_id_to_knet_config.end()) {
        *bcm_knet_config = it->second;
      }
    }
    if (bcm_rate_limit_config != nullptr) {
      auto it = node_id_to_rate_limit_config.find(node_id);
      if (it != node_id_to_rate_limit_config.end()) {
        *bcm_rate_limit_config = it->second;
      }
    }
  }
}

namespace {

int GetWithDefault(int value, int def) { return value > 0 ? value : def; }

}  // namespace

::util::Status BcmPacketioManager::StartRx(
    const GoogleConfig::BcmRxConfig& bcm_rx_config) const {
  // Translate GoogleConfig::BcmRxConfig to BcmSdkInterface::RxConfig.
  BcmSdkInterface::RxConfig sdk_rx_config;
  sdk_rx_config.rx_pool_pkt_count =
      GetWithDefault(bcm_rx_config.rx_pool_pkt_count(), kDefaultRxPoolPktCount);
  sdk_rx_config.rx_pool_bytes_per_pkt = GetWithDefault(
      bcm_rx_config.rx_pool_bytes_per_pkt(), kDefaultRxPoolBytesPerPkt);
  sdk_rx_config.max_pkt_size_bytes = GetWithDefault(
      bcm_rx_config.max_pkt_size_bytes(), kDefaultMaxPktSizeBytes);
  sdk_rx_config.pkts_per_chain =
      GetWithDefault(bcm_rx_config.pkts_per_chain(), kDefaultPktsPerChain);
  sdk_rx_config.max_rate_pps =
      GetWithDefault(bcm_rx_config.max_rate_pps(), kDefaultMaxRatePps);
  sdk_rx_config.max_burst_pkts =
      GetWithDefault(bcm_rx_config.max_burst_pkts(), kDefaultBurstPps);
  sdk_rx_config.use_interrupt = bcm_rx_config.use_interrupt();
  // If bcm_rx_config.dma_channel_configs() is not empty, use the given
  // key values directly. If not, use the default DMA channel config.
  if (!bcm_rx_config.dma_channel_configs().empty()) {
    for (const auto& e : bcm_rx_config.dma_channel_configs()) {
      sdk_rx_config.dma_channel_configs[e.first].chains = e.second.chains();
      sdk_rx_config.dma_channel_configs[e.first].strip_crc =
          e.second.strip_crc();
      sdk_rx_config.dma_channel_configs[e.first].strip_vlan =
          e.second.strip_vlan();
      sdk_rx_config.dma_channel_configs[e.first].oversized_packets_ok =
          e.second.oversized_packets_ok();
      sdk_rx_config.dma_channel_configs[e.first].no_pkt_parsing =
          e.second.no_pkt_parsing();
      for (int c : e.second.cos_set()) {
        CHECK_RETURN_IF_FALSE(c >= 0 && c <= kMaxCos)
            << "Invalid CoS in cos_set: " << bcm_rx_config.ShortDebugString();
        sdk_rx_config.dma_channel_configs[e.first].cos_set.insert(c);
      }
    }
  } else {
    // For the default DMA channel config, all the supported cos are mapped to
    // channel 1.
    sdk_rx_config.dma_channel_configs[kDefaultDmaChannel].chains =
        kDefaultDmaChannelChains;
    for (int c = 0; c <= kMaxCos; ++c) {
      sdk_rx_config.dma_channel_configs[kDefaultDmaChannel].cos_set.insert(c);
    }
  }

  RETURN_IF_ERROR(bcm_sdk_interface_->StartRx(unit_, sdk_rx_config));

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::StartTx(
    const GoogleConfig::BcmTxConfig& bcm_tx_config) const {
  // TODO(unknown): Seems like for KNET there is nothing to configure for TX.
  // Investigate this more.
  return ::util::OkStatus();
}

std::string BcmPacketioManager::GetKnetIntfNameTemplate(
    GoogleConfig::BcmKnetIntfPurpose purpose, int cpu_queue) const {
  return absl::Substitute(kNetifNameTemplate, cpu_queue);
}

::util::Status BcmPacketioManager::SetupKnetIntfs(
    const GoogleConfig::BcmKnetConfig& bcm_knet_config) {
  // If bcm_knet_config has any entry in knet_intf_configs, use that. If not,
  // only configure KNET interface for the default purpose (controller). Note
  // that we do not allow multiple KNET interfaces with the same purpose on a
  // node, because it does not make sense.
  purpose_to_knet_intf_.clear();
  if (bcm_knet_config.knet_intf_configs_size()) {
    std::set<int> cpu_queues = {};
    std::set<GoogleConfig::BcmKnetIntfPurpose> purposes = {};
    for (const auto& knet_intf_config : bcm_knet_config.knet_intf_configs()) {
      CHECK_RETURN_IF_FALSE(knet_intf_config.cpu_queue() > 0 &&
                            knet_intf_config.cpu_queue() <= kMaxCpuQueue)
          << "Invalid KNET CPU queue: " << knet_intf_config.cpu_queue()
          << ", found in " << bcm_knet_config.ShortDebugString();
      CHECK_RETURN_IF_FALSE(!cpu_queues.count(knet_intf_config.cpu_queue()))
          << "Multiple KNET interface configs for CPU queue "
          << knet_intf_config.cpu_queue() << ", found in "
          << bcm_knet_config.ShortDebugString();
      cpu_queues.insert(knet_intf_config.cpu_queue());
      CHECK_RETURN_IF_FALSE(!purposes.count(knet_intf_config.purpose()))
          << "Multiple KNET interface configs for purpose "
          << GoogleConfig::BcmKnetIntfPurpose_Name(knet_intf_config.purpose())
          << ", found in " << bcm_knet_config.ShortDebugString();
      purposes.insert(knet_intf_config.purpose());
      CHECK_RETURN_IF_FALSE(knet_intf_config.mtu() > 0)
          << "Invalid KNET interface MTU: " << knet_intf_config.mtu()
          << ", found in " << bcm_knet_config.ShortDebugString();
      GoogleConfig::BcmKnetIntfPurpose purpose = knet_intf_config.purpose();
      purpose_to_knet_intf_[purpose].cpu_queue = knet_intf_config.cpu_queue();
      purpose_to_knet_intf_[purpose].mtu = knet_intf_config.mtu();
      purpose_to_knet_intf_[purpose].vlan = knet_intf_config.vlan();
      // The name is just a template for the intf name at this point.
      purpose_to_knet_intf_[purpose].netif_name =
          GetKnetIntfNameTemplate(purpose, knet_intf_config.cpu_queue());
    }
  } else {
    GoogleConfig::BcmKnetIntfPurpose purpose =
        GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER;
    purpose_to_knet_intf_[purpose].cpu_queue = kDefaultCpuQueue;
    purpose_to_knet_intf_[purpose].mtu = kDefaultKnetIntfMtu;
    // The name is just a template for the intf name at this point.
    purpose_to_knet_intf_[purpose].netif_name =
        GetKnetIntfNameTemplate(purpose, kDefaultCpuQueue);
  }

  // Now that CPU queues are clear, go ahead and setup the KNET interfaces
  // by calling the SDK and save their ids.
  for (auto& entry : purpose_to_knet_intf_) {
    RETURN_IF_ERROR(SetupSingleKnetIntf(entry.first, &entry.second));
  }

  // Finally after all the KNET intfs are setup, bring up the RX threads.
  // If spawning the thread has some issues we will return error but we will
  // not retry after the next config push. This probably points to a serious
  // system issue unrelated to Stratum.
  for (auto& entry : purpose_to_knet_intf_) {
    KnetIntfRxThreadData* data =
        new KnetIntfRxThreadData(node_id_, entry.first, this);
    knet_intf_rx_thread_data_.push_back(data);
    // TODO(unknown): How about some thread attributes. Do we need any?
    int ret = pthread_create(&entry.second.rx_thread_id, nullptr,
                             &BcmPacketioManager::KnetIntfRxThreadFunc, data);
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to spawn RX thread for KNET interface "
             << entry.second.netif_name << " created for node with ID "
             << node_id_ << " (unit: " << unit_ << ", purpose: "
             << GoogleConfig::BcmKnetIntfPurpose_Name(entry.first)
             << ", vlan: " << entry.second.vlan
             << ", cpu_queue: " << entry.second.cpu_queue
             << ", netif_id: " << entry.second.netif_id
             << ", netif_index: " << entry.second.netif_index
             << ", rx_thread_id: " << entry.second.rx_thread_id
             << "). Err: " << ret << ".";
    }
    LOG(INFO) << "KNET interface " << entry.second.netif_name
              << " created for node with ID " << node_id_ << " (unit: " << unit_
              << ", purpose: "
              << GoogleConfig::BcmKnetIntfPurpose_Name(entry.first)
              << ", vlan: " << entry.second.vlan
              << ", cpu_queue: " << entry.second.cpu_queue
              << ", netif_id: " << entry.second.netif_id
              << ", netif_index: " << entry.second.netif_index
              << ", rx_thread_id: " << entry.second.rx_thread_id << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::SetupSingleKnetIntf(
    GoogleConfig::BcmKnetIntfPurpose purpose, BcmKnetIntf* intf) const {
  if (intf == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null intf!";
  }

  CHECK_RETURN_IF_FALSE(intf->filter_ids.empty())
      << "No KNET filter given for KNET intf (unit " << unit_ << " and purpose "
      << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  // intf->netif_name contains the interface name template. This
  // template will be read and passed to the kernel by SDK. Then
  // intf->netif_name is updated by the value returned by the kernel.
  RETURN_IF_ERROR(bcm_sdk_interface_->CreateKnetIntf(
      unit_, intf->vlan, &intf->netif_name, &intf->netif_id));

  // Create a socket and bind it to the KNET interface. Then, use IOCTL to setup
  // the interface.
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1) {
    return MAKE_ERROR(ERR_INTERNAL) << "Couldn't create socket.";
  }

  // Set interface to UP.
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, intf->netif_name.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't get IFFLAGS for KNET interface " << intf->netif_name
           << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }
  ifr.ifr_flags |= IFF_UP;
  if (ioctl(sock, SIOCSIFFLAGS, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't get IFFLAGS for KNET interface " << intf->netif_name
           << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }

  // Configure MTU.
  // TODO(max): With SDKLT the MTU is set at KNET interface creation.
  // On OpenNSA the MTU is currently configured when loading the KNET Kernel
  // module, but the ioctl() should work. Need to investigate the KNET module
  // source code.
  /*
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, intf->netif_name.c_str(), IFNAMSIZ);
  ifr.ifr_mtu = intf->mtu ? intf->mtu : kDefaultKnetIntfMtu;
  if (ioctl(sock, SIOCSIFMTU, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't set MTU for KNET interface " << intf->netif_name
           << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }
  */

  // Get interface ifindex
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, intf->netif_name.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't get ifindex for KNET interface " << intf->netif_name
           << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }
  intf->netif_index = ifr.ifr_ifindex;

  // Get interface MAC to be used as source MAC for TX.
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, intf->netif_name.c_str(), IFNAMSIZ);
  if (ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
    close(sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't get MAC address from KNET interface "
           << intf->netif_name << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }
  {
    uint8 mac[6];
    memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
    uint64 i = *reinterpret_cast<const uint16*>(&mac[0]);
    i <<= 32;
    intf->smac = (i | *reinterpret_cast<const uint32*>(&mac[2]));
  }

  close(sock);

  // Now setup KNET filters for the interface. The type of the filter depends
  // on the purpose given by the confing (the default purpose being controller).
  std::vector<BcmSdkInterface::KnetFilterType> knet_filter_types = {};
  switch (purpose) {
    case GoogleConfig::BCM_KNET_INTF_PURPOSE_CONTROLLER:
      knet_filter_types.push_back(
          BcmSdkInterface::KnetFilterType::CATCH_ALL);
          // TODO(max): enable later?
          // BcmSdkInterface::KnetFilterType::CATCH_NON_SFLOW_FP_MATCH);
      break;
    case GoogleConfig::BCM_KNET_INTF_PURPOSE_SFLOW:
      knet_filter_types.push_back(
          BcmSdkInterface::KnetFilterType::CATCH_SFLOW_FROM_INGRESS_PORT);
      knet_filter_types.push_back(
          BcmSdkInterface::KnetFilterType::CATCH_SFLOW_FROM_EGRESS_PORT);
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Un-supported KNET interface purpose for unit " << unit_ << ": "
             << GoogleConfig::BcmKnetIntfPurpose_Name(purpose);
  }

  CHECK_RETURN_IF_FALSE(intf->filter_ids.empty());
  for (auto type : knet_filter_types) {
    ASSIGN_OR_RETURN(int filter_id, bcm_sdk_interface_->CreateKnetFilter(
                                        unit_, intf->netif_id, type));
    intf->filter_ids.insert(filter_id);
  }

  // At the last stage, create the socket for this interface for RX/TX. We
  // create 2 separate sockets for TX and RX:
  // - The TX socket is just a simple socket which is not bound to any KNET
  //   interface at this stage. The interface index is used directly in the
  //   message header when we send the packet out.
  // - The RX socket however is configured fully here. We bind it to its KNET
  //   interface, etc.
  intf->tx_sock = socket(AF_PACKET, SOCK_RAW, 0);
  intf->rx_sock = socket(AF_PACKET, SOCK_RAW, 0);
  if (intf->tx_sock == -1 || intf->rx_sock == -1) {
    return MAKE_ERROR(ERR_INTERNAL) << "Couldn't create socket.";
  }

  // Set Berkeley Packet Filter (BPF) for the socket. The filters here are
  // copied directly from Sandcastle. No need to change anything here.
  const struct sock_filter filters[] = {
      // 0. Retrieve "packet type" (see <netpacket/packet.h> for types) from
      //    linux-specific magical negative offset
      {0x28, 0, 0, 0xfffff004},
      // 1. Branch if equal to 4 (PACKET_OUTGOING). Go to 2 if so, 3 otherwise.
      {0x15, 0, 1, 0x00000004},
      // 2. Return 0 (ignore packet)
      {0x6, 0, 0, 0x00000000},
      // 3. Return 65535 (capture entire packet)
      {0x6, 0, 0, 0x0000ffff},
  };
  const struct sock_fprog fprog = {
      sizeof(filters) / sizeof(filters[0]),
      const_cast<struct sock_filter*>(filters),
  };
  if (setsockopt(intf->rx_sock, SOL_SOCKET, SO_ATTACH_FILTER, &fprog,
                 sizeof(fprog)) < 0) {
    close(intf->rx_sock);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't call setsockopt(SO_ATTACH_FILTER) for KNET interface "
           << intf->netif_name << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }

  // Set the RX buffer size (if given by flags).
  if (FLAGS_knet_rx_buf_size > 0) {
    int knet_rx_buf_size = FLAGS_knet_rx_buf_size;
    if (setsockopt(intf->rx_sock, SOL_SOCKET, SO_RCVBUFFORCE, &knet_rx_buf_size,
                   sizeof(knet_rx_buf_size)) < 0) {
      close(intf->rx_sock);
      return MAKE_ERROR(ERR_INTERNAL)
             << "Couldn't call setsockopt(SO_RCVBUFFORCE) for KNET interface "
             << intf->netif_name << " (unit " << unit_ << " and purpose "
             << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
    }
  }

  // Now bind socket to the interface. To bind to the interface, we do not use
  // setsockopt(SO_BINDTODEVICE). Instead we use bind with netif_index.
  struct sockaddr_ll addr;
  memset(&addr, 0, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_protocol = htons(ETH_P_ALL);
  addr.sll_ifindex = intf->netif_index;
  if (bind(intf->rx_sock, reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr)) < 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Couldn't bind the socket for KNET interface " << intf->netif_name
           << " (unit " << unit_ << " and purpose "
           << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::SetRateLimit(
    const GoogleConfig::BcmRateLimitConfig& bcm_rate_limit_config) const {
  // If the config is empty, silently exit. Nothing to do.
  if (bcm_rate_limit_config.max_rate_pps() == 0 &&
      bcm_rate_limit_config.max_burst_pkts() == 0) {
    return ::util::OkStatus();
  }

  // Translate GoogleConfig::BcmRateLimitConfig to
  // BcmSdkInterface::RateLimitConfig.
  BcmSdkInterface::RateLimitConfig sdk_rate_limit_config;
  sdk_rate_limit_config.max_rate_pps = bcm_rate_limit_config.max_rate_pps();
  sdk_rate_limit_config.max_burst_pkts = bcm_rate_limit_config.max_burst_pkts();
  for (const auto& e : bcm_rate_limit_config.per_cos_rate_limit_configs()) {
    sdk_rate_limit_config.per_cos_rate_limit_configs[e.first].max_rate_pps =
        e.second.max_rate_pps();
    sdk_rate_limit_config.per_cos_rate_limit_configs[e.first].max_burst_pkts =
        e.second.max_burst_pkts();
  }

  return bcm_sdk_interface_->SetRateLimit(unit_, sdk_rate_limit_config);
}

::util::StatusOr<BcmKnetIntf*> BcmPacketioManager::GetBcmKnetIntf(
    GoogleConfig::BcmKnetIntfPurpose purpose) {
  BcmKnetIntf* intf = gtl::FindOrNull(purpose_to_knet_intf_, purpose);
  CHECK_RETURN_IF_FALSE(intf != nullptr)
      << "KNET interface with purpose "
      << GoogleConfig::BcmKnetIntfPurpose_Name(purpose)
      << " does not exist for node with ID " << node_id_ << " mapped to unit "
      << unit_ << ".";

  return intf;
}

::util::Status BcmPacketioManager::HandleKnetIntfPacketRx(
    GoogleConfig::BcmKnetIntfPurpose purpose) {
  // Find all data from the BcmKnetIntf this thread cares about. Note that all
  // the RX threads will wait for the config push to be done. After that we do
  // not expect BcmKnetIntf for this purpose to change at all (if it does,
  // VerifyChassisConfig() will return reboot required).
  int rx_sock = -1, netif_index = -1;
  {
    absl::ReaderMutexLock l(&chassis_lock);
    if (shutdown) return ::util::OkStatus();
    ASSIGN_OR_RETURN(const BcmKnetIntf* intf, GetBcmKnetIntf(purpose));
    rx_sock = intf->rx_sock;
    netif_index = intf->netif_index;
    CHECK_RETURN_IF_FALSE(rx_sock > 0)  // MUST NOT HAPPEN!
        << "KNET interface with purpose "
        << GoogleConfig::BcmKnetIntfPurpose_Name(purpose) << " on node with ID "
        << node_id_ << " mapped to unit " << unit_
        << " does not have a RX socket.";
  }

  // Use the newest linux poll mechanism (epoll) to detect whether we have
  // data to read on the socket.
  struct epoll_event event;
  int efd = epoll_create1(0);
  if (efd < 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "epoll_create1() failed. errno: " << errno << ".";
  }
  event.data.fd = rx_sock;  // not even used.
  event.events = EPOLLIN;
  if (epoll_ctl(efd, EPOLL_CTL_ADD, rx_sock, &event) != 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "epoll_ctl() failed. errno: " << errno << ".";
  }
  while (true) {
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    struct epoll_event pevents[1];  // we care about one event at a time.
    int ret = epoll_wait(efd, pevents, 1, FLAGS_knet_rx_poll_timeout_ms);
    VLOG(2) << "RXThread " << GoogleConfig::BcmKnetIntfPurpose_Name(purpose)
        << " epoll_wait() = " << ret;
    if (ret < 0) {
      VLOG(1) << "Error in epoll_wait(). errno: " << errno << ".";
      INCREMENT_RX_COUNTER(purpose, rx_errors_epoll_wait_failures);
      continue;  // let it retry
    } else if (ret > 0 && pevents[0].events & EPOLLIN) {
      // We have data to receive. Try to read max of
      // FLAGS_knet_max_num_packets_to_read_at_once packets before we try to
      // check for exit criteria.
      std::vector<::p4::v1::PacketIn> packets;
      for (int i = 0; i < FLAGS_knet_max_num_packets_to_read_at_once; ++i) {
        absl::ReaderMutexLock l(&chassis_lock);
        if (shutdown) break;
        std::string header = "";
        ::p4::v1::PacketIn packet;
        ASSIGN_OR_RETURN(bool retry,
                         RxPacket(purpose, rx_sock, netif_index, &header,
                                  packet.mutable_payload()));
        if (!retry) break;
        if (!header.empty()) {
          // We received good data. Process it. The parsing errors will not
          // result in RX thread to shutdown.
          int ingress_logical_port = 0, egress_logical_port = 0;
          PacketInMetadata meta;
          ::util::Status status = bcm_sdk_interface_->ParseKnetHeaderForRx(
              unit_, header, &ingress_logical_port, &egress_logical_port,
              &meta.cos);
          if (!status.ok()) {
            VLOG(1) << "Failed to parse KNET header for a packet on unit "
                    << unit_ << ": " << status.error_message();
            INCREMENT_RX_COUNTER(purpose, rx_drops_knet_header_parse_error);
            continue;  // let it retry
          }
          // Find ingress port ID.
          if (ingress_logical_port == kCpuLogicalPort) {
            // This means CPU port by default.
            meta.ingress_port_id = kCpuPortId;
          } else {
            uint32* ingress_port_id =
                gtl::FindOrNull(logical_port_to_port_id_, ingress_logical_port);
            if (ingress_port_id == nullptr) {
              VLOG(1) << "Ingress logical port " << ingress_logical_port
                      << " on unit " << unit_ << " is unknown!";
              INCREMENT_RX_COUNTER(purpose, rx_drops_unknown_ingress_port);
              continue;  // let it retry
            }
            meta.ingress_port_id = *ingress_port_id;
            auto ret = bcm_chassis_ro_interface_->GetParentTrunkId(
                node_id_, *ingress_port_id);
            if (ret.ok()) {
              // If status is OK, there is a parent trunk.
              meta.ingress_trunk_id = ret.ValueOrDie();
            }
          }
          // Find egress port ID.
          if (egress_logical_port == kCpuLogicalPort) {
            // This means CPU port by default.
            meta.egress_port_id = kCpuPortId;
          } else if (egress_logical_port == 1) {
            // SDKLT sets egress port to 1 for packets that do not match
            // MY_STATION table or got dropped by the ASIC?
            // TODO(unknown): check this and decide what to report upwards
            meta.egress_port_id = 1;
          } else {
            uint32* egress_port_id =
                gtl::FindOrNull(logical_port_to_port_id_, egress_logical_port);
            if (egress_port_id == nullptr) {
              VLOG(1) << "Egress logical port " << egress_logical_port
                      << " on unit " << unit_ << " is unknown!";
              INCREMENT_RX_COUNTER(purpose, rx_drops_unknown_egress_port);
              continue;  // let it retry
            }
            meta.egress_port_id = *egress_port_id;
          }
          VLOG(1) << "PacketInMetadata.ingress_port_id: "
                  << meta.ingress_port_id << "\n"
                  << "PacketInMetadata.ingress_trunk_id: "
                  << meta.ingress_trunk_id << "\n"
                  << "PacketInMetadata.egress_port_id: " << meta.egress_port_id
                  << "\n"
                  << "PacketInMetadata.cos: " << meta.cos;
          status = DeparsePacketInMetadata(meta, &packet);
          if (!status.ok()) {
            INCREMENT_RX_COUNTER(purpose, rx_drops_metadata_deparse_error);
            continue;  // let it retry
          }
          INCREMENT_RX_COUNTER(purpose, rx_accepts);
          packets.push_back(packet);
        }
      }
      // Send the packet to the packet RX writer.
      if (!packets.empty()) {
        absl::ReaderMutexLock l(&rx_writer_lock_);
        auto* writer = gtl::FindOrNull(purpose_to_rx_writer_, purpose);
        if (writer != nullptr) {
          for (const auto& p : packets) {
            (*writer)->Write(p);
          }
        }
      }
    }
  }

  close(efd);
  LOG(INFO) << "Killed RX thread for KNET interface with purpose "
            << GoogleConfig::BcmKnetIntfPurpose_Name(purpose)
            << " on node with ID " << node_id_ << " mapped to unit " << unit_
            << ".";

  return ::util::OkStatus();
}

::util::StatusOr<bool> BcmPacketioManager::RxPacket(
    GoogleConfig::BcmKnetIntfPurpose purpose, int sock, int netif_index,
    std::string* header, std::string* payload) {
  if (header == nullptr || payload == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL) << "Null header or payload!";
  }

  header->clear();
  payload->clear();

  constexpr size_t kMaxIovLen = 2;
  struct iovec iov[kMaxIovLen];
  size_t idx = 0;
  size_t header_size = bcm_sdk_interface_->GetKnetHeaderSizeForRx(unit_);
  std::unique_ptr<char[]> header_buffer(new char[header_size]);
  std::unique_ptr<char[]> payload_buffer(new char[kMaxRxBufferSize]);
  memset(header_buffer.get(), 0, header_size);
  memset(payload_buffer.get(), 0, kMaxRxBufferSize);

  iov[idx].iov_base = header_buffer.get();
  iov[idx].iov_len = header_size;
  idx++;

  iov[idx].iov_base = payload_buffer.get();
  iov[idx].iov_len = kMaxRxBufferSize;
  idx++;

  CHECK_RETURN_IF_FALSE(idx <= kMaxIovLen);  // juts in case. Will never happen

  struct sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = idx;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);

  ssize_t res = recvmsg(sock, &msg, MSG_DONTWAIT);
  if (res < 0) {
    switch (errno) {
      case EINTR:
        // Signal received before we could read anything. Need to retry.
        return true;
      case EAGAIN:
        // No data was available. No need to retry before we check for data
        // available again.
        return false;
      default:
        VLOG(1) << "Error when receiving packet on netif  " << netif_index
                << " on unit " << unit_ << ": " << errno;
        INCREMENT_RX_COUNTER(purpose, rx_errors_internal_read_failures);
        // We retry in case of other errors as well.
        return true;
    }
  } else if (res == 0) {
    INCREMENT_RX_COUNTER(purpose, rx_errors_sock_shutdown);
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unexpected socket shutdown on netif  " << netif_index
           << " on unit " << unit_ << ".";
  }

  INCREMENT_RX_COUNTER(purpose, all_rx);
  // res > 0 so cast to size_t is safe.
  if (static_cast<size_t>(res) < header_size) {
    VLOG(1) << "Num of received bytes on netif  " << netif_index << " on unit "
            << unit_ << " < " << header_size << ".";
    INCREMENT_RX_COUNTER(purpose, rx_errors_incomplete_read);
    return true;
  }
  size_t payload_size = static_cast<size_t>(res - header_size);

  // Try to see if the message looks OK. If not retry.
  if (msg.msg_flags & MSG_TRUNC || sa.sll_ifindex != netif_index ||
      sa.sll_pkttype == PACKET_OUTGOING) {
    VLOG(1) << "Received invalid packet on netif  " << netif_index
            << " on unit " << unit_ << ".";
    INCREMENT_RX_COUNTER(purpose, rx_errors_invalid_packet);
    return true;
  }

  // Strip some known VLAN tags.
  struct ether_header* ether_header =
      reinterpret_cast<struct ether_header*>(payload_buffer.get());
  bool tagged = false;
  if (payload_size >= sizeof(struct ether_header) + kVlanIdSize &&
      ntohs(ether_header->ether_type) == ETHERTYPE_VLAN) {
    auto* pid = reinterpret_cast<uint16*>(payload_buffer.get() +
                                          sizeof(struct ether_header));
    uint16 vlan = ntohs(*pid) & kVlanIdMask;
    if (vlan == kDefaultVlan || vlan == kArpVlan || vlan == 0) {
      tagged = true;
    }
  }

  if (tagged) {
    payload->assign(payload_buffer.get(), ETH_ALEN * 2);
    payload->append(payload_buffer.get() + ETH_ALEN * 2 + kVlanTagSize,
                    payload_size - ETH_ALEN * 2 - kVlanTagSize);
  } else {
    payload->assign(payload_buffer.get(), payload_size);
  }
  header->assign(header_buffer.get(), header_size);

  return true;
}

::util::Status BcmPacketioManager::DeparsePacketInMetadata(
    const PacketInMetadata& meta, ::p4::v1::PacketIn* packet) {
  // Note: We are down-casting to uint32 for the port/trunk IDs in this method.
  // This should not cause an issue as controller is already using 32 bit port
  // or trunk IDs.
  if (meta.ingress_port_id > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_INGRESS_PORT);
    mapped_packet_metadata.set_u32(meta.ingress_port_id);
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketInMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  if (meta.ingress_trunk_id > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_INGRESS_TRUNK);
    mapped_packet_metadata.set_u32(meta.ingress_trunk_id);
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketInMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  if (meta.egress_port_id > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
    mapped_packet_metadata.set_u32(meta.egress_port_id);
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketInMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  // TODO(unknown): Controller has not defined any metadata for CoS yet. Enable
  // this after this is done.
  /*
  if (meta.cos > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_COS);
    mapped_packet_metadata.set_u32(static_cast<uint32>(meta.cos));
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketInMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  */

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::DeparsePacketOutMetadata(
    const PacketOutMetadata& meta, ::p4::v1::PacketOut* packet) {
  // Note: We are down-casting to uint32 for the port/trunk IDs in this method.
  // This should not cause an issue as controller is already using 32 bit port
  // or trunk IDs.
  if (meta.egress_trunk_id > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_TRUNK);
    mapped_packet_metadata.set_u32(meta.egress_trunk_id);
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketOutMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  if (meta.egress_port_id > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_EGRESS_PORT);
    mapped_packet_metadata.set_u32(meta.egress_port_id);
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketOutMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  // TODO(unknown): Controller has not defined any metadata for CoS yet. Enable
  // this after this is done.
  /*
  if (meta.cos > 0) {
    MappedPacketMetadata mapped_packet_metadata;
    mapped_packet_metadata.set_type(P4_FIELD_TYPE_COS);
    mapped_packet_metadata.set_u32(static_cast<uint32>(meta.cos));
    RETURN_IF_ERROR(p4_table_mapper_->DeparsePacketInMetadata(
        mapped_packet_metadata, packet->add_metadata()));
  }
  */

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::TxPacket(
    GoogleConfig::BcmKnetIntfPurpose purpose, int sock, int vlan,
    int netif_index, bool direct_tx, const std::string& header,
    const std::string& payload) {
  CHECK_RETURN_IF_FALSE(payload.length() >= sizeof(struct ether_header));

  constexpr size_t kMaxIovLen = 4;
  struct iovec iov[kMaxIovLen];
  size_t idx = 0;       // points to the current iov being filled up
  ssize_t tot_len = 0;  // total packet size to be transmitted
  memset(&iov, 0, sizeof(iov));

  iov[idx].iov_base = const_cast<char*>(header.data());
  iov[idx].iov_len = header.length();
  tot_len += iov[idx].iov_len;
  idx++;
  // Add payload without caring about (missing) VLAN tags.
  iov[idx].iov_base = const_cast<char*>(payload.data());
  iov[idx].iov_len = payload.length();
  tot_len += iov[idx].iov_len;
  idx++;

  CHECK_RETURN_IF_FALSE(idx <= kMaxIovLen);  // just in case. Will never happen

  // Here sa.sll_addr is left zeroed out, matching what's in rcpu_hdr.
  struct sockaddr_ll sa;
  memset(&sa, 0, sizeof(sa));
  sa.sll_family = AF_PACKET;
  sa.sll_ifindex = netif_index;
  sa.sll_halen = ETH_ALEN;

  struct msghdr msg;
  memset(&msg, 0, sizeof(msg));
  msg.msg_iov = iov;
  msg.msg_iovlen = idx;
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);

  while (1) {
    int res = sendmsg(sock, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
    if (res < 0) {
      switch (errno) {
        case EINTR:
          // signal received before we could transmit anything. Need to retry.
          continue;
        default:
          INCREMENT_TX_COUNTER(purpose, tx_errors_internal_send_failures);
          return MAKE_ERROR(ERR_INTERNAL)
                 << "Error when transmitting packet to netif " << netif_index
                 << " on unit " << unit_ << ": " << errno;
      }
    } else if (res != tot_len) {
      INCREMENT_TX_COUNTER(purpose, tx_errors_incomplete_send);
      return MAKE_ERROR(ERR_INTERNAL)
             << "Incomplete packet transmit on netif  " << netif_index
             << " on unit " << unit_ << " (" << res << " != " << tot_len
             << ").";
    }
    break;
  }

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::ParsePacketOutMetadata(
    const ::p4::v1::PacketOut& packet, PacketOutMetadata* meta) {
  meta->cos = kDefaultCos;  // default
  for (const auto& metadata : packet.metadata()) {
    // Query P4TableMapper to understand what this metadata refers to.
    MappedPacketMetadata mapped_packet_metadata;
    RETURN_IF_ERROR(p4_table_mapper_->ParsePacketOutMetadata(
        metadata, &mapped_packet_metadata));
    switch (mapped_packet_metadata.type()) {
      case P4_FIELD_TYPE_EGRESS_PORT:
        meta->egress_port_id = mapped_packet_metadata.u32();
        break;
      case P4_FIELD_TYPE_EGRESS_TRUNK:
        meta->egress_trunk_id = mapped_packet_metadata.u32();
        break;
      case P4_FIELD_TYPE_COS:
        meta->cos = static_cast<int>(mapped_packet_metadata.u32());
        break;
      default:
        VLOG(1) << "Unknown/unsupported meta: " << metadata.ShortDebugString()
                << ".";
        break;
    }
  }
  // If the port/trunk is given we transmit the port directly to the port/trunk.
  // Otherwise, we transmit the packet to ingress pipeline of the given node.
  // TODO(max): This implicit way is in conflict with the explicit flag in
  // packet_out header
  meta->use_ingress_pipeline =
      (meta->egress_port_id == 0 && meta->egress_trunk_id == 0);

  return ::util::OkStatus();
}

::util::Status BcmPacketioManager::ParsePacketInMetadata(
    const ::p4::v1::PacketIn& packet, PacketInMetadata* meta) {
  meta->cos = kDefaultCos;  // default
  for (const auto& metadata : packet.metadata()) {
    // Query P4TableMapper to understand what this metadata refers to.
    MappedPacketMetadata mapped_packet_metadata;
    RETURN_IF_ERROR(p4_table_mapper_->ParsePacketInMetadata(
        metadata, &mapped_packet_metadata));
    switch (mapped_packet_metadata.type()) {
      case P4_FIELD_TYPE_EGRESS_PORT:
        meta->egress_port_id = mapped_packet_metadata.u32();
        break;
      case P4_FIELD_TYPE_INGRESS_PORT:
        meta->ingress_port_id = mapped_packet_metadata.u32();
        break;
      case P4_FIELD_TYPE_COS:
        meta->cos = static_cast<int>(mapped_packet_metadata.u32());
        break;
      default:
        VLOG(1) << "Unknown/unsupported meta: " << metadata.ShortDebugString()
                << ".";
        break;
    }
  }

  return ::util::OkStatus();
}

void* BcmPacketioManager::KnetIntfRxThreadFunc(void* arg) {
  KnetIntfRxThreadData* data = static_cast<KnetIntfRxThreadData*>(arg);
  ::util::Status status = data->mgr->HandleKnetIntfPacketRx(data->purpose);
  if (!status.ok()) {
    LOG(ERROR) << "Non-OK exit of RX thread for KNET interface with purpose "
               << GoogleConfig::BcmKnetIntfPurpose_Name(data->purpose)
               << " on node with ID " << data->node_id << ".";
  }
  return nullptr;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
