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

class FileSystemHelperMock : public FileSystemHelper {
 public:
  FileSystemHelperMock() : FileSystemHelper() {
    ::testing::DefaultValue<::util::Status>::Set(::util::OkStatus());
  }

  MOCK_CONST_METHOD3(CheckHashSumFile,
                     bool(
                         const std::string& path,
                         const std::string& old_hash,
                         ::gnoi::HashType_HashMethod method));

  MOCK_CONST_METHOD2(GetHashSum,
                     std::string(
                         std::istream & istream,
                         ::gnoi::HashType_HashMethod method));

  MOCK_CONST_METHOD0(CreateTempDir, std::string());

  MOCK_CONST_METHOD1(TempFileName,
                     std::string(
                         std::string
                         path));

  MOCK_CONST_METHOD1(RemoveDir,
                     ::util::Status(
                         const std::string& path));

  MOCK_CONST_METHOD1(RemoveFile,
                     ::util::Status(
                         const std::string& path));

  MOCK_CONST_METHOD1(PathExists, bool(const std::string& path));

  MOCK_CONST_METHOD2(CopyFile,
                     ::util::Status(
                         const std::string& src,
                         const std::string& dst));

  MOCK_CONST_METHOD3(StringToFile,
                     ::util::Status(
                         const std::string& data,
                         const std::string& filename,
                         bool append));
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
  MOCK_METHOD0(GetFileSystemHelper, std::shared_ptr<FileSystemHelper>());
  MOCK_METHOD0(GetTime, uint64_t());
  MOCK_METHOD0(Reboot, ::util::Status());
};

} // namespace hal
} // namespace stratum

#endif //STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_MOCK_H_
