/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpEventHandler;

// Generic callback for status changes on ONLP.
class OnlpEventCallback {
 public:
  OnlpEventCallback();
  virtual ~OnlpEventCallback() {}

  // Implementations should override this function to perform the desired
  // callback when the onlp status changes.
  virtual ::util::Status HandleStatusChange(const OidInfo& oid_info) = 0;

 protected:
  friend class OnlpEventHandler;
  // The OnlpEventHandler that is currently sending updates to this callback.
  OnlpEventHandler* handler_;
};

// Represents a callback for status changes on a specific ONLP OID.
class OnlpOidEventCallback : public OnlpEventCallback {
 public:
  // Creates a new OnlpEventCallback that receives callbacks for any status
  // changes that occur for the given oid.
  explicit OnlpOidEventCallback(OnlpOid oid);
  OnlpOidEventCallback(const OnlpOidEventCallback& other) = delete;
  OnlpOidEventCallback& operator=(const OnlpOidEventCallback& other) = delete;
  virtual ~OnlpOidEventCallback();

  virtual OnlpOid GetOid() const { return oid_; }

 private:
  friend class OnlpEventHandler;
  OnlpOid oid_;
};

// Represents a callback for status changes on any of the ONLP SFPs.
// Mainly, a callback when a SFP plugged in or unplugged event was detected.
// Note: this callback does not have to be associated with any specific SFP.
class OnlpSfpEventCallback : public OnlpEventCallback {
 public:
  // Creates a new OnlpSfpEventCallback that receives callbacks for status
  // changes that occur for any SFPs.
  OnlpSfpEventCallback();
  OnlpSfpEventCallback(const OnlpSfpEventCallback& other) = delete;
  OnlpSfpEventCallback& operator=(const OnlpSfpEventCallback& other) = delete;
  virtual ~OnlpSfpEventCallback();
};

class OnlpEventHandler {
 public:
  static ::util::StatusOr<std::unique_ptr<OnlpEventHandler>> Make(
      const OnlpInterface* onlp);
  OnlpEventHandler(const OnlpEventHandler& other) = delete;
  OnlpEventHandler& operator=(const OnlpEventHandler& other) = delete;
  virtual ~OnlpEventHandler();

  // Starts sending callbacks to the given OnlpEventCallback. The specific
  // events which will be sent to this callback are specified within the given
  // OnlpEventCallback. This does not take ownership of the given callback.
  // The owner of the callback can safely destroy it at any time after this
  // call, and it will automatically unregister itself. Only one callback may
  // exist per OID.
  virtual ::util::Status RegisterOidEventCallback(
      OnlpOidEventCallback* callback);
  // Stops sending callbacks to the given OnlpEventCallback. This is called
  // automatically if a OnlpEventCallback is deleted.
  virtual ::util::Status UnregisterOidEventCallback(
      OnlpOidEventCallback* callback);

  // Starts sending callbacks to the given OnlpSfpEventCallback.
  // Only one callback may exist for all of the SFPs.
  virtual ::util::Status RegisterSfpEventCallback(
      OnlpSfpEventCallback* callback);
  // Stops sending callbacks to the given OnlpSfpEventCallback. This is called
  // automatically if a OnlpSfpEventCallback is deleted.
  virtual ::util::Status UnregisterSfpEventCallback(
      OnlpSfpEventCallback* callback);

  // Adds a single callback that is called once after each time any other onlp
  // callback executes. If this callback already exists, it is overwritten. The
  // callback is passed a failing status if something went wrong while running
  // normal event callbacks.
  virtual void AddUpdateCallback(std::function<void(::util::Status)> callback);

 protected:
  explicit OnlpEventHandler(const OnlpInterface* onlp)
      : onlp_(onlp), max_front_port_num_(ONLP_MAX_FRONT_PORT_NUM) {}

 private:
  friend class OnlpEventHandlerTest;
  struct OidStatusMonitor {
    HwState previous_status = HW_STATE_UNKNOWN;
    OnlpOidEventCallback* callback = nullptr;
  };

  // Keep track the entire SFP presence bitmap instead of individual
  // present status - for better performance.
  struct SfpStatusMonitor {
    OnlpPresentBitmap previous_map;
    OnlpSfpEventCallback* callback = nullptr;
  };

  // Initializes and starts the thread that polls onlp for oid and
  // sfp presence updates.
  ::util::Status InitializePollingThread();
  // Helper function for pthread_create.
  static void* RunPollingThread(void* onlp_event_handler_ptr);
  ::util::Status PollOids();
  ::util::Status PollSfpPresence();

  const OnlpInterface* onlp_ = nullptr;
  absl::Mutex monitor_lock_;
  absl::CondVar monitor_cond_var_;
  absl::flat_hash_map<OnlpOid, OidStatusMonitor> status_monitors_
      GUARDED_BY(monitor_lock_);
  std::function<void(::util::Status)> update_callback_
      GUARDED_BY(monitor_lock_);
  SfpStatusMonitor sfp_status_monitor_ GUARDED_BY(monitor_lock_);
  OnlpPortNumber max_front_port_num_ GUARDED_BY(monitor_lock_);
  // This pointer is set whenever we are currently executing a callback. This
  // lets us freely call UnregisterEventCallback for any callback except the one
  // that is currently executing.
  OnlpEventCallback* executing_callback_ = nullptr;
  bool monitor_loop_running_ GUARDED_BY(monitor_lock_) = false;
  pthread_t monitor_loop_thread_id_;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_EVENT_HANDLER_H_
