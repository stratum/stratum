// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_H_
#define STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_H_

#include <functional>
#include <vector>

#include "stratum/glue/status/status.h"
#include "stratum/glue/integral_types.h"

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
