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
  // todo: check if an incomplete type could work. answer: no, can't call
  // member functions on incomplete types.
  class SessionInterface {
   public:
    virtual ~SessionInterface() {}

    // Start a new batch.
    virtual ::util::Status BeginBatch() = 0;

    // End the current batch.
    virtual ::util::Status EndBatch() = 0;
  };

  // TableKeyInterface hides the BfRt table key.
  // TODO(max): docs
  class TableKeyInterface {
   public:
    virtual ~TableKeyInterface() {}
    virtual ::util::Status SetExact(int id, const std::string& value) = 0;
    virtual ::util::Status GetExact(int id, std::string* value) const = 0;
    virtual ::util::Status SetTernary(int id, const std::string& value,
                                      const std::string& mask) = 0;
    virtual ::util::Status GetTernary(int id, std::string* value,
                                      std::string* mask) const = 0;
    virtual ::util::Status SetLpm(int id, const std::string& prefix,
                                  uint16 prefix_length) = 0;
    virtual ::util::Status GetLpm(int id, std::string* prefix,
                                  uint16* prefix_length) const = 0;
    virtual ::util::Status SetRange(int id, const std::string& low,
                                    const std::string& high) = 0;
    virtual ::util::Status GetRange(int id, std::string* low,
                                    std::string* high) const = 0;
    virtual ::util::Status SetPriority(uint32 priority) = 0;
    virtual ::util::Status GetPriority(uint32* priority) const = 0;
  };

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

    // Like SetCounterData, but deactivates all other fields. Useful when
    // modifying counter values without touching the action.
    virtual ::util::Status SetOnlyCounterData(uint64 bytes, uint64 packets) = 0;

    // Get the counter values.
    virtual ::util::Status GetCounterData(uint64* bytes,
                                          uint64* packets) const = 0;

    // Get the action ID.
    virtual ::util::Status GetActionId(int* action_id) const = 0;

    // Resets all data fields.
    virtual ::util::Status Reset(int action_id) = 0;
  };


  virtual ~BfSdeInterface() {}

  virtual ::util::Status AddDevice(int device,
                                   const BfrtDeviceConfig& device_config) = 0;

  // Creates a new SDE session.
  virtual ::util::StatusOr<std::shared_ptr<SessionInterface>>
  CreateSession() = 0;

  // Allocates a new table key object.
  virtual ::util::StatusOr<std::unique_ptr<TableKeyInterface>> CreateTableKey(
      int table_id) = 0;

  // Allocates a new table data object. Action id can be zero when not known or
  // not applicable.
  virtual ::util::StatusOr<std::unique_ptr<TableDataInterface>> CreateTableData(
      int table_id, int action_id) = 0;

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
      const std::vector<uint32>& ports) = 0;

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
  // BfRt table ID, not P4Runtime. Timeout specifies the maximum time to wait
  // for the counters to sync.
  // TODO(max): figure out optional counter data API, see TotW#163
  virtual ::util::Status ReadIndirectCounter(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 counter_id, int counter_index, absl::optional<uint64>* byte_count,
      absl::optional<uint64>* packet_count, absl::Duration timeout) = 0;

  // Updates a register at the given index in a table. The table ID must be a
  // BfRt table ID, not P4Runtime. Timeout specifies the maximum time to wait
  // for the registers to sync.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status WriteRegister(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      const std::string& register_data) = 0;

  // Reads the data from a register in a table, or all registers if index is 0.
  // The table ID must be a BfRt table ID, not P4Runtime.
  // TODO(max): figure out optional register index API, see TotW#163
  virtual ::util::Status ReadRegisters(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      std::vector<uint32>* register_indices,
      std::vector<uint64>* register_datas, absl::Duration timeout) = 0;

  // Inserts an action profile member. The table ID must be a BfRt table, not
  // P4Runtime.
  virtual ::util::Status InsertActionProfileMember(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id,
      const TableDataInterface* table_data) = 0;

  // Modifies an existing action profile member. The table ID must be a BfRt
  // table, not P4Runtime.
  virtual ::util::Status ModifyActionProfileMember(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id,
      const TableDataInterface* table_data) = 0;

  // Deletes an action profile member. The table ID must be a BfRt
  // table, not P4Runtime. Returns an error if the member does not exist.
  virtual ::util::Status DeleteActionProfileMember(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id) = 0;

  // Returns the action profile member from the given table, or all
  // members if member ID is 0.
  virtual ::util::Status GetActionProfileMembers(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, std::vector<int>* member_ids,
      std::vector<std::unique_ptr<TableDataInterface>>* table_datas) = 0;

  // Inserts an action profile group. The table ID must be a BfRt table, not
  // P4Runtime.
  virtual ::util::Status InsertActionProfileGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) = 0;

  // Modifies an action profile group. The table ID must be a BfRt table, not
  // P4Runtime.
  virtual ::util::Status ModifyActionProfileGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) = 0;

  // Deletes an action profile group. The table ID must be a BfRt table, not
  // P4Runtime.
  virtual ::util::Status DeleteActionProfileGroup(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id) = 0;

  // Returns the action profile group from the given table, or all
  // groups if member ID is 0.
  virtual ::util::Status GetActionProfileGroups(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, std::vector<int>* group_ids,
      std::vector<int>* max_group_sizes,
      std::vector<std::vector<uint32>>* member_ids,
      std::vector<std::vector<bool>>* member_status) = 0;

  // Inserts a new table entry with the given key and data. Fails if the table
  // entry already exists.
  virtual ::util::Status InsertTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) = 0;

  // Modifies an existing table entry with the given key and data. Fails if the
  // table entry does not exists.
  virtual ::util::Status ModifyTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) = 0;

  // Delets an existing table entry with the given key and data. Fails if the
  // table entry does not exists.
  virtual ::util::Status DeleteTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key) = 0;

  // Fetches an existing table entry for the given key. Fails if the table entry
  // does not exists.
  virtual ::util::Status GetTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      TableDataInterface* table_data) = 0;

  // Fetches all table entries in the given table.
  virtual ::util::Status GetAllTableEntries(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id,
      std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
      std::vector<std::unique_ptr<TableDataInterface>>* table_datas) = 0;

  // Sets the default table entry (action) for a table.
  virtual ::util::Status SetDefaultTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, const TableDataInterface* table_data) = 0;

  // Resets the default table entry (action) of a table.
  virtual ::util::Status ResetDefaultTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id) = 0;

  // Gets the default table entry (action) of a table.
  virtual ::util::Status GetDefaultTableEntry(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, TableDataInterface* table_data) = 0;

  // Synchronizes the driver cached counter values with the current hardware
  // state for a given BfRt table.
  virtual ::util::Status SynchronizeCounters(
      int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 table_id, absl::Duration timeout) = 0;

  // Returns the equivalent BfRt ID for the given P4RT ID.
  virtual ::util::StatusOr<uint32> GetBfRtId(uint32 p4info_id) const = 0;

  // Returns the equivalent P4RT ID for the given BfRt ID.
  virtual ::util::StatusOr<uint32> GetP4InfoId(uint32 bfrt_id) const = 0;

  // Gets the action selector ID of an action profile.
  virtual ::util::StatusOr<uint32> GetActionSelectorBfRtId(
      uint32 action_profile_id) const = 0;

  // Gets the action profile ID of an action selector.
  virtual ::util::StatusOr<uint32> GetActionProfileBfRtId(
      uint32 action_selector_id) const = 0;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BfSdeInterface() {}
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BF_SDE_INTERFACE_H_
