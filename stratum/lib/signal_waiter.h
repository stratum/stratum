// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_SIGNAL_WAITER_H_
#define STRATUM_LIB_SIGNAL_WAITER_H_

#include <semaphore.h>
#include <signal.h>

#include <atomic>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "stratum/glue/status/status.h"

namespace stratum {
namespace hal {

// Class 'SignalWaiter' is a utility class that waits allows calling threads to
// wait on a set of signals. This class is initialized statically on program
// start and registers handlers to listen for SIGINT, SIGTERM, and SIGUSR2.
// Users can also call HandleSignal() to deliver a signal to waiters instead of
// kill() or pthread_kill().
class SignalWaiter final {
 public:
  // Called by the signal handler when it receives a signal.
  // This function is async-signal-safe and can only call other
  // async-signal-safe functions.
  static ::util::Status HandleSignal(int value);

  // Blocking call to wait for one of the signals. Returns when one of the
  // signals is received.
  static void WaitForSignal();

  // SignalWaiter is neither copyable nor movable.
  SignalWaiter(const SignalWaiter&) = delete;
  SignalWaiter& operator=(const SignalWaiter&) = delete;

  // Static instance that gets created at program start.
  static SignalWaiter instance_;

 private:
  // Private constructor and destructor.
  SignalWaiter(const std::vector<int> signals = {SIGINT, SIGTERM, SIGUSR2});
  ~SignalWaiter();

  // Semaphore that is used to block threads that call WaitForSignal(). The
  // semaphore is initialized locked (value 0). Waiting threads call sem_wait(),
  // and the signal handler calls sem_post(). The semaphone remains unlocked
  // (value >= 1) after a signal has been handled.
  sem_t sem_;

  // Vector of signals for which we registered handlers.
  std::vector<int> signals_;

  // Map from signals for which we registered handlers to their old handlers.
  // This map is used to restore the signal handlers to their previous state
  // in the class destructor.
  absl::flat_hash_map<int, sighandler_t> old_signal_handlers_;

  friend class SignalWaiterTest;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_LIB_SIGNAL_WAITER_H_
