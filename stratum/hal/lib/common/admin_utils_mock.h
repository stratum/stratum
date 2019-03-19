// Copyright 2018 Google LLC
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

#ifndef STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_MOCK_H_

#include "admin_utils_interface.h"

#include <string>
#include "gmock/gmock.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {

class AdminServiceShellHelperMock : public AdminServiceShellHelper {
 public:
  AdminServiceShellHelperMock(const std::string& s)
      : AdminServiceShellHelper(s) {}
  MOCK_METHOD0(Execute, bool());
  MOCK_METHOD0(GetStdout, std::vector<std::string>());
  MOCK_METHOD0(GetStderr, std::vector<std::string>());
  MOCK_METHOD0(GetReturnCode, int());
};

class AdminServiceUtilsInterfaceMock : public AdminServiceUtilsInterface {
 public:
  std::shared_ptr<AdminServiceShellHelper>
  GetShellHelper(const std::string& s) override {
    return GetShellHelperProxy(s);
  }

  // Used to enable mocking the underlying ShellHelper
  // The test provides the object to be returned (and defines mock methods
  // on the object)
  MOCK_METHOD1(GetShellHelperProxy,
               std::shared_ptr<AdminServiceShellHelper>(const std::string &));

  MOCK_METHOD0(GetTime, uint64_t());
};

} // namespace hal
} // namespace stratum

#endif //STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_MOCK_H_
