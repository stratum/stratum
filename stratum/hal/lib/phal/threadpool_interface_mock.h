// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_MOCK_H_

#include <vector>

#include "gmock/gmock.h"
#include "stratum/hal/lib/phal/threadpool_interface.h"

namespace stratum {
namespace hal {
namespace phal {

class ThreadpoolInterfaceMock : public ThreadpoolInterface {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(Schedule, TaskId(std::function<void()> closure));
  MOCK_METHOD1(WaitAll, void(const std::vector<TaskId>& threads));
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_MOCK_H_
