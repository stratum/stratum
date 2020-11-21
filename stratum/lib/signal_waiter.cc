// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/signal_waiter.h"

#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

namespace {

// Signal received callback which is registered as the handler for signals using
// signal() system call.
void SignalRcvCallback(int value) { SignalWaiter::HandleSignal(value); }

}  // namespace

// Initialize static member.
SignalWaiter SignalWaiter::instance_;

SignalWaiter::SignalWaiter(const std::vector<int> signals) : signals_(signals) {
  CHECK_ERR(sem_init(&sem_, 0, 0));
  // Register the signal handlers and save the old handlers as well.
  for (const int s : signals) {
    sighandler_t h = signal(s, SignalRcvCallback);
    if (h == SIG_ERR) {
      LOG(FATAL) << "Failed to register signal: " << strsignal(s) << " (" << s
                 << ").";
    }
    old_signal_handlers_[s] = h;
  }
}

SignalWaiter::~SignalWaiter() {
  // Register the old handlers for all the signals.
  for (const auto& e : old_signal_handlers_) {
    signal(e.first, e.second);
  }

  CHECK_ERR(sem_destroy(&sem_));
}

// Reminder: async-signal-safe function calls only!
::util::Status SignalWaiter::HandleSignal(int value) {
  const auto& s = instance_.signals_;
  if (std::find(s.begin(), s.end(), value) == s.end()) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Tried to handle unregistered signal: " << strsignal(value) << " ("
        << value << ").";
  }

  // Wake up one thread waiting for a signal.
  CHECK_ERR(sem_post(&instance_.sem_));
  return ::util::OkStatus();
}

void SignalWaiter::WaitForSignal() {
  while (sem_wait(&instance_.sem_) != 0) {
    // Fail if not EINTR; otherwise continue waiting.
    PCHECK(errno == EINTR);
  }

  // Wake up another thread that is waiting for a signal.
  CHECK_ERR(sem_post(&instance_.sem_));
}

}  // namespace hal
}  // namespace stratum
