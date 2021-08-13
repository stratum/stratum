// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

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

// Represents a callback for status changes on a specific ONLP OID.
class OnlpEventCallback {
 public:
  // Creates a new OnlpEventCallback that receives callbacks for any status
  // changes that occur for the given oid.
  explicit OnlpEventCallback(OnlpOid oid);
  OnlpEventCallback(const OnlpEventCallback& other) = delete;
  OnlpEventCallback& operator=(const OnlpEventCallback& other) = delete;
  virtual ~OnlpEventCallback();

  // Implementations should override this function to perform the desired
  // callback when the oid status changes.
  virtual ::util::Status HandleOidStatusChange(const OidInfo& oid_info) = 0;

  virtual OnlpOid GetOid() const { return oid_; }

 private:
  friend class OnlpEventHandler;
  OnlpOid oid_;
  // The OnlpEventHandler that is currently sending updates to this callback.
  OnlpEventHandler* handler_;
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
  virtual ::util::Status RegisterEventCallback(OnlpEventCallback* callback);
  // Stops sending callbacks to the given OnlpEventCallback. This is called
  // automatically if a OnlpEventCallback is deleted.
  virtual ::util::Status UnregisterEventCallback(OnlpEventCallback* callback);

  // Adds a single callback that is called once after each time any other onlp
  // callback executes. If this callback already exists, it is overwritten. The
  // callback is passed a failing status if something went wrong while running
  // normal event callbacks.
  virtual void AddUpdateCallback(std::function<void(::util::Status)> callback);

 protected:
  explicit OnlpEventHandler(const OnlpInterface* onlp) : onlp_(onlp) {}

 private:
  friend class OnlpEventHandlerTest;
  struct OidStatusMonitor {
    HwState previous_status = HW_STATE_UNKNOWN;
    OnlpEventCallback* callback = nullptr;
  };

  // Initializes and starts the thread that polls onlp for oid updates.
  ::util::Status InitializePollingThread();
  // Helper function for pthread_create.
  static void* RunPollingThread(void* onlp_event_handler_ptr);
  ::util::Status PollOids();

  const OnlpInterface* onlp_ = nullptr;
  absl::Mutex monitor_lock_;
  absl::CondVar monitor_cond_var_;
  absl::flat_hash_map<OnlpOid, OidStatusMonitor> status_monitors_
      GUARDED_BY(monitor_lock_);
  std::function<void(::util::Status)> update_callback_
      GUARDED_BY(monitor_lock_);
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
