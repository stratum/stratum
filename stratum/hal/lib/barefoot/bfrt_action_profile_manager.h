// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf.pb.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtActionProfileManager {
 public:
  // Pushes the pipline info.
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ExternEntry& action_profile_entry) LOCKS_EXCLUDED(lock_);

  // Writes an action profile member.
  ::util::Status WriteActionProfileMember(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      LOCKS_EXCLUDED(lock_);

  // Writes an action profile group.
  ::util::Status WriteActionProfileGroup(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileEntry(s) matched by the given extern entry.
  ::util::Status ReadActionProfileEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::ExternEntry& action_profile_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileMember(s) matched by the given entry.
  ::util::Status ReadActionProfileMember(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Reads the P4 ActionProfileGroup(s) matched by the given entry.
  ::util::Status ReadActionProfileGroup(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates an action profile manager instance.
  static std::unique_ptr<BfrtActionProfileManager> CreateInstance(
      BfSdeInterface* bf_sde_interface, int device);

 private:
  // Private constructor, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfrtActionProfileManager(BfSdeInterface* bf_sde_interface,
                                    int device);

  // Internal version of WriteActionProfileMember which takes no locks.
  ::util::Status DoWriteActionProfileMember(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileMember& action_profile_member)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of WriteActionProfileGroup which takes no locks.
  ::util::Status DoWriteActionProfileGroup(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id, const ::p4::v1::Update::Type type,
      const ::p4::v1::ActionProfileGroup& action_profile_group)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileMember which takes no locks.
  ::util::Status DoReadActionProfileMember(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id,
      const ::p4::v1::ActionProfileMember& action_profile_member,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Internal version of ReadActionProfileGroup which takes no locks.
  ::util::Status DoReadActionProfileGroup(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      uint32 bfrt_table_id,
      const ::p4::v1::ActionProfileGroup& action_profile_group,
      WriterInterface<::p4::v1::ReadResponse>* writer)
      SHARED_LOCKS_REQUIRED(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  // TODO(max): Check if removeable
  mutable absl::Mutex lock_;

  // Pointer to a BfSdeInterface implementation that wraps all the SDE calls.
  BfSdeInterface* bf_sde_interface_ = nullptr;  // not owned by this class.

  // Fixed zero-based Tofino device number corresponding to the node/ASIC
  // managed by this class instance. Assigned in the class constructor.
  const int device_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_ACTION_PROFILE_MANAGER_H_
