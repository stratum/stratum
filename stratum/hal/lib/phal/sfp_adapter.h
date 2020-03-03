// Copyright 2019 Open Networking Foundation
// Copyright 2019 Dell EMC
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_
#define STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_

#include <memory>
#include <thread>  // NOLINT
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "stratum/lib/channel/channel.h"

namespace stratum {
namespace hal {
namespace phal {

class SfpAdapter final : public Adapter {
 public:
  explicit SfpAdapter(AttributeDatabaseInterface* attribute_db_interface);

  ~SfpAdapter();

  // Slot and port are 1-based.
  ::util::Status GetFrontPanelPortInfo(int slot, int port,
                                       FrontPanelPortInfo* fp_port_info);

  // Registers a subscriber to receive sfp state change notifications.
  // The returned id can be used to unregister later.
  // See: PhalInterface::RegisterTransceiverEventWriter.
  ::util::StatusOr<int> RegisterSfpEventSubscriber(
      std::unique_ptr<ChannelWriter<PhalInterface::TransceiverEvent>> writer,
      int priority) LOCKS_EXCLUDED(subscribers_lock_);

  // Unregisters a subscriber.
  // See: PhalInterface::UnregisterTransceiverEvent.
  ::util::Status UnregisterSfpEventSubscriber(int id)
      LOCKS_EXCLUDED(subscribers_lock_);

 private:
  // Conservative channel depth to never drop notifications.
  static constexpr size_t kDefaultChannelDepth = 512;

  // Attribute Db path to get the hardware state of all sfp transceivers.
  const Path kAllTransceiversPath = {
      PathEntry("cards", -1, true, true, false),
      PathEntry("ports", -1, true, true, false),
      PathEntry("transceiver"),
      PathEntry("hardware_state", -1, false, true, false),
  };

  // Helper function to create the subscription for all Sfp state changes. Only
  // set up once per instance.
  ::util::Status SetupSfpDatabaseSubscriptions()
      EXCLUSIVE_LOCKS_REQUIRED(subscribers_lock_);

  ::util::Status OneShotUpdate() LOCKS_EXCLUDED(subscribers_lock_);

  // Thread function that reads updates from the attribute database subscription
  // and passes them along the subscribers.
  ::util::Status TransceiverEventReaderThreadFunc(
      std::unique_ptr<ChannelReader<PhalDB>> reader);

  // Mutex guarding internal state.
  absl::Mutex subscribers_lock_;

  // Writers to forward the Transceiver events to. They are registered by
  // external manager classes to receive the SFP Transceiver events. The
  // managers can be running in different threads. The is sorted based on the
  // the priority of the TransceiverEventWriter intances.
  std::vector<PhalInterface::TransceiverEventWriter> subscribers_
      GUARDED_BY(subscribers_lock_);

  // Stores the subscription query to keep it alive.
  std::unique_ptr<Query> query_ GUARDED_BY(subscribers_lock_);

  // Stores pointer to the subscription channel to close it on shutdown.
  std::shared_ptr<Channel<PhalDB>> channel_ GUARDED_BY(subscribers_lock_);

  // Stores the attribute Db subscription reader thread.
  std::thread sfp_reader_thread_ GUARDED_BY(subscribers_lock_);
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SFP_ADAPTER_H_
