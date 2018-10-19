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


#ifndef STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_

#include "stratum/hal/lib/phal/udev_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class UdevMock : public UdevInterface {
 public:
  MOCK_METHOD1(Initialize, ::util::Status(const std::string& filter));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD0(Check, ::util::StatusOr<std::pair<std::string, std::string>>());
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_UDEV_MOCK_H_
