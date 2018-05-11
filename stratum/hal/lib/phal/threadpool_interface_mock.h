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


#ifndef STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_THREADPOOL_INTERFACE_MOCK_H_

#include "third_party/stratum/hal/lib/phal/threadpool_interface.h"
#include "testing/base/public/gmock.h"

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
