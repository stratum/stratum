// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_MOCK_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "stratum/hal/lib/common/admin_utils_interface.h"

namespace stratum {
namespace hal {

class AdminServiceShellHelperMock : public AdminServiceShellHelper {
 public:
  explicit AdminServiceShellHelperMock(const std::string& s)
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
                     bool(const std::string& path, const std::string& old_hash,
                          ::gnoi::types::HashType_HashMethod method));

  MOCK_CONST_METHOD2(GetHashSum,
                     std::string(std::istream& istream,
                                 ::gnoi::types::HashType_HashMethod method));

  MOCK_CONST_METHOD0(CreateTempDir, std::string());

  MOCK_CONST_METHOD1(TempFileName, std::string(std::string path));

  MOCK_CONST_METHOD1(RemoveDir, ::util::Status(const std::string& path));

  MOCK_CONST_METHOD1(RemoveFile, ::util::Status(const std::string& path));

  MOCK_CONST_METHOD1(PathExists, bool(const std::string& path));

  MOCK_CONST_METHOD2(CopyFile, ::util::Status(const std::string& src,
                                              const std::string& dst));

  MOCK_CONST_METHOD3(StringToFile,
                     ::util::Status(const std::string& data,
                                    const std::string& filename, bool append));
};

class AdminServiceUtilsInterfaceMock : public AdminServiceUtilsInterface {
 public:
  std::shared_ptr<AdminServiceShellHelper> GetShellHelper(
      const std::string& s) override {
    return GetShellHelperProxy(s);
  }

  // Used to enable mocking the underlying ShellHelper
  // The test provides the object to be returned (and defines mock methods
  // on the object)
  MOCK_METHOD1(GetShellHelperProxy,
               std::shared_ptr<AdminServiceShellHelper>(const std::string&));
  MOCK_METHOD0(GetFileSystemHelper, std::shared_ptr<FileSystemHelper>());
  MOCK_METHOD0(GetTime, uint64_t());
  MOCK_METHOD0(Reboot, ::util::Status());
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_MOCK_H_
