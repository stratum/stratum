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


#include "third_party/stratum/hal/lib/phal/udev_event_handler.h"

#include "base/commandlineflags.h"
#include "third_party/stratum/hal/lib/common/constants.h"
#include "third_party/stratum/lib/macros.h"
#include "third_party/absl/synchronization/mutex.h"
#include "util/gtl/map_util.h"

DEFINE_int32(udev_polling_interval_ms, 200,
             "Polling interval for checking udev events in the udev thread.");

namespace stratum {
namespace hal {
namespace phal {

// TODO: Add a udev action type enum for ADD, REMOVE, and CHANGE.

UdevEventCallback::UdevEventCallback(const std::string& udev_filter,
                                     const std::string& dev_path)
    : udev_filter_(udev_filter), dev_path_(dev_path) {}

UdevEventCallback::~UdevEventCallback() {
  if (handler_ != nullptr) {
    if (!handler_->UnregisterEventCallback(this).ok()) {
      LOG(ERROR) << "Encountered error while unregistering udev callback.";
    }
  }
}

UdevEventHandler::~UdevEventHandler() {
  bool running = false;
  {
    absl::MutexLock lock(&udev_lock_);
    std::swap(running, udev_monitor_loop_running_);
  }
  if (running) pthread_join(udev_monitor_loop_thread_id_, nullptr);

  // Unregister any remaining event callbacks.
  absl::MutexLock lock(&udev_lock_);
  for (const auto& filter_and_monitor : udev_monitors_) {
    const UdevMonitorInfo& monitor = filter_and_monitor.second;
    for (const auto& dev_path_and_callback : monitor.dev_path_to_callback) {
      UdevEventCallback* callback = dev_path_and_callback.second;
      callback->SetUdevEventHandler(nullptr);
    }
  }
}

::util::Status UdevEventHandler::AddNewUdevMonitor(
    const std::string& udev_filter) {
  std::unique_ptr<UdevMonitor> udev_monitor;
  // We first begin monitoring events for this udev_filter, and then enumerate
  // all devices in the system. This gives us an up-to-date picture of the
  // system's state. The order here is key -- we need to catch any events that
  // happen immediately after we enumerate devices, so we have to start
  // listening first!
  ASSIGN_OR_RETURN(udev_monitor, udev_->MakeUdevMonitor());
  RETURN_IF_ERROR(udev_monitor->AddFilter(udev_filter));
  RETURN_IF_ERROR(udev_monitor->EnableReceiving());
  UdevMonitorInfo monitor_info;
  // We've successfully started listening, so we can enumerate devices.
  ASSIGN_OR_RETURN(auto existing_dev_paths_and_actions,
                   udev_->EnumerateSubsystem(udev_filter));
  for (const auto& dev_path_and_action : existing_dev_paths_and_actions) {
    Udev::Event fake_action;
    fake_action.sequence_number = 0;
    fake_action.action_type = dev_path_and_action.second;
    monitor_info.dev_path_to_last_action[dev_path_and_action.first] =
        fake_action;
  }
  monitor_info.monitor = std::move(udev_monitor);
  auto ret = udev_monitors_.insert(
      std::make_pair(udev_filter, std::move(monitor_info)));
  CHECK_RETURN_IF_FALSE(ret.second) << "Cannot add the same monitor twice.";
  return ::util::OkStatus();
}

::util::StatusOr<bool> UdevEventHandler::UpdateUdevMonitorInfo(
    UdevMonitorInfo* monitor_info, Udev::Event event) {
  const std::string& dev_path = event.device_path;
  UdevSequenceNumber seqnum = event.sequence_number;
  const std::string& action = event.action_type;
  CHECK_RETURN_IF_FALSE(!dev_path.empty() && !action.empty())
      << "Encountered invalid udev event (" << dev_path << ", " << action
      << ").";
  auto prev_event =
      gtl::FindOrNull(monitor_info->dev_path_to_last_action, dev_path);
  // Only update if the new seqnum is greater than the one seen previously.
  bool perform_update = prev_event && seqnum > prev_event->sequence_number;
  if (perform_update) monitor_info->dev_path_to_last_action[dev_path] = event;
  return perform_update;
}

::util::Status UdevEventHandler::RegisterEventCallback(
    UdevEventCallback* callback) {
  absl::MutexLock lock(&udev_lock_);
  CHECK_RETURN_IF_FALSE(callback->GetUdevEventHandler() == nullptr)
      << "Cannot register a UdevEventCallback twice.";
  UdevMonitorInfo* found_monitor =
      gtl::FindOrNull(udev_monitors_, callback->GetUdevFilter());
  if (found_monitor == nullptr) {
    // we must create a new udev monitor for this udev_filter.
    RETURN_IF_ERROR(AddNewUdevMonitor(callback->GetUdevFilter()));
    found_monitor = gtl::FindOrNull(udev_monitors_, callback->GetUdevFilter());
    CHECK_RETURN_IF_FALSE(found_monitor)
        << "Could not find udev monitor that was just added.";
  }
  auto ret = found_monitor->dev_path_to_callback.insert(
      std::make_pair(callback->GetDevPath(), callback));
  CHECK_RETURN_IF_FALSE(ret.second)
      << "Cannot register multiple callbacks for a single filter/dev_path.";
  // Mark this device as updated so that we always receive an initial callback.
  found_monitor->dev_paths_to_update.insert(callback->GetDevPath());
  // Add a "remove" event for this device. If the device is not present, our
  // initial callback will report a "remove" action (a reasonable default).
  // If the device is already present, then it will already have an action in
  // dev_path_to_last_action, and insert is a no-op.
  Udev::Event fake_action;
  fake_action.sequence_number = 0;
  fake_action.action_type = "remove";
  found_monitor->dev_path_to_last_action.insert(
      std::make_pair(callback->GetDevPath(), fake_action));
  callback->SetUdevEventHandler(this);
  return ::util::OkStatus();
}

::util::Status UdevEventHandler::UnregisterEventCallback(
    UdevEventCallback* callback) {
  CHECK_RETURN_IF_FALSE(callback->GetUdevEventHandler() == this)
      << "Attempted to unregister a callback that is registered with a "
      << "different UdevEventHandler.";
  absl::MutexLock lock(&udev_lock_);
  while (executing_callback_ == callback) {
    // We can't unregister a callback while it's running.
    udev_cond_var_.Wait(&udev_lock_);
  }
  // We are not executing this callback, and can safely remove it.
  callback->SetUdevEventHandler(nullptr);
  auto found_monitor =
      gtl::FindOrNull(udev_monitors_, callback->GetUdevFilter());
  CHECK_RETURN_IF_FALSE(found_monitor)
      << "Could not find udev monitor " << callback->GetUdevFilter() << ".";
  CHECK_RETURN_IF_FALSE(
      found_monitor->dev_path_to_callback.erase(callback->GetDevPath()) == 1)
      << "Could not find callback for dev_path " << callback->GetDevPath()
      << ".";
  return ::util::OkStatus();
}

::util::StatusOr<std::unique_ptr<UdevEventHandler>>
UdevEventHandler::MakeUdevEventHandler(
    const SystemInterface* system_interface) {
  std::unique_ptr<UdevEventHandler> handler(
      new UdevEventHandler(system_interface));
  RETURN_IF_ERROR(handler->InitializeUdev());
  RETURN_IF_ERROR(handler->StartMonitorThread());
  return std::move(handler);
}

::util::Status UdevEventHandler::InitializeUdev() {
  absl::MutexLock lock(&udev_lock_);
  ASSIGN_OR_RETURN(udev_, system_interface_->MakeUdev());
  return ::util::OkStatus();
}

::util::Status UdevEventHandler::StartMonitorThread() {
  absl::MutexLock lock(&udev_lock_);
  CHECK_RETURN_IF_FALSE(!pthread_create(&udev_monitor_loop_thread_id_, nullptr,
                                        &UdevEventHandler::RunUdevMonitorLoop,
                                        this));
  udev_monitor_loop_running_ = true;
  return ::util::OkStatus();
}

void* UdevEventHandler::RunUdevMonitorLoop(void* udev_event_handler_ptr) {
  UdevEventHandler* udev_event_handler =
      static_cast<UdevEventHandler*>(udev_event_handler_ptr);
  udev_event_handler->UdevMonitorLoop();
  return nullptr;
}

void UdevEventHandler::UdevMonitorLoop() {
  while (true) {
    {
      // Check if the thread should stop.
      absl::MutexLock lock(&udev_lock_);
      if (!udev_monitor_loop_running_) break;
    }
    usleep(FLAGS_udev_polling_interval_ms * 1000);
    ::util::Status poll_status = PollUdevMonitors();
    if (!poll_status.ok()) {
      LOG(ERROR) << "PollUdevMonitors failed: " << poll_status.error_message();
      continue;
    }
    ::util::Status callback_status = SendCallbacks();
    if (!callback_status.ok()) {
      LOG(ERROR) << "SendCallbacks failed: " << callback_status.error_message();
    }
  }
}

::util::Status UdevEventHandler::PollUdevMonitors() {
  absl::MutexLock lock(&udev_lock_);
  for (auto& filter_and_monitor : udev_monitors_) {
    UdevMonitorInfo* monitor_info = &filter_and_monitor.second;
    while (true) {
      Udev::Event event;
      ::util::StatusOr<bool> found_event =
          monitor_info->monitor->GetUdevEvent(&event);
      CHECK_RETURN_IF_FALSE(found_event.ok())
          << "Failed to get new udev event.";
      if (!found_event.ValueOrDie()) break;  // We have seen every new event.
      ASSIGN_OR_RETURN(bool update_performed,
                       UpdateUdevMonitorInfo(monitor_info, event));
      if (update_performed) {
        // If anyone is listening, send a callback.
        monitor_info->dev_paths_to_update.insert(event.device_path);
      }
    }
  }
  return ::util::OkStatus();
}

::util::StatusOr<bool> UdevEventHandler::FindCallbackToExecute(
    UdevEventCallback** callback_to_execute, std::string* action_to_send) {
  absl::MutexLock lock(&udev_lock_);
  for (auto& filter_and_monitor : udev_monitors_) {
    UdevMonitorInfo* monitor_info = &filter_and_monitor.second;
    for (const auto& dev_path : monitor_info->dev_paths_to_update) {
      // An event has occurred on this monitor. Look for a callback.
      auto callback =
          gtl::FindOrNull(monitor_info->dev_path_to_callback, dev_path);
      if (!callback) continue;
      auto last_action =
          gtl::FindOrNull(monitor_info->dev_path_to_last_action, dev_path);
      CHECK_RETURN_IF_FALSE(last_action)
          << "An event occurred, but could not be found.";
      // We set executing_callback_ so that no other thread can delete
      // or unregister this callback until we're done running it.
      executing_callback_ = *callback_to_execute = *callback;
      *action_to_send = last_action->action_type;
      monitor_info->dev_paths_to_update.erase(dev_path);
      return true;
    }
  }
  return false;  // We've searched everything and found no callback.
}

::util::Status UdevEventHandler::SendCallbacks() {
  {
    absl::MutexLock lock(&udev_lock_);
    CHECK_RETURN_IF_FALSE(executing_callback_ == nullptr)
        << "Encountered non-null executing_callback_, but no "
        << "callback is currently executing.";
  }
  // We are now ready to find and execute callbacks! We find and execute one
  // callback on each pass through this loop.
  while (true) {
    UdevEventCallback* callback_to_execute = nullptr;
    std::string action_to_send;
    ASSIGN_OR_RETURN(
        bool callback_found,
        FindCallbackToExecute(&callback_to_execute, &action_to_send));
    if (!callback_found) return ::util::OkStatus();  // No more callbacks!
    CHECK_RETURN_IF_FALSE(callback_to_execute != nullptr)
        << "We should never reach this point if no callback is running.";
    // We release udev_lock_ while executing this callback. This enables
    // callbacks to register or unregister other callbacks (but not
    // themselves, thanks to executing_callback_).
    // TODO: Handle errors in callbacks.
    callback_to_execute->HandleUdevEvent(action_to_send).IgnoreError();
    {
      absl::MutexLock lock(&udev_lock_);
      executing_callback_ = nullptr;
      udev_cond_var_.SignalAll();
    }
  }
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
