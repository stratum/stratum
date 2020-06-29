// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H
#define STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "bf_rt/bf_rt_init.hpp"
#include "bf_rt/bf_rt_session.hpp"
#include "bf_rt/bf_rt_table_key.hpp"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt_id_mapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/timer_daemon.h"

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtCounterManager {
 public:
  // Pushes the forwarding pipeline config
  ::util::Status PushForwardingPipelineConfig(const BfrtDeviceConfig& config,
                                              const bfrt::BfRtInfo* bfrt_info)
      LOCKS_EXCLUDED(lock_);

  // Writes an indrect counter entry.
  ::util::Status WriteIndirectCounterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::Update::Type type,
      const ::p4::v1::CounterEntry& counter_entry) LOCKS_EXCLUDED(lock_);

  // Reads an indirect counter entry.
  ::util::StatusOr<::p4::v1::CounterEntry> ReadIndirectCounterEntry(
      std::shared_ptr<bfrt::BfRtSession> bfrt_session,
      const ::p4::v1::CounterEntry& counter_entry) LOCKS_EXCLUDED(lock_);

  // Creates a table manager instance.
  static std::unique_ptr<BfrtCounterManager> CreateInstance(
      const BfrtIdMapper* bfrt_id_mapper);

 private:
  // Private constructure, we can create the instance by using `CreateInstance`
  // function only.
  BfrtCounterManager(const BfrtIdMapper* bfrt_id_mapper);

  // And timer action that sync all counters.
  ::util::Status SyncCounters();

  // The BfRt info, requires by some function to get runtime
  // instances like tables.
  const bfrt::BfRtInfo* bfrt_info_ GUARDED_BY(lock_);

  // Reader-writer lock used to protect access to pipeline state.
  mutable absl::Mutex lock_;

  // The ID mapper that maps P4Runtime ID to BfRt ones (vice versa).
  // Not owned by this class
  const BfrtIdMapper* bfrt_id_mapper_;

  // The timer that sync up counters periodically
  TimerDaemon::DescriptorPtr counter_sync_timer_;
};

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BAREFOOT_BFRT_COUNTER_MANAGER_H
