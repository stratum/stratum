// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_PRE_MANAGER_H_
#define STRATUM_HAL_LIB_TDI_TDI_PRE_MANAGER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"

namespace stratum {
namespace hal {
namespace tdi {

using PreEntry = ::p4::v1::PacketReplicationEngineEntry;

class TdiPreManager {
 public:
  // Pushes a ForwardingPipelineConfig.
  ::util::Status PushForwardingPipelineConfig(const TdiDeviceConfig& config)
      LOCKS_EXCLUDED(lock_);

  // Writes a PRE entry.
  ::util::Status WritePreEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type& type, const PreEntry& entry)
      LOCKS_EXCLUDED(lock_);

  // Reads a PRE entry.
  ::util::Status ReadPreEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const PreEntry& entry, WriterInterface<::p4::v1::ReadResponse>* writer)
      LOCKS_EXCLUDED(lock_);

  static std::unique_ptr<TdiPreManager> CreateInstance(
      TdiSdeInterface* tdi_sde_interface, int device);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit TdiPreManager(TdiSdeInterface* tdi_sde_interface, int device);

  // Insert/Modify/Delete a multicast group entry.
  // This function creates one or more multicast nodes based on replicas in
  // the entry and associate them to a multicast group.
  ::util::Status WriteMulticastGroupEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type& type,
      const ::p4::v1::MulticastGroupEntry& entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Insert/Modify/Delete a clone session entry.
  ::util::Status WriteCloneSessionEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type& type,
      const ::p4::v1::CloneSessionEntry& entry) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reads a multicast group entry.
  ::util::Status ReadMulticastGroupEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::MulticastGroupEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reads a clone session entry.
  ::util::Status ReadCloneSessionEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::CloneSessionEntry& entry,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Insert new multicast nodes of a given multicast group.
  ::util::StatusOr<std::vector<uint32>> InsertMulticastNodes(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::MulticastGroupEntry& entry)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // Pointer to a TdiSdeInterface implementation that wraps all the SDE calls.
  TdiSdeInterface* tdi_sde_interface_ = nullptr;  // not owned by this class.

  // Fixed zero-based Tofino device number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_;
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_PRE_MANAGER_H_
