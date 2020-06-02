// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/phal/phal_sim.h"

#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/macros.h"
#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

PhalSim* PhalSim::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex PhalSim::init_lock_(absl::kConstInit);

/* static */
constexpr int PhalSim::kMaxNumTransceiverEventWriters;

PhalSim::PhalSim() {}

PhalSim::~PhalSim() {}

::util::Status PhalSim::PushChassisConfig(const ChassisConfig& config) {
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    // TODO(unknown): Implement this function.
    initialized_ = true;
  }

  return ::util::OkStatus();
}

::util::Status PhalSim::VerifyChassisConfig(const ChassisConfig& config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::Shutdown() {
  absl::WriterMutexLock l(&config_lock_);
  initialized_ = false;

  return ::util::OkStatus();
}

::util::StatusOr<int> PhalSim::RegisterTransceiverEventWriter(
    std::unique_ptr<ChannelWriter<TransceiverEvent>> writer, int priority) {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  CHECK_RETURN_IF_FALSE(transceiver_event_writers_.size() <
                        static_cast<size_t>(kMaxNumTransceiverEventWriters))
      << "Can only support " << kMaxNumTransceiverEventWriters
      << " transceiver event Writers.";

  // Find the next available ID for the Writer.
  int next_id = kInvalidWriterId;
  for (int id = 1;
       id <= static_cast<int>(transceiver_event_writers_.size()) + 1;
       ++id) {
    auto it = std::find_if(
        transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
        [id](const TransceiverEventWriter& w) { return w.id == id; });
    if (it == transceiver_event_writers_.end()) {
      // This id is free. Pick it up.
      next_id = id;
      break;
    }
  }
  CHECK_RETURN_IF_FALSE(next_id != kInvalidWriterId)
      << "Could not find a new ID for the Writer. next_id=" << next_id << ".";

  transceiver_event_writers_.insert({std::move(writer), priority, next_id});

  return next_id;
}

::util::Status PhalSim::UnregisterTransceiverEventWriter(int id) {
  absl::WriterMutexLock l(&config_lock_);

  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  auto it = std::find_if(
      transceiver_event_writers_.begin(), transceiver_event_writers_.end(),
      [id](const TransceiverEventWriter& h) { return h.id == id; });
  CHECK_RETURN_IF_FALSE(it != transceiver_event_writers_.end())
      << "Could not find a transceiver event Writer with ID " << id << ".";

  transceiver_event_writers_.erase(it);

  return ::util::OkStatus();
}

::util::Status PhalSim::GetFrontPanelPortInfo(
    int slot, int port, FrontPanelPortInfo* fp_port_info) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::GetOpticalTransceiverInfo(
    int module, int network_interface, OpticalTransceiverInfo* ot_info) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::SetOpticalTransceiverInfo(
    int module, int network_interface, const OpticalTransceiverInfo& ot_info) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status PhalSim::SetPortLedState(int slot, int port, int channel,
                                        LedColor color, LedState state) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

PhalSim* PhalSim::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new PhalSim();
  }

  return singleton_;
}

}  // namespace hal
}  // namespace stratum
