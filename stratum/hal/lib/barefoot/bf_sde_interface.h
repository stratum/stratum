// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_
#define STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_

#include <memory>

#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace barefoot {

// TODO(max): docs
class BfSdeInterface {
 public:
  // PortStatusEvent encapsulates the information received on a port status
  // event. Port refers to the SDE internal device port ID.
  struct PortStatusEvent {
    int device;
    int port;
    PortState state;
  };

  // SessionInterface allows starting sessions to batch requests.
  // todo: check if an incomplete type could work
  class SessionInterface {
   public:
    virtual ~SessionInterface() {}

    // Start a new batch.
    virtual ::util::Status BeginBatch() = 0;

    // End the current batch.
    virtual ::util::Status EndBatch() = 0;
  };

  virtual ~BfSdeInterface() {}

  virtual ::util::Status AddDevice(int device,
                                   const BfrtDeviceConfig& device_config) = 0;

  virtual ::util::StatusOr<std::shared_ptr<SessionInterface>>
  CreateSession() = 0;

  virtual ::util::StatusOr<PortState> GetPortState(int device, int port) = 0;

  virtual ::util::Status GetPortCounters(int device, int port,
                                         PortCounters* counters) = 0;

  virtual ::util::Status RegisterPortStatusEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) = 0;

  virtual ::util::Status UnregisterPortStatusEventWriter() = 0;

  virtual ::util::Status AddPort(int device, int port, uint64 speed_bps,
                                 FecMode fec_mode = FEC_MODE_UNKNOWN) = 0;

  virtual ::util::Status DeletePort(int device, int port) = 0;

  virtual ::util::Status EnablePort(int device, int port) = 0;

  virtual ::util::Status DisablePort(int device, int port) = 0;

  virtual ::util::Status SetPortAutonegPolicy(int device, int port,
                                              TriState autoneg) = 0;

  virtual ::util::Status SetPortMtu(int device, int port, int32 mtu) = 0;

  virtual bool IsValidPort(int device, int port) = 0;

  virtual ::util::Status SetPortLoopbackMode(int device, int port,
                                             LoopbackState loopback_mode) = 0;

  virtual ::util::StatusOr<uint32> GetPortIdFromPortKey(
      int device, const PortKey& port_key) = 0;

  // Get the CPU port of a device.
  virtual ::util::StatusOr<int> GetPcieCpuPort(int device) = 0;

  // Set the CPU port in the traffic manager.
  virtual ::util::Status SetTmCpuPort(int device, int port) = 0;

  // Check whether we are running on the software model.
  virtual ::util::StatusOr<bool> IsSoftwareModel(int device) = 0;

  virtual ::util::Status TxPacket(int device, const std::string& packet) = 0;

  virtual ::util::Status StartPacketIo(int device) = 0;

  virtual ::util::Status StopPacketIo(int device) = 0;

  virtual ::util::Status RegisterPacketReceiveWriter(
      int device, std::unique_ptr<ChannelWriter<std::string>> writer) = 0;

  virtual ::util::Status UnregisterPacketReceiveWriter(int device) = 0;

  // Create a new multicast node with the given parameters. Returns the newly
  // allocated node id.
  virtual ::util::StatusOr<uint32> CreateMulticastNode(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
      const std::vector<uint32> ports) = 0;

  // Returns the node IDs linked to the given multicast group ID.
  // TODO(max): rename to GetMulticastNodeIdsInMulticastGroup
  virtual ::util::StatusOr<std::vector<uint32>> GetNodesInMulticastGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 group_id) = 0;

  // Delete the given multicast nodes.
  virtual ::util::Status DeleteMulticastNodes(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const std::vector<uint32>& mc_node_ids) = 0;

  // Returns the multicast node with the given ID ($pre.node table).
  virtual ::util::Status GetMulticastNode(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
      std::vector<uint32>* ports) = 0;

  // Inserts a multicast group ($pre.mgid table).
  virtual ::util::Status InsertMulticastGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) = 0;

  // Modifies a multicast group ($pre.mgid table).
  virtual ::util::Status ModifyMulticastGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) = 0;

  // Deletes a multicast group ($pre.mgid table).
  virtual ::util::Status DeleteMulticastGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 group_id) = 0;

  // Returns the multicast group with the given ID ($pre.mgid table), or all
  // groups if ID is 0.
  virtual ::util::Status GetMulticastGroups(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 group_id, std::vector<uint32>* group_ids,
      std::vector<std::vector<uint32>>* mc_node_ids) = 0;

  // Inserts a clone session ($mirror.cfg table).
  virtual ::util::Status InsertCloneSession(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) = 0;

  // Modifies a clone session ($mirror.cfg table).
  virtual ::util::Status ModifyCloneSession(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) = 0;

  // Deletes a clone session ($mirror.cfg table).
  virtual ::util::Status DeleteCloneSession(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 session_id) = 0;

  // Returns the clone session with the given ID ($mirror.cfg table), or all
  // sessions if ID is 0.
  virtual ::util::Status GetCloneSessions(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 session_id, std::vector<uint32>* session_ids,
      std::vector<int>* egress_ports, std::vector<int>* coss,
      std::vector<int>* max_pkt_lens) = 0;

  // Updates an indirect counter at the given index. The counter ID must be a
  // BfRt table ID, not P4Runtime.
  // TODO(max): figure out optional counter data API, see TotW#163
  virtual ::util::Status WriteIndirectCounter(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
      absl::optional<uint64> packet_count) = 0;

  // Reads the data from an indirect counter. The counter ID must be a
  // BfRt table ID, not P4Runtime.
  // TODO(max): figure out optional counter data API, see TotW#163
  virtual ::util::Status ReadIndirectCounter(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 counter_id, int counter_index, absl::optional<uint64>* byte_count,
      absl::optional<uint64>* packet_count, absl::Duration timeout) = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BfSdeInterface() {}
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_
