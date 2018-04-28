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


#ifndef STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_H_

#include <functional>
#include <vector>

#include "third_party/stratum/glue/status/status.h"
#include "third_party/absl/base/integral_types.h"

namespace stratum {
namespace hal {
namespace phal {

typedef uint32 TaskId;

class ThreadpoolInterface {
 public:
  virtual ~ThreadpoolInterface() {}
  // Setup and start any internal structures (i.e. threads).
  virtual void Start() = 0;
  // Schedule a single task to execute, and return a TaskId for the new task.
  virtual TaskId Schedule(std::function<void()> closure) = 0;
  // Block until all tasks with the given TaskIds have completed. Any TaskIds
  // that have no matching task are ignored.
  virtual void WaitAll(const std::vector<TaskId>& tasks) = 0;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_H_
