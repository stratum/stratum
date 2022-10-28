// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_TDI_TDI_ACTION_PROFILE_MANAGER_H_
#define STRATUM_HAL_LIB_TDI_TDI_ACTION_PROFILE_MANAGER_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"

namespace stratum {
namespace hal {
namespace tdi {

class TdiActionProfileManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushForwardingPipelineConfig(const TdiDeviceConfig& config)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ExternEntry& action_profile_entry) LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileMember(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile group.
  ::util::Status WriteActionProfileGroup(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileEntry(s) matched by the given extern entry.
  ::util::Status ReadActionProfileEntry(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::ExternEntry& action_profile_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileMember(s) matched by the given entry.
  ::util::Status ReadActionProfileMember(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileGroup(s) matched by the given entry.
  ::util::Status ReadActionProfileGroup(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates an action profile manager instance.
  static std::unique_ptr<TdiActionProfileManager> CreateInstance(
      TdiSdeInterface* tdi_sde_interface, int device);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit TdiActionProfileManager(TdiSdeInterface* tdi_sde_interface,
                                    int device);

  // Internal version of WriteActionProfileMember which takes no locks.
  ::util::Status DoWriteActionProfileMember(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of WriteActionProfileGroup which takes no locks.
  ::util::Status DoWriteActionProfileGroup(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileMember which takes no locks.
  ::util::Status DoReadActionProfileMember(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileGroup which takes no locks.
  ::util::Status DoReadActionProfileGroup(
      std::shared_ptr<TdiSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  // TODO(max): Check if removeable
  mutable absl::Mutex lock_;

  // Pointer to a TdiSdeInterface implementation that wraps all the SDE calls.
  TdiSdeInterface* tdi_sde_interface_ = nullptr;  // not owned by this class.

  // Helper class to validate the P4Info and requests against it.
  // TODO(max): Maybe this manager should be created in the node and passed down
  // to all feature managers.
  std::unique_ptr<P4InfoManager> p4_info_manager_ GUARDED_BY(lock_);

  // Fixed zero-based Tofino device number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_;
};

}  // namespace tdi
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_TDI_ACTION_PROFILE_MANAGER_H_
