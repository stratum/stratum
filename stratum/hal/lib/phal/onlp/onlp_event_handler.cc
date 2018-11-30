// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"

#include <algorithm>

#include "base/commandlineflags.h"
#include "stratum/lib/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "util/gtl/map_util.h"
#include "util/task/status.h"

// Note: We want to keep this polling interval relatively short. Unlike with
// with udev, it's possible for us to miss state changes entirely if they occur
// too fast in succession. This may not matter in most cases, but in extremely
// unlikely edge cases it could cause issues. E.g. if a transceiver is removed
// and a different one is inserted into the same port in less than ~200ms, we
// won't report any change in hardware state.
// TODO: Deal more precisely with removable hardware components. For
// instance, if we notice that fixed fields for a transceiver have changed, we
// should report this as a removal event and an insertion event.
ABSL_FLAG(int32, onlp_polling_interval_ms, 200,
          "Polling interval for checking ONLP for hardware state changes.");

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

OnlpEventCallback::OnlpEventCallback(OnlpOid oid)
    : oid_(oid), handler_(nullptr) {}

OnlpEventCallback::~OnlpEventCallback() {
  if (handler_ != nullptr) {
    ::util::Status unregister_result = handler_->UnregisterEventCallback(this);
    if (!unregister_result.ok()) {
      LOG(ERROR)
          << "Encountered error while unregistering an ONLP event callback: "
          << unregister_result;
    }
  }
}

::util::StatusOr<std::unique_ptr<OnlpEventHandler>> OnlpEventHandler::Make(
    const OnlpInterface* onlp) {
  std::unique_ptr<OnlpEventHandler> handler(new OnlpEventHandler(onlp));
  RETURN_IF_ERROR(handler->InitializePollingThread());
  return std::move(handler);
}

OnlpEventHandler::~OnlpEventHandler() {
  bool running = false;
  {
    absl::MutexLock lock(&monitor_lock_);
    std::swap(running, monitor_loop_running_);
  }
  if (running) pthread_join(monitor_loop_thread_id_, nullptr);

  // Unregister any remaining event callbacks.
  absl::MutexLock lock(&monitor_lock_);
  for (auto& oid_and_monitor : status_monitors_) {
    OidStatusMonitor& monitor = oid_and_monitor.second;
    monitor.callback->handler_ = nullptr;
  }
}

::util::Status OnlpEventHandler::RegisterEventCallback(
    OnlpEventCallback* callback) {
  absl::MutexLock lock(&monitor_lock_);
  CHECK_RETURN_IF_FALSE(callback->handler_ == nullptr)
      << "Cannot register a callback that is already registered.";
  OidStatusMonitor& status_monitor =
      gtl::LookupOrInsert(&status_monitors_, callback->GetOid(), {});
  CHECK_RETURN_IF_FALSE(status_monitor.callback == nullptr)
      << "Cannot register two callbacks for the same OID.";
  status_monitor.callback = callback;
  callback->handler_ = this;
  // previous_status is initialized to HW_STATE_UNKNOWN, so we'll automatically
  // send an initial update to this callback.
  return ::util::OkStatus();
}

::util::Status OnlpEventHandler::UnregisterEventCallback(
    OnlpEventCallback* callback) {
  absl::MutexLock lock(&monitor_lock_);
  CHECK_RETURN_IF_FALSE(callback->handler_ == this)
      << "Cannot unregister a callback that is not currently registered.";
  // We can't unregister this callback while it's running.
  while (executing_callback_ == callback)
    monitor_cond_var_.Wait(&monitor_lock_);

  OidStatusMonitor* status_monitor =
      gtl::FindOrNull(status_monitors_, callback->GetOid());
  CHECK_RETURN_IF_FALSE(status_monitor != nullptr)
      << "Encountered an OnlpEventCallback with no matching status monitor.";
  callback->handler_ = nullptr;
  status_monitors_.erase(callback->GetOid());
  return ::util::OkStatus();
}

void OnlpEventHandler::AddUpdateCallback(
    std::function<void(::util::Status)> callback) {
  absl::MutexLock lock(&monitor_lock_);
  while (executing_callback_) monitor_cond_var_.Wait(&monitor_lock_);
  update_callback_ = std::move(callback);
}

::util::Status OnlpEventHandler::InitializePollingThread() {
  absl::MutexLock lock(&monitor_lock_);
  CHECK_RETURN_IF_FALSE(!pthread_create(&monitor_loop_thread_id_, nullptr,
                                        &OnlpEventHandler::RunPollingThread,
                                        this))
      << "Failed to start the polling thread.";
  monitor_loop_running_ = true;
  return ::util::OkStatus();
}

void* OnlpEventHandler::RunPollingThread(void* onlp_event_handler_ptr) {
  OnlpEventHandler* handler =
      static_cast<OnlpEventHandler*>(onlp_event_handler_ptr);
  absl::Time last_polling_time = absl::InfinitePast();
  while (true) {
    // We keep the polling time as consistent as possible.
    absl::SleepFor(
        last_polling_time +
        absl::Milliseconds(base::GetFlag(FLAGS_onlp_polling_interval_ms)) -
        absl::Now());
    last_polling_time = absl::Now();
    {
      absl::MutexLock lock(&handler->monitor_lock_);
      if (!handler->monitor_loop_running_) break;
    }
    ::util::Status result = handler->PollOids();
    if (!result.ok()) {
      LOG(ERROR) << "Error while polling oids: " << result;
    }
  }
  return nullptr;
}

::util::Status OnlpEventHandler::PollOids() {
  // First we find all of the oids that have been updated.
  absl::flat_hash_map<OnlpOid, OidInfo> updated_oids;
  {
    absl::MutexLock lock(&monitor_lock_);
    for (auto& oid_and_monitor : status_monitors_) {
      OnlpOid oid = oid_and_monitor.first;
      OidStatusMonitor& status_monitor = oid_and_monitor.second;
      ASSIGN_OR_RETURN(OidInfo info, onlp_->GetOidInfo(oid));
      HwState new_status = info.GetHardwareState();
      if (new_status != status_monitor.previous_status) {
        status_monitor.previous_status = new_status;
        updated_oids.insert(std::make_pair(oid, info));
      }
    }
  }

  // Now we actually send updates.
  ::util::Status result = ::util::OkStatus();
  bool callback_sent = false;
  for (const auto& oid_and_info : updated_oids) {
    OnlpOid oid = oid_and_info.first;
    const OidInfo& info = oid_and_info.second;
    {
      absl::MutexLock lock(&monitor_lock_);
      OidStatusMonitor* status_monitor = gtl::FindOrNull(status_monitors_, oid);
      // This callback may have already been unregistered, in which case we
      // silently skip it.
      if (status_monitor == nullptr) continue;
      executing_callback_ = status_monitor->callback;
      callback_sent = true;
    }
    // We don't hold the monitor lock while executing our callback. This means
    // that our callback is allowed to register or unregister any callback
    // except itself (attempting to unregister itself will deadlock).
    APPEND_STATUS_IF_ERROR(result,
                           executing_callback_->HandleOidStatusChange(info));
    {
      absl::MutexLock lock(&monitor_lock_);
      executing_callback_ = nullptr;
      monitor_cond_var_.SignalAll();
    }
  }

  // We sent an update callback if at least one event callback occurred.
  if (callback_sent) {
    absl::MutexLock lock(&monitor_lock_);
    if (update_callback_) {
      update_callback_(result);
    }
  }
  return result;
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
