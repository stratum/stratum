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


#ifndef STRATUM_LIB_TIMER_DAEMON_H_
#define STRATUM_LIB_TIMER_DAEMON_H_

#include <pthread.h>
#include <algorithm>
#include <functional>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"
#include "absl/public/lib/error.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace stratum {
namespace hal {

class TimerDaemon final {
 private:
  using Action = std::function<::util::Status()>;

  class Descriptor {
   public:
    explicit Descriptor(const Action& action)
        : repeat_(true), period_(absl::Seconds(1)), action_(action) {}
    explicit Descriptor(bool repeat, const Action& action)
        : repeat_(repeat), period_(absl::Seconds(1)), action_(action) {}
    ~Descriptor() {}
    bool Repeat() { return repeat_; }
    absl::Duration Period() { return period_; }
    ::util::Status ExecuteAction() { return action_(); }

    bool repeat_;
    absl::Time due_time_;
    absl::Duration period_;

   private:
    Action action_ = []() {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL) << "Noop timer action!";
      return error;
    };
  };

  using DescriptorWeakPtr = std::weak_ptr<Descriptor>;

  // In order to use TimerDaemon::Descriptor as a key in heap operations a
  // comparator functor has to be defined.
  struct TimerDescriptorComparator {
    bool operator()(const TimerDaemon::DescriptorWeakPtr& lhs,
                    const TimerDaemon::DescriptorWeakPtr& rhs) const {
      // The goal of this method is to find which timer descriptor describes a
      // timer that should be executed earlier: lhs or rhs?
      // If a timer is canceled then the weak pointer pointing to it will
      // 'expire' and there is no point in keeping such pointer on the heap. As
      // the most effective way of removing an element from a heap is to move it
      // to the top and then delete it, expired pointers should be theated as
      // ones that are to be executed 'now'. This approach makes them to
      // 'bubbled up' to the top as quickly as possible and then they can be
      // removed.
      if (lhs.expired() && !rhs.expired()) {
        // rhs is expired, so, it is the lowest number.
        return true;
      }
      if (rhs.expired() && !lhs.expired()) {
        // lhs is expired, so, it is the lowest number.
        return false;
      }
      if (rhs.expired() && lhs.expired()) {
        // Both are expired, so, it really does not matter what we return.
        return false;
      }
      // Both elements are still valid. Compare them!
      return std::shared_ptr<Descriptor>(lhs)->due_time_ >
             std::shared_ptr<Descriptor>(rhs)->due_time_;
    }
  };

 public:
  using DescriptorPtr = std::shared_ptr<Descriptor>;

  // Starts the timer service. Creates a thread that calls Execute() every 1ms.
  static ::util::Status Start() LOCKS_EXCLUDED(access_lock_);
  // Stops the timer service. Notifies the timer thread to exit and waits until
  // it joins.
  static ::util::Status Stop() LOCKS_EXCLUDED(access_lock_);
  // The 'worker' of the timer service. Is called every 1ms and checks if the
  // first timer to be executed should be executed. If so, the action is
  // executed and then if the timer is periodic the timer is updated and
  // re-inserted into the heap with timers. It also takes care of all expired
  // timers.
  static bool Execute() LOCKS_EXCLUDED(access_lock_);

  // Creates a one-shot timer that will execute 'action' 'delay_ms' milliseconds
  // from now.
  static ::util::Status RequestOneShotTimer(uint64 delay_ms,
                                            const Action& action,
                                            DescriptorPtr* desc);
  // Creates a periodic timier that will first time execute the 'action'
  // 'delay_ms' milliseconds from now and then will execute the 'action' every
  // 'period_ms' missilseconds.
  static ::util::Status RequestPeriodicTimer(uint64 delay_ms, uint64 period_ms,
                                             const Action& action,
                                             DescriptorPtr* desc);

 private:
  TimerDaemon() : started_(false) {}

  // If there is an action to be executed at this moment of time a non-null
  // pointer is returned.
  DescriptorPtr GetAction();

  // Returns true if the timer daemon is stopped.
  bool IsStopped();

  static TimerDaemon* GetInstance() {
    static TimerDaemon* singleton = new TimerDaemon();

    return singleton;
  }
  // Internal method creating requested timer.
  ::util::Status RequestTimer(bool repeat, uint64 delay_ms, uint64 period_ms,
                              Action action, DescriptorPtr* desc)
      LOCKS_EXCLUDED(access_lock_);

  // A Mutex used to guard access to the list of pointers to timer requests and
  // the started_ flag.
  mutable absl::Mutex access_lock_;

  std::vector<DescriptorWeakPtr> timers_ GUARDED_BY(access_lock_);

  pthread_t tid_ = 0;  // will not be destroyed before the thread is joined.

  bool started_ GUARDED_BY(access_lock_);

  friend class TimerDaemonTest;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_LIB_TIMER_DAEMON_H_
