// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H_
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H_

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bf_sde_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtCounterManager {
 public:
  // Pushes the forwarding pipeline config
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config)
      LOCKS_EXCLUDED(lock_);

  // Writes an indrect counter entry.
  ::util::Status WriteIndirectCounterEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::CounterEntry& counter_entry) LOCKS_EXCLUDED(lock_);

  // Reads an indirect counter entry.
  ::util::Status ReadIndirectCounterEntry(
      std::shared_ptr<BfSdeInterface::SessionInterface> session,
      const ::p4::v1::CounterEntry& counter_entry,
      WriterInterface<::p4::v1::ReadResponse>* writer) LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance.
  static std::unique_ptr<BfrtCounterManager> CreateInstance(
      BfSdeInterface* bf_sde_interface_, int device);

 private:
  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  explicit BfrtCounterManager(BfSdeInterface* bf_sde_interface_, int device);

  // Reader-writer lock used to protect access to pipeline state.
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

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H_
