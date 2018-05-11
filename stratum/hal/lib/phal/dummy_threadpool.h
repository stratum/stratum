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


#ifndef STRATUM_HAL_LIB_PHAL_DUMMY_THREADPOOL_H_
#define STRATUM_HAL_LIB_PHAL_DUMMY_THREADPOOL_H_

#include <functional>
#include <map>
#include <vector>

#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/hal/lib/phal/threadpool_interface.h"
#include "third_party/absl/base/thread_annotations.h"
#include "third_party/absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace phal {

// An extremely elegant threadpool that executes all tasks serially.
// TODO: Make or find a real threadpool.
class DummyThreadpool : public ThreadpoolInterface {
 public:
  void Start() override;
  TaskId Schedule(std::function<void()> closure) override;
  void WaitAll(const std::vector<TaskId>& tasks) override;

 private:
  absl::Mutex lock_;
  std::map<TaskId, std::function<void()>> closures_ GUARDED_BY(lock_){};
  TaskId id_counter_ GUARDED_BY(lock_) = 0;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_DUMMY_THREADPOOL_H_
