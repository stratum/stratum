/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {
class UdevEventHandler;

// Represents a callback for a specific udev filter and dev path. This callback
// will be called every time any action occurs for the given device, as well as
// once when the callback is first registered. All callbacks should be
// unregistered or destroyed before their UdevEventHandler is destroyed.
class UdevEventCallback {
 public:
  // Creates a new UdevEventCallback that will receive callbacks for any actions
  // received for the given udev filter string and device path.
  UdevEventCallback(const std::string& udev_filter,
                    const std::string& dev_path);
  // Destroying a UdevEventCallback will automatically and safely unregister it
  // from any UdevEventHandler that is currently handling it, even if it is
  // currently executing.
  virtual ~UdevEventCallback();

  // These functions are used by UdevEventHandler when registering and
  // unregistering this callback.
  const std::string& GetUdevFilter() { return udev_filter_; }
  const std::string& GetDevPath() { return dev_path_; }

  // Implementations should override this function to perform the desired
  // callback. Note that this function should not be called by anything except
  // a UdevEventHandler unless it provides its own threadsafety.
  // UdevEventHandler will never call two callbacks simultaneously.
  virtual ::util::Status HandleUdevEvent(const std::string& action) = 0;

 private:
  friend class UdevEventHandler;
  UdevEventHandler* GetUdevEventHandler() { return handler_; }
  void SetUdevEventHandler(UdevEventHandler* handler) { handler_ = handler; }

  // The udev filter and device to which this callback will respond.
  const std::string udev_filter_;
  const std::string dev_path_;
  // The UdevEventHandler currently handling this UdevEventCallback.
  UdevEventHandler* handler_ = nullptr;
};

// Sends callbacks to a set of UdevEventCallback objects when system hardware
// state changes. This is built on top of libudev, and will respond to fake
// udev events as well as actual hardware events.
class UdevEventHandler {
 public:
  virtual ~UdevEventHandler();
  // Creates a new UdevEventHandler that uses the given SystemInterface to
  // detect all udev events.
  static ::util::StatusOr<std::unique_ptr<UdevEventHandler>>
  MakeUdevEventHandler(const SystemInterface* system_interface);

  // Starts sending callbacks to the given UdevEventCallback. The specific
  // events which will be sent to this callback are specified within the given
  // UdevEventCallback. This does not take ownership of the given callback.
  // The owner of the callback can safely destroy it at any time after this
  // call, and it will automatically unregister itself.
  virtual ::util::Status RegisterEventCallback(UdevEventCallback* callback);
  // Stops sending callbacks to the given UdevEventCallback. This is called
  // automatically if a UdevEventCallback is deleted.
  virtual ::util::Status UnregisterEventCallback(UdevEventCallback* callback);

  // Adds a single callback that is called once after each time any other udev
  // callback executes. If this callback already exists, it is overwritten. The
  // callback is passed a failing status if something went wrong while running
  // normal event callbacks.
  virtual void AddUpdateCallback(std::function<void(::util::Status)> callback);

 protected:
  explicit UdevEventHandler(const SystemInterface* system_interface)
      : system_interface_(system_interface) {}

 private:
  friend class UdevEventHandlerTest;
  // Holds all information pertaining to a single udev monitor. All fields in a
  // UdevMonitorInfo are guarded by the udev_lock_ in their parent
  // UdevEventHandler.
  struct UdevMonitorInfo {
    std::unique_ptr<UdevMonitor> monitor;
    // Maps device paths onto the most recent associated udev event.
    absl::flat_hash_map<std::string, Udev::Event> dev_path_to_last_action;
    absl::flat_hash_map<std::string, UdevEventCallback*> dev_path_to_callback;
    // This set is used to temporarily hold device paths that have seen some
    // sort of action. If a dev_path in this table has a corresponding entry in
    // dev_path_to_callback, the callback will be called.
    absl::flat_hash_set<std::string> dev_paths_to_update;
  };
  // Initializes everything necessary to listen for udev events.
  ::util::Status InitializeUdev();
  // Initializes and starts the thread that monitors udev events.
  ::util::Status StartMonitorThread();
  // Adds and initializes a new udev monitor that listens for actions
  // that match the given udev filter.
  ::util::Status AddNewUdevMonitor(const std::string& udev_filter)
      EXCLUSIVE_LOCKS_REQUIRED(udev_lock_);
  // Updates the given UdevMonitorInfo to reflect the new event. An update is
  // only performed if this event is the latest event seen for its device
  // (determined by udev sequence numbers). The returned bool is true iff the
  // event is new and an update was performed.
  ::util::StatusOr<bool> UpdateUdevMonitorInfo(UdevMonitorInfo* monitor_info,
                                               Udev::Event event)
      EXCLUSIVE_LOCKS_REQUIRED(udev_lock_);

  // This is a helper function for pthread_create.
  static void* RunUdevMonitorLoop(void* udev_event_handler_ptr);
  // Runs the main udev monitor loop. Does not return until
  // udev_monitor_loop_running_ is set to false.
  void UdevMonitorLoop() LOCKS_EXCLUDED(udev_lock_);
  // Searches for an event that has occurred and requires a callback. If no such
  // event is found, returns false. Otherwise, returns true and sets
  // callback_to_execute and action_to_send to the values appropriate for this
  // callback. If a callback is returned, sets executing_callback_ to this
  // callback.
  ::util::StatusOr<bool> FindCallbackToExecute(
      UdevEventCallback** callback_to_execute, std::string* action_to_send)
      LOCKS_EXCLUDED(udev_lock_);
  // These two helper functions are called by UdevMonitorLoop.
  ::util::Status PollUdevMonitors() LOCKS_EXCLUDED(udev_lock_);
  ::util::Status SendCallbacks() LOCKS_EXCLUDED(udev_lock_);

  const SystemInterface* system_interface_;

  absl::Mutex udev_lock_;
  absl::CondVar udev_cond_var_;
  std::unique_ptr<Udev> udev_ GUARDED_BY(udev_lock_);
  absl::flat_hash_map<std::string, UdevMonitorInfo> udev_monitors_
      GUARDED_BY(udev_lock_);
  std::function<void(::util::Status)> update_callback_ = nullptr;
  // This pointer is set whenever we are currently executing a callback.
  // This lets us freely call (Un)RegisterEventCallback for any callback except
  // the one that is currently executing.
  UdevEventCallback* executing_callback_ GUARDED_BY(udev_lock_) = nullptr;
  bool udev_monitor_loop_running_ GUARDED_BY(udev_lock_) = false;
  pthread_t udev_monitor_loop_thread_id_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_EVENT_HANDLER_H_
