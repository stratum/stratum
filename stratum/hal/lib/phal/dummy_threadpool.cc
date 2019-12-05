// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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
