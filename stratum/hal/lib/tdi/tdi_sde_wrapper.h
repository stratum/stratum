// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_SDE_WRAPPER_H_
#define STRATUM_HAL_LIB_TDI_TDI_SDE_WRAPPER_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_id_mapper.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/lib/channel/channel.h"

#ifdef TOFINO_TARGET
#include "pkt_mgr/pkt_mgr_intf.h"
#endif

extern "C" {
// Get the /sys fs file name of the ...
int switch_pci_sysfs_str_get(char* name, size_t name_size);
}

namespace stratum {
namespace hal {
namespace tdi {

class TableKey : public TdiSdeInterface::TableKeyInterface {
 public:
  explicit TableKey(std::unique_ptr<::tdi::TableKey> table_key)
      : table_key_(std::move(table_key)) {}

  // TableKeyInterface public methods.
  ::util::Status SetExact(int id, const std::string& value) override;
  ::util::Status GetExact(int id, std::string* value) const override;
  ::util::Status SetTernary(int id, const std::string& value,
                            const std::string& mask) override;
  ::util::Status GetTernary(int id, std::string* value,
                            std::string* mask) const override;
  ::util::Status SetLpm(int id, const std::string& prefix,
                        uint16 prefix_length) override;
  ::util::Status GetLpm(int id, std::string* prefix,
                        uint16* prefix_length) const override;
  ::util::Status SetRange(int id, const std::string& low,
                          const std::string& high) override;
  ::util::Status GetRange(int id, std::string* low,
                          std::string* high) const override;
  ::util::Status SetPriority(uint64 priority) override;
  ::util::Status GetPriority(uint32* priority) const override;

  // Allocates a new table key object.
  static ::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableKeyInterface>>
  CreateTableKey(const ::tdi::TdiInfo* tdi_info_, int table_id);

  // Stores the underlying SDE object.
  std::unique_ptr<::tdi::TableKey> table_key_;

 private:
  TableKey() {}
};

class TableData : public TdiSdeInterface::TableDataInterface {
 public:
  explicit TableData(std::unique_ptr<::tdi::TableData> table_data)
      : table_data_(std::move(table_data)) {}

  // TableDataInterface public methods.
  ::util::Status SetParam(int id, const std::string& value) override;
  ::util::Status GetParam(int id, std::string* value) const override;
  ::util::Status SetActionMemberId(uint64 action_member_id) override;
  ::util::Status GetActionMemberId(uint64* action_member_id) const override;
  ::util::Status SetSelectorGroupId(uint64 selector_group_id) override;
  ::util::Status GetSelectorGroupId(uint64* selector_group_id) const override;
  ::util::Status SetCounterData(uint64 bytes, uint64 packets) override;
  ::util::Status GetCounterData(uint64* bytes, uint64* packets) const override;
  ::util::Status GetActionId(int* action_id) const override;
  ::util::Status Reset(int action_id) override;

  // Allocates a new table data object.
  static ::util::StatusOr<std::unique_ptr<TdiSdeInterface::TableDataInterface>>
  CreateTableData(const ::tdi::TdiInfo* tdi_info_, int table_id, int action_id);

  // Stores the underlying SDE object.
  std::unique_ptr<::tdi::TableData> table_data_;

 private:
  TableData() {}
};

// The "TdiSdeWrapper" is an implementation of TdiSdeInterface which is used
// on real hardware to talk to the Tofino ASIC.
class TdiSdeWrapper : public TdiSdeInterface {
 public:
  // Default MTU for ports on Tofino.
  static constexpr int32 kBfDefaultMtu = 10 * 1024;  // 10K

  // Wrapper around the TDI session object.
  class Session : public TdiSdeInterface::SessionInterface {
   public:
    // SessionInterface public methods.
    ::util::Status BeginBatch() override {
      RETURN_IF_TDI_ERROR(tdi_session_->beginBatch());
      return ::util::OkStatus();
    }
    ::util::Status EndBatch() override {
      RETURN_IF_TDI_ERROR(tdi_session_->endBatch(/*hardware sync*/ true));
      RETURN_IF_TDI_ERROR(tdi_session_->completeOperations());
      return ::util::OkStatus();
    }

    static ::util::StatusOr<std::shared_ptr<TdiSdeInterface::SessionInterface>>
    CreateSession() {
      std::shared_ptr<::tdi::Session> tdi_session;
      const ::tdi::Device* device = nullptr;
      uint32 dev_id = 0;
      ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
      device->createSession(&tdi_session);

      RET_CHECK(tdi_session) << "Failed to create new session.";

      return std::shared_ptr<TdiSdeInterface::SessionInterface>(
          new Session(tdi_session));
    }

    // Stores the underlying SDE session.
    std::shared_ptr<::tdi::Session> tdi_session_;

   private:
    // Private constructor. Use CreateSession() instead.
    Session() {}
    explicit Session(std::shared_ptr<::tdi::Session> tdi_session)
        : tdi_session_(tdi_session) {}
  };

  // TdiSdeInterface public methods.
  ::util::Status InitializeSde(const std::string& sde_install_path,
                               const std::string& sde_config_file,
                               bool run_in_background) override;
  ::util::Status AddDevice(int device,
                           const TdiDeviceConfig& device_config) override;
  ::util::StatusOr<std::shared_ptr<TdiSdeInterface::SessionInterface>>
  CreateSession() override;
  ::util::StatusOr<std::unique_ptr<TableKeyInterface>> CreateTableKey(
      int table_id) override;
  ::util::StatusOr<std::unique_ptr<TableDataInterface>> CreateTableData(
      int table_id, int action_id) override;
  ::util::StatusOr<PortState> GetPortState(int device, int port) override;
  ::util::Status GetPortCounters(int device, int port,
                                 PortCounters* counters) override;
  ::util::Status RegisterPortStatusEventWriter(
      std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) override
      LOCKS_EXCLUDED(port_status_event_writer_lock_);
  ::util::Status UnregisterPortStatusEventWriter() override
      LOCKS_EXCLUDED(port_status_event_writer_lock_);
  ::util::Status GetPortInfo(int device, int port,
                             TargetDatapathId* target_dp_id) override;
  ::util::Status AddPort(int device, int port, uint64 speed_bps,
                         FecMode fec_mode) override;
  ::util::Status AddPort(int device, int port, uint64 speed_bps,
                         const PortConfigParams& config,
                         FecMode fec_mode) override;
  ::util::Status DeletePort(int device, int port) override;
  ::util::Status EnablePort(int device, int port) override;
  ::util::Status DisablePort(int device, int port) override;
  ::util::Status SetPortMtu(int device, int port, int32 mtu) override;
  bool IsValidPort(int device, int port) override;
  ::util::StatusOr<uint32> GetPortIdFromPortKey(
      int device, const PortKey& port_key) override;
  ::util::StatusOr<int> GetPcieCpuPort(int device) override;
  ::util::Status SetTmCpuPort(int device, int port) override;
  ::util::Status SetDeflectOnDropDestination(int device, int port,
                                             int queue) override;
  ::util::StatusOr<bool> IsSoftwareModel(int device) override;
  std::string GetChipType(int device) const override;
  std::string GetSdeVersion() const override;
  ::util::Status TxPacket(int device, const std::string& packet) override;
  ::util::Status StartPacketIo(int device) override;
  ::util::Status StopPacketIo(int device) override;
  ::util::Status RegisterPacketReceiveWriter(
      int device, std::unique_ptr<ChannelWriter<std::string>> writer) override;
  ::util::Status UnregisterPacketReceiveWriter(int device) override;
  ::util::StatusOr<uint32> CreateMulticastNode(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
      const std::vector<uint32>& ports) override LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<std::vector<uint32>> GetNodesInMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteMulticastNodes(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const std::vector<uint32>& mc_node_ids) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetMulticastNode(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
      std::vector<uint32>* ports) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status InsertMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ModifyMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetMulticastGroups(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, std::vector<uint32>* group_ids,
      std::vector<std::vector<uint32>>* mc_node_ids) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status InsertCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ModifyCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetCloneSessions(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, std::vector<uint32>* session_ids,
      std::vector<int>* egress_ports, std::vector<int>* coss,
      std::vector<int>* max_pkt_lens) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status WriteIndirectCounter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
      absl::optional<uint64> packet_count) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status ReadIndirectCounter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 counter_id, absl::optional<uint32> counter_index,
      std::vector<uint32>* counter_indices,
      std::vector<absl::optional<uint64>>* byte_counts,
      std::vector<absl::optional<uint64>>* packet_counts,
      absl::Duration timeout) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status WriteRegister(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      const std::string& register_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status ReadRegisters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> register_index,
      std::vector<uint32>* register_indices,
      std::vector<uint64>* register_values, absl::Duration timeout) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status WriteIndirectMeter(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> meter_index, bool in_pps,
      uint64 cir, uint64 cburst, uint64 pir, uint64 pburst) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ReadIndirectMeters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::optional<uint32> meter_index,
      std::vector<uint32>* meter_indices, std::vector<uint64>* cirs,
      std::vector<uint64>* cbursts, std::vector<uint64>* pirs,
      std::vector<uint64>* pbursts, std::vector<bool>* in_pps) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status InsertActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id,
      const TableDataInterface* table_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status ModifyActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id,
      const TableDataInterface* table_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetActionProfileMembers(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, std::vector<int>* member_ids,
      std::vector<std::unique_ptr<TableDataInterface>>* table_values) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status InsertActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ModifyActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetActionProfileGroups(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, std::vector<int>* group_ids,
      std::vector<int>* max_group_sizes,
      std::vector<std::vector<uint32>>* member_ids,
      std::vector<std::vector<bool>>* member_status) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status SynchronizeCounters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::Duration timeout) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status InsertTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status ModifyTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      const TableDataInterface* table_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status DeleteTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableKeyInterface* table_key,
      TableDataInterface* table_data) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetAllTableEntries(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id,
      std::vector<std::unique_ptr<TableKeyInterface>>* table_keys,
      std::vector<std::unique_ptr<TableDataInterface>>* table_values) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status SetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, const TableDataInterface* table_data) override
      LOCKS_EXCLUDED(data_lock_);
  ::util::Status ResetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id) override LOCKS_EXCLUDED(data_lock_);
  ::util::Status GetDefaultTableEntry(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, TableDataInterface* table_data) override
      LOCKS_EXCLUDED(data_lock_);

  ::util::StatusOr<uint32> GetTdiRtId(uint32 p4info_id) const override
      LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<uint32> GetP4InfoId(uint32 tdi_id) const override
      LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<uint32> GetActionSelectorTdiRtId(
      uint32 action_profile_id) const override LOCKS_EXCLUDED(data_lock_);
  ::util::StatusOr<uint32> GetActionProfileTdiRtId(
      uint32 action_selector_id) const override LOCKS_EXCLUDED(data_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static TdiSdeWrapper* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  // The following public functions are specific to this class. They are to be
  // called by SDE callbacks only.

  // Return the singleton instance to be used in the SDE callbacks.
  static TdiSdeWrapper* GetSingleton() LOCKS_EXCLUDED(init_lock_);

#ifdef TOFINO_TARGET
  // Writes a received packet to the registered Rx writer. Called from the SDE
  // callback function.
  ::util::Status HandlePacketRx(bf_dev_id_t device, bf_pkt* pkt,
                                bf_pkt_rx_ring_t rx_ring)
      LOCKS_EXCLUDED(packet_rx_callback_lock_);
#endif

  // Called whenever a port status event is received from SDK. It forwards the
  // port status event to the module who registered a callback by calling
  // RegisterPortStatusEventWriter().
  ::util::Status OnPortStatusEvent(int device, int dev_port, bool up,
                                   absl::Time timestamp)
      LOCKS_EXCLUDED(port_status_event_writer_lock_);

  // TdiSdeWrapper is neither copyable nor movable.
  TdiSdeWrapper(const TdiSdeWrapper&) = delete;
  TdiSdeWrapper& operator=(const TdiSdeWrapper&) = delete;
  TdiSdeWrapper(TdiSdeWrapper&&) = delete;
  TdiSdeWrapper& operator=(TdiSdeWrapper&&) = delete;

 protected:
  // RW mutex lock for protecting the singleton instance initialization and
  // reading it back from other threads. Unlike other singleton classes, we
  // use RW lock as we need the pointer to class to be returned.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static TdiSdeWrapper* singleton_ GUARDED_BY(init_lock_);

 private:
  // Timeout for Write() operations on port status events.
  static constexpr absl::Duration kWriteTimeout = absl::InfiniteDuration();
  static constexpr int MAX_PORT_HDL_STRING_LEN = 100;
  static constexpr int _PI_UPDATE_MAX_NAME_SIZE = 100;

  // Private constructor, use CreateSingleton and GetSingleton().
  TdiSdeWrapper();

  // RM Mutex to protect the port status writer.
  mutable absl::Mutex port_status_event_writer_lock_;

  // Mutex protecting the packet rx writer map.
  mutable absl::Mutex packet_rx_callback_lock_;

  // RW mutex lock for protecting the pipeline state.
  mutable absl::Mutex data_lock_;

#ifdef TOFINO_TARGET
  // Callback registed with the SDE for Tx notifications.
  static bf_status_t BfPktTxNotifyCallback(bf_dev_id_t device,
                                           bf_pkt_tx_ring_t tx_ring,
                                           uint64 tx_cookie, uint32 status);

  // Callback registed with the SDE for Rx notifications.
  static bf_status_t BfPktRxNotifyCallback(bf_dev_id_t device, bf_pkt* pkt,
                                           void* cookie,
                                           bf_pkt_rx_ring_t rx_ring);
#endif

  // Common code for multicast group handling.
  ::util::Status WriteMulticastGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 group_id, const std::vector<uint32>& mc_node_ids, bool insert)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Common code for clone session handling.
  ::util::Status WriteCloneSession(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 session_id, int egress_port, int cos, int max_pkt_len, bool insert)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Common code for action profile member handling.
  ::util::Status WriteActionProfileMember(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int member_id, const TableDataInterface* table_data,
      bool insert) SHARED_LOCKS_REQUIRED(data_lock_);

  // Common code for action profile group handling.
  ::util::Status WriteActionProfileGroup(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, int group_id, int max_group_size,
      const std::vector<uint32>& member_ids,
      const std::vector<bool>& member_status, bool insert)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Helper function to find, but not allocate, at free multicast node id.
  // This function is not optimized for speed yet.
  ::util::StatusOr<uint32> GetFreeMulticastNodeId(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Helper to dump the entire PRE table state for debugging. Only runs at v=2.
  ::util::Status DumpPreState(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Synchronizes the driver cached register values with the current hardware
  // state for a given TDI table.
  // TODO(max): consolidate with SynchronizeCounters
  ::util::Status SynchronizeRegisters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::Duration timeout)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Internal version SynchronizeCounters without locks.
  // TODO(max): consolidate with SynchronizeRegisters
  ::util::Status DoSynchronizeCounters(
      int device, std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 table_id, absl::Duration timeout)
      SHARED_LOCKS_REQUIRED(data_lock_);

  // Writer to forward the port status change message to. It is registered
  // by chassis manager to receive SDE port status change events.
  std::unique_ptr<ChannelWriter<PortStatusEvent>> port_status_event_writer_
      GUARDED_BY(port_status_event_writer_lock_);

  // Map from device ID to packet receive writer.
  absl::flat_hash_map<int, std::unique_ptr<ChannelWriter<std::string>>>
      device_to_packet_rx_writer_ GUARDED_BY(packet_rx_callback_lock_);

  // TODO(max): make the following maps to handle multiple devices.
  // Pointer to the ID mapper. Not owned by this class.
  std::unique_ptr<TdiIdMapper> tdi_id_mapper_ GUARDED_BY(data_lock_);

  // Pointer to the current BfR info object. Not owned by this class.
  const ::tdi::TdiInfo* tdi_info_ GUARDED_BY(data_lock_);
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_SDE_WRAPPER_H_
