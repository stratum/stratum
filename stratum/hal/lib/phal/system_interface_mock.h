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


#ifndef STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_

#include "third_party/stratum/hal/lib/phal/system_interface.h"
#include "testing/base/public/gmock.h"

namespace stratum {
namespace hal {
namespace phal {

class MockSystemInterface : public SystemInterface {
 public:
  MockSystemInterface() : SystemInterface() {}

  MOCK_CONST_METHOD2(WriteStringToFile,
                     ::util::Status(const std::string& buffer,
                                    const std::string& path));
  MOCK_CONST_METHOD2(ReadFileToString, ::util::Status(const std::string& path,
                                                      std::string* buffer));
  MOCK_CONST_METHOD1(PathExists, bool(const std::string& path));
  MOCK_CONST_METHOD0(MakeUdev, ::util::StatusOr<std::unique_ptr<Udev>>());
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_SYSTEM_INTERFACE_MOCK_H_
