// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_SDE_INTERFACE_H_
#define STRATUM_HAL_LIB_TDI_TDI_SDE_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace tdi {

// The "TdiSdeInterface" class in HAL implements a shim layer around the TDI
// SDE. It is defined as an abstract class to allow multiple implementations:
// 1- TdiSdeWrapper: The real implementation which includes all the SDE API
//    calls.
// 2- TdiSdeMock: Mock class used for unit testing.
class TdiSdeInterface {
 public:
  // PortStatusEvent encapsulates the information received on a port status
  // event. Port refers to the SDE internal device port ID.
  struct PortStatusEvent {
    int device;
    int port;
    PortState state;
    absl::Time time_last_changed;
  };

  struct HotplugConfigParams {
    uint32 qemu_socket_port;
    uint64 qemu_vm_mac_address;
    std::string qemu_socket_ip;
    std::string qemu_vm_netdev_id;
    std::string qemu_vm_chardev_id;
    std::string qemu_vm_device_id;
    std::string native_socket_path;
    QemuHotplugMode qemu_hotplug_mode;
  };

  struct PortConfigParams {
    DpdkPortType port_type;
    DpdkDeviceType device_type;
    PacketDirection packet_dir;
    int queues;
    int mtu;
    std::string socket_path;
    std::string host_name;
    std::string port_name;
    std::string pipeline_name;
    std::string mempool_name;
    std::string pci_bdf;
    struct HotplugConfigParams hotplug_config;
  };

  // SessionInterface is a proxy class for TDI sessions. Most API calls require
  // an active session. It also allows batching requests for performance.
  class SessionInterface {
   public:
    virtual ~SessionInterface() {}

    // Start a new batch.
    virtual ::util::Status BeginBatch() = 0;

    // End the current batch.
    virtual ::util::Status EndBatch() = 0;
  };

  // TableKeyInterface is a proxy class for TDI table keys.
  class TableKeyInterface {
   public:
    virtual ~TableKeyInterface() {}

    // Sets an exact match key field.
    virtual ::util::Status SetExact(int id, const std::string& value) = 0;

    // Gets an exact match key field.
    virtual ::util::Status GetExact(int id, std::string* value) const = 0;

    // Sets a ternary match key field.
    virtual ::util::Status SetTernary(int id, const std::string& value,
                                      const std::string& mask) = 0;

    // Gets a ternary match key field.
    virtual ::util::Status GetTernary(int id, std::string* value,
                                      std::string* mask) const = 0;

    // Sets a LPM match key field.
    virtual ::util::Status SetLpm(int id, const std::string& prefix,
                                  uint16 prefix_length) = 0;

    // Gets a LPM match key field.
    virtual ::util::Status GetLpm(int id, std::string* prefix,
                                  uint16* prefix_length) const = 0;

    // Sets a range match key field.
    virtual ::util::Status SetRange(int id, const std::string& low,
                                    const std::string& high) = 0;

    // Gets a LPM match key field.
    virtual ::util::Status GetRange(int id, std::string* low,
                                    std::string* high) const = 0;

    // Sets the priority of this table key. 0 is the highest priority.
    virtual ::util::Status SetPriority(uint64 priority) = 0;

    // Gets the priority of this table key. 0 is the highest priority.
    virtual ::util::Status GetPriority(uint32* priority) const = 0;
  };

  // TableKeyInterface is a proxy class for TDI table data.
  class TableDataInterface {
   public:
    virtual ~TableDataInterface() {}

    // Sets a table data action parameter.
    virtual ::util::Status SetParam(int id, const std::string& value) = 0;

    // Get a table data action parameter.
    virtual ::util::Status GetParam(int id, std::string* value) const = 0;

    // Sets the $ACTION_MEMBER_ID field.
    virtual ::util::Status SetActionMemberId(uint64 action_member_id) = 0;

    // Gets the $ACTION_MEMBER_ID field.
    virtual ::util::Status GetActionMemberId(
        uint64* action_member_id) const = 0;

    // Sets the $SELECTOR_GROUP_ID field.
    virtual ::util::Status SetSelectorGroupId(uint64 selector_group_id) = 0;

    // Gets the $SELECTOR_GROUP_ID field.
    virtual ::util::Status GetSelectorGroupId(
        uint64* selector_group_id) const = 0;

    // Convenience function to update the counter values in the table data.
    // This hides the IDs for the $COUNTER_SPEC_BYTES fields.
    virtual ::util::Status SetCounterData(uint64 bytes, uint64 packets) = 0;

    // Get the counter values.
    virtual ::util::Status GetCounterData(uint64* bytes,
                                          uint64* packets) const = 0;

    // Get the action ID.
    virtual ::util::Status GetActionId(int* action_id) const = 0;

    // Resets all data fields.
    virtual ::util::Status Reset(int action_id) = 0;
  };

  virtual ~TdiSdeInterface() {}

  // Initializes the SDE. Must be called before any other methods.
  virtual ::util::Status InitializeSde(const std::string& sde_install_path,
                                       const std::string& sde_config_file,
                                       bool run_in_background) = 0;

  // Add and initialize a device. The device config (pipeline) will be loaded
  // into the ASIC. Can be used to re-initialize an existing device.
  virtual ::util::Status AddDevice(int device,
                                   const TdiDeviceConfig& device_config) = 0;

  // Creates a new TDI session.
  virtual ::util::StatusOr<std::shared_ptr<SessionInterface>>
  CreateSession() = 0;

  // Allocates a new table key object.
  virtual ::util::StatusOr<std::unique_ptr<TableKeyInterface>> CreateTableKey(
      int table_id) = 0;

  // Allocates a new table data object. Action id can be zero when not known or
  // not applicable.
  virtual ::util::StatusOr<std::unique_ptr<TableDataInterface>> CreateTableData(
      int table_id, int action_id) = 0;

  // Registers a writer through which to send any port status events. The
  // message contains a tuple (device, port, state), where port refers to the
  // Barefoot SDE device port. There can only be one writer.
  virtual ::util::Status RegisterPortStatusEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) = 0;

  // Unregisters the port status writer.
  virtual ::util::Status UnregisterPortStatusEventWriter() = 0;

  // Get Port Info
  virtual ::util::Status GetPortInfo(int device, int port,
                                     TargetDatapathId *target_dp_id) = 0;

  // Add a new port with the given parameters.
  virtual ::util::Status AddPort(int device, int port, uint64 speed_bps,
                                 FecMode fec_mode = FEC_MODE_UNKNOWN) = 0;

  // Add a new port with the given parameters.
  virtual ::util::Status AddPort(int device, int port, uint64 speed_bps,
                                 const PortConfigParams& config,
                                 FecMode fec_mode = FEC_MODE_UNKNOWN) = 0 ;

  // Hotplug add/delete the port
  virtual ::util::Status HotplugPort(int device, int port,
                            HotplugConfigParams& hotplug_config) = 0;

  // Delete a port.
  virtual ::util::Status DeletePort(int device, int port) = 0;

  // Enable a port.
  virtual ::util::Status EnablePort(int device, int port) = 0;

  // Disable a port.
  virtual ::util::Status DisablePort(int device, int port) = 0;

  // Set the port shaping properties on a port.
  // If is_in_pps is true, the burst size and rate are measured in packets and
  // pps. Else, they're in bytes and bps.
  virtual ::util::Status SetPortShapingRate(int device, int port,
                                            bool is_in_pps, uint32 burst_size,
                                            uint64 rate_per_second) = 0;

  // Enable port shaping on a port.
  virtual ::util::Status EnablePortShaping(int device, int port,
                                           TriState enable) = 0;

  // Get the operational state of a port.
  virtual ::util::StatusOr<PortState> GetPortState(int device, int port) = 0;

  // Get the port counters of a port.
  virtual ::util::Status GetPortCounters(int device, int port,
                                         PortCounters* counters) = 0;

  // Set the auto negotiation policy on a port.
  virtual ::util::Status SetPortAutonegPolicy(int device, int port,
                                              TriState autoneg) = 0;

  // Set the MTU on a port.
  virtual ::util::Status SetPortMtu(int device, int port, int32 mtu) = 0;

  // Checks if a port is valid.
  virtual bool IsValidPort(int device, int port) = 0;

  // Set the given port into the specified loopback mode.
  virtual ::util::Status SetPortLoopbackMode(int device, int port,
                                             LoopbackState loopback_mode) = 0;

  // Returns the SDE device port ID for the given PortKey.
  virtual ::util::StatusOr<uint32> GetPortIdFromPortKey(
      int device, const PortKey& port_key) = 0;

  // Get the CPU port of a device.
  virtual ::util::StatusOr<int> GetPcieCpuPort(int device) = 0;

  // Set the CPU port in the traffic manager.
  virtual ::util::Status SetTmCpuPort(int device, int port) = 0;

  // Sets the (port, queue) deflect destination for dropped packets.
  virtual ::util::Status SetDeflectOnDropDestination(int device, int port,
                                                     int queue) = 0;

  // Check whether we are running on the software model.
  virtual ::util::StatusOr<bool> IsSoftwareModel(int device) = 0;

  // Return the chip type as a string.
  virtual std::string GetChipType(int device) const = 0;

  // Return the SDE version string.
  virtual std::string GetSdeVersion() const = 0;

  // Send a packet to the PCIe CPU port.
  virtual ::util::Status TxPacket(int device, const std::string& packet) = 0;

  // Setup PacketIO to transmit and receive packets from the CPU port.
  virtual ::util::Status StartPacketIo(int device) = 0;

  // Undo the PacketIO setup. No further packets can be sent or received.
  virtual ::util::Status StopPacketIo(int device) = 0;

  // Registers a writer to be invoked when we receive a packet on the PCIe CPU
  // port. There can only be one writer per device.
  virtual ::util::Status RegisterPacketReceiveWriter(
      int device, std::unique_ptr<ChannelWriter<std::string>> writer) = 0;

  // Unregisters the writer registered to this device by
  // RegisterPacketReceiveWriter().
  virtual ::util::Status UnregisterPacketReceiveWriter(int device) = 0;

  // Create a new multicast node with the given parameters. Returns the newly
  // allocated node id.
  virtual ::util::StatusOr<uint32> CreateMulticastNode(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
      const std::vector<uint32>& ports) = 0;

  // Returns the node IDs linked to the given multicast group ID.
  // TODO(max): rename to GetMulticastNodeIdsInMulticastGroup
  virtual ::util::StatusOr<std::vector<uint32>> GetNodesInMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id) = 0;

  // Delete the given multicast nodes.
  virtual ::util::Status DeleteMulticastNodes(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const std::vector<uint32>& mc_node_ids) = 0;

  // Returns the multicast node with the given ID ($pre.node table).
  virtual ::util::Status GetMulticastNode(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
      std::vector<uint32>* ports) = 0;

  // Inserts a multicast group ($pre.mgid table).
  virtual ::util::Status InsertMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) = 0;

  // Modifies a multicast group ($pre.mgid table).
  virtual ::util::Status ModifyMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) = 0;

  // Deletes a multicast group ($pre.mgid table).
  virtual ::util::Status DeleteMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id) = 0;

  // Returns the multicast group with the given ID ($pre.mgid table), or all
  // groups if ID is 0.
  virtual ::util::Status GetMulticastGroups(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, std::vector<uint32>* group_ids,
      std::vector<std::vector<uint32>>* mc_node_ids) = 0;

  // Inserts a clone session ($mirror.cfg table).
  virtual ::util::Status InsertCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) = 0;

  // Modifies a clone session ($mirror.cfg table).
  virtual ::util::Status ModifyCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) = 0;

  // Deletes a clone session ($mirror.cfg table).
  virtual ::util::Status DeleteCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id) = 0;

  // Returns the clone session with the given ID ($mirror.cfg table), or all
  // sessions if ID is 0.
  virtual ::util::Status GetCloneSessions(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, std::vector<uint32>* session_ids,
      std::vector<int>* egress_ports, std::vector<int>* coss,
      std::vector<int>* max_pkt_lens) = 0;

  // Updates an indirect counter at the given index. The counter ID must be a
  // TDI table ID, not P4Runtime.
  // TODO(max): figure out optional counter data API, see TotW#163
  virtual ::util::Status WriteIndirectCounter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
      absl::optional<uint64> packet_count) = 0;

  // Reads the data from an indirect counter. The counter ID must be a
  // TDI table ID, not P4Runtime. Timeout specifies the maximum time to wait
  // for the counters to sync.
  // TODO(max): figure out optional counter data API, see TotW#163
  virtual ::util::Status ReadIndirectCounter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 counter_id, absl::optional<uint32> counter_index,
      std::vector<uint32>* counter_indices,
      std::vector<absl::optional<uint64>>* byte_counts,
      std::vector<absl::optional<uint64>>* packet_counts,
      absl::Duration timeout) = 0;

  // Updates a register at the given index in a table. The table ID must be a
  // TDI table ID, not P4Runtime. Timeout specifies the maximum time to wait
  // for the registers to sync.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status WriteRegister(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      const std::string& register_data) = 0;

  // Reads the data from a register in a table, or all registers if index is 0.
  // The table ID must be a TDI table ID, not P4Runtime.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status ReadRegisters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      std::vector<uint32>* register_indices,
      std::vector<uint64>* register_values, absl::Duration timeout) = 0;

  // Updates an indirect meter at the given index. The table ID must be a
  // TDI table ID, not P4Runtime.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status WriteIndirectMeter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> meter_index, bool in_pps,
      uint64 cir, uint64 cburst, uint64 pir, uint64 pburst) = 0;

  // Reads the data from an indirect meter, or all meters if index is 0.
  // The table ID must be a TDI table ID, not P4Runtime.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status ReadIndirectMeters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> meter_index,
      std::vector<uint32>* meter_indices, std::vector<uint64>* cirs,
      std::vector<uint64>* cbursts, std::vector<uint64>* pirs,
      std::vector<uint64>* pbursts, std::vector<bool>* in_pps) = 0;

  // Inserts an action profile member. The table ID must be a TDI table, not
  // P4Runtime.
  virtual ::util::Status InsertActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, const TableDataInterface* table_data) = 0;

  // Modifies an existing action profile member. The table ID must be a TDI
  // table, not P4Runtime.
  virtual ::util::Status ModifyActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, const TableDataInterface* table_data) = 0;

  // Deletes an action profile member. The table ID must be a TDI
  // table, not P4Runtime. Returns an error if the member does not exist.
  virtual ::util::Status DeleteActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id) = 0;

  // Returns the action profile member from the given table, or all
  // members if member ID is 0.
  virtual ::util::Status GetActionProfileMembers(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, std::vector<int>* member_ids,
      std::vector<std::unique_ptr<TableDataInterface>>* table_values) = 0;

  // Inserts an action profile group. The table ID must be a TDI table, not
  // P4Runtime.
  virtual ::util::Status InsertActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) = 0;

  // Modifies an action profile group. The table ID must be a TDI table, not
  // P4Runtime.
  virtual ::util::Status ModifyActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) = 0;

  // Deletes an action profile group. The table ID must be a TDI table, not
  // P4Runtime.
  virtual ::util::Status DeleteActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id) = 0;

  // Returns the action profile group from the given table, or all
  // groups if member ID is 0.
  virtual ::util::Status GetActionProfileGroups(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, std::vector<int>* group_ids,
      std::vector<int>* max_group_sizes,
      std::vector<std::vector<uint32>>* member_ids,
      std::vector<std::vector<bool>>* member_status) = 0;

  // Inserts a new table entry with the given key and data. Fails if the table
  // entry already exists.
  virtual ::util::Status InsertTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) = 0;

  // Modifies an existing table entry with the given key and data. Fails if the
  // table entry does not exists.
  virtual ::util::Status ModifyTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) = 0;

  // Delets an existing table entry with the given key and data. Fails if the
  // table entry does not exists.
  virtual ::util::Status DeleteTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key) = 0;

  // Fetches an existing table entry for the given key. Fails if the table entry
  // does not exists.
  virtual ::util::Status GetTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      TableDataInterface* table_data) = 0;

  // Fetches all table entries in the given table.
  virtual ::util::Status GetAllTableEntries(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id,
      std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
      std::vector<std::unique_ptr<TableDataInterface>>* table_values) = 0;

  // Sets the default table entry (action) for a table.
  virtual ::util::Status SetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableDataInterface* table_data) = 0;

  // Resets the default table entry (action) of a table.
  virtual ::util::Status ResetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id) = 0;

  // Gets the default table entry (action) of a table.
  virtual ::util::Status GetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, TableDataInterface* table_data) = 0;

  // Synchronizes the driver cached counter values with the current hardware
  // state for a given TDI table.
  virtual ::util::Status SynchronizeCounters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::Duration timeout) = 0;

  // Returns the equivalent TDI ID for the given P4RT ID.
  virtual ::util::StatusOr<uint32> GetTdiRtId(uint32 p4info_id) const = 0;

  // Returns the equivalent P4RT ID for the given TDI ID.
  virtual ::util::StatusOr<uint32> GetP4InfoId(uint32 bfrt_id) const = 0;

  // Gets the action selector ID of an action profile.
  virtual ::util::StatusOr<uint32> GetActionSelectorTdiRtId(
      uint32 action_profile_id) const = 0;

  // Gets the action profile ID of an action selector.
  virtual ::util::StatusOr<uint32> GetActionProfileTdiRtId(
      uint32 action_selector_id) const = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  TdiSdeInterface() {}
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_SDE_INTERFACE_H_
