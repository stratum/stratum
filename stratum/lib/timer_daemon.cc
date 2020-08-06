// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/lib/timer_daemon.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {

namespace {
// A function that is executed by the thread created by TimerDaemon::Start().
// It provides a timer resolution of 1ms.
static void* Timer(void* arg) {
  while (true) {
    // Sleep for 1ms.
    absl::SleepFor(absl::Milliseconds(1));

    // Call the method that will process the list of timers.
    // If the method returns 'false' it means that this thread should be
    // terminated.
    if (!TimerDaemon::Execute()) break;
  }
  return nullptr;
}
}  // namespace

bool TimerDaemon::IsStopped() {
  absl::WriterMutexLock l(&access_lock_);
  return !started_;
}

TimerDaemon::DescriptorPtr TimerDaemon::GetAction() {
  // Once woken up check if the timer at the top of the heap is due.
  absl::WriterMutexLock l(&access_lock_);

  if (timers_.empty()) {
    return nullptr;
  }
  if (timers_.front().expired()) {
    pop_heap(timers_.begin(), timers_.end(), TimerDescriptorComparator());
    timers_.pop_back();
    return nullptr;
  }
  TimerDaemon::DescriptorPtr front = DescriptorPtr(timers_.front());
  absl::Time now = absl::Now();
  if (front->due_time_ <= now) {
    pop_heap(timers_.begin(), timers_.end(), TimerDescriptorComparator());
    timers_.pop_back();
    if (front->Repeat()) {
      // Periodic timer. Insert it in the queue again.
      front->due_time_ += front->Period();
      timers_.push_back(front);
      push_heap(timers_.begin(), timers_.end(), TimerDescriptorComparator());
    }
    return front;
  }
  return nullptr;
}

bool TimerDaemon::Execute() {
  TimerDaemon* daemon = GetInstance();

  if (daemon->IsStopped()) return false;

  auto front = daemon->GetAction();
  if (front != nullptr) {
    // Execute the timer's action!
    const auto& status = front->ExecuteAction();
    if (status.ok()) {
      VLOG(1) << "Timer has been triggered!";
    } else {
      LOG(ERROR) << "Error executing action: " << status;
    }
  }
  return true;
}

::util::Status TimerDaemon::Start() {
  absl::WriterMutexLock l(&GetInstance()->access_lock_);
  if (GetInstance()->started_ == true) {
    return ::util::OkStatus();
  }

  GetInstance()->started_ = true;

  if (pthread_create(&GetInstance()->tid_, nullptr, &Timer, nullptr) != 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "Failed to create the timer thread.";
  } else {
    VLOG(1) << "The timer daemon has been started.";
    return ::util::OkStatus();
  }
}

::util::Status TimerDaemon::Stop() {
  {
    absl::WriterMutexLock l(&GetInstance()->access_lock_);
    GetInstance()->started_ = false;
  }

  if (pthread_join(GetInstance()->tid_, nullptr) != 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "Failed to join the timer thread.";
  } else {
    absl::WriterMutexLock l(&GetInstance()->access_lock_);
    GetInstance()->timers_.clear();
    GetInstance()->tid_ = 0;

    VLOG(1) << "The timer daemon has been stopped.";
    return ::util::OkStatus();
  }
}

::util::Status TimerDaemon::RequestOneShotTimer(uint64 delay_ms,
                                                 const Action& action,
                                                 DescriptorPtr* desc) {
  return GetInstance()->RequestTimer(false, delay_ms,
                                     /* period_ms (ignored)= */ 0, action,
                                     desc);
}

::util::Status TimerDaemon::RequestPeriodicTimer(uint64 delay_ms,
                                                  uint64 period_ms,
                                                  const Action& action,
                                                  DescriptorPtr* desc) {
  return GetInstance()->RequestTimer(true, delay_ms, period_ms, action, desc);
}

::util::Status TimerDaemon::RequestTimer(bool repeat, uint64 delay_ms,
                                          uint64 period_ms, Action action,
                                          DescriptorPtr* desc) {
  absl::WriterMutexLock l(&access_lock_);

  VLOG(1) << "Registered timer.";

  absl::Time now = absl::Now();
  *desc = std::make_shared<Descriptor>(repeat, action);
  (*desc)->due_time_ = now + absl::Milliseconds(delay_ms);
  (*desc)->period_ = absl::Milliseconds(period_ms);
  timers_.push_back(DescriptorWeakPtr(*desc));
  push_heap(timers_.begin(), timers_.end(), TimerDescriptorComparator());

  return ::util::OkStatus();
}

}  // namespace hal
}  // namespace stratum
