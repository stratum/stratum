// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_TABLE_MANAGER_H_
#define STRATUM_HAL_LIB_TDI_TDI_TABLE_MANAGER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/lib/timer_daemon.h"

namespace stratum {
namespace hal {
namespace tdi {

class TdiTableManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushForwardingPipelineConfig(const TdiDeviceConfig& config)
      LOCKS_EXCLUDED(lock_);

  // Verifies a P4-based forwarding pipeline configuration intended for this
  // manager.
  ::util::Status VerifyForwardingPipelineConfig(
      const ::p4::v1::ForwardingPipelineConfig& config) const
      LOCKS_EXCLUDED(lock_);

  // Writes a table entry.
  ::util::Status WriteTableEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::TableEntry& table_entry) LOCKS_EXCLUDED(lock_);

  // Reads the P4 TableEntry(s) matched by the given table entry.
  ::util::Status ReadTableEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Modify the counter data of a table entry.
  ::util::Status WriteDirectCounterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::DirectCounterEntry& direct_counter_entry)
      LOCKS_EXCLUDED(lock_);

  // Modify the data of a register entry.
  ::util::Status WriteRegisterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::RegisterEntry& register_entry) LOCKS_EXCLUDED(lock_);

  // Modify the data of a meter entry.
  ::util::Status WriteMeterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::MeterEntry& meter_entry) LOCKS_EXCLUDED(lock_);

  // Read the counter data of a table entry.
  ::util::StatusOr<::p4::v1::DirectCounterEntry> ReadDirectCounterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::DirectCounterEntry& direct_counter_entry)
      LOCKS_EXCLUDED(lock_);

  // Read the data of a register entry.
  ::util::Status ReadRegisterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::RegisterEntry& register_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Read the data of a meter entry.
  ::util::Status ReadMeterEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::MeterEntry& meter_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance.
  static std::unique_ptr<TdiTableManager> CreateInstance(
      OperationMode mode, TdiSdeInterface* tdi_sde_interface, int device);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit TdiTableManager(OperationMode mode,
                           TdiSdeInterface* tdi_sde_interface, int device);

  ::util::Status BuildTableKey(const ::p4::v1::TableEntry& table_entry,
                               TdiSdeInterface::TableKeyInterface* table_key)
      SHARED_LOCKS_REQUIRED(lock_);

  ::util::Status BuildTableActionData(
      const ::p4::v1::Action& action,
      TdiSdeInterface::TableDataInterface* table_data);

  // Builds a SDE table data from the given P4 table entry. The table data
  // object is reset, even in case of failure.
  ::util::Status BuildTableData(
      const ::p4::v1::TableEntry& table_entry,
      TdiSdeInterface::TableDataInterface* table_data);

  ::util::Status ReadSingleTableEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  ::util::Status ReadDefaultTableEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  ::util::Status ReadAllTableEntries(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::TableEntry& table_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Construct a P4RT table entry from a table entry request, table key and
  // table data.
  ::util::StatusOr<::p4::v1::TableEntry> BuildP4TableEntry(
      const ::p4::v1::TableEntry& request,
      const TdiSdeInterface::TableKeyInterface* table_key,
      const TdiSdeInterface::TableDataInterface* table_data)
      SHARED_LOCKS_REQUIRED(lock_);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  std::vector<TimerDaemon::DescriptorPtr> register_timer_descriptors_
      GUARDED_BY(lock_);

  // Pointer to a TdiSdeInterface implementation that wraps all the SDE calls.
  TdiSdeInterface* tdi_sde_interface_ = nullptr;  // not owned by this class.

  // Helper class to validate the P4Info and requests against it.
  // TODO(max): Maybe this manager should be created in the node and passed down
  // to all feature managers.
  std::unique_ptr<P4InfoManager> p4_info_manager_ GUARDED_BY(lock_);

  // Fixed zero-based Tofino device number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_;

  friend class TdiTableManagerTest;
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_TABLE_MANAGER_H_
