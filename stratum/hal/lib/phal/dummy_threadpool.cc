// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/dummy_threadpool.h"

#include <utility>

#include "absl/synchronization/mutex.h"
#include "stratum/hal/lib/phal/threadpool_interface.h"

namespace stratum {
namespace hal {
namespace phal {

void DummyThreadpool::Start() {
  // Start our one thread... Done!
}
TaskId DummyThreadpool::Schedule(std::function<void()> closure) {
  TaskId id;
  absl::MutexLock lock(&lock_);
  id = id_counter_++;
  // Loop through possible task id until we find one that isn't currently taken.
  // If this ever fails to find a unique id (i.e. we overflow int32),
  // DummyThreadpool is long overdue for replacement by a real threadpool.
  while (closures_.find(id) != closures_.end()) id = id_counter_++;
  closures_.insert(std::make_pair(id, closure));
  return id;
}
void DummyThreadpool::WaitAll(const std::vector<TaskId>& tasks) {
  std::vector<std::function<void()>> to_execute;
  {
    absl::MutexLock lock(&lock_);
    for (auto task : tasks) {
      auto closure = closures_.find(task);
      if (closure != closures_.end()) {
        to_execute.push_back(closure->second);
        closures_.erase(closure);
      }
    }
  }
  for (const auto& closure : to_execute) closure();
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
