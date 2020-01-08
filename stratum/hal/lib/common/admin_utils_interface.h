// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_INTERFACE_H_
#define STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_INTERFACE_H_

#include <string>
#include <memory>
#include <vector>

#include "types/types.pb.h"
#include "stratum/glue/status/status.h"
#include "system/system.grpc.pb.h"
#include "stratum/glue/integral_types.h"

namespace {  // NOLINT
const int ERROR_RETURN_CODE = -1;
}

namespace stratum {
namespace hal {


// Provides interface to call a shell command, and retrieve the results
class AdminServiceShellHelper {
 public:
  explicit AdminServiceShellHelper(const std::string& command)
      : command_(command),
        cmd_return_code_(
            ERROR_RETURN_CODE) {}

  // Runs the command provided in the constructor
  // @return true if cmd succeeded, false if failure
  virtual bool Execute();

  // Getters for the private stdout/stderr vectors
  virtual std::vector<std::string> GetStdout();
  virtual std::vector<std::string> GetStderr();

  // Gets the return code of the executed command
  int GetReturnCode();

 private:
  // Command to be executed (cmdline)
  std::string command_;
  // Return of the executed command, populated after cmd exits;
  // By default is zero
  int cmd_return_code_;
  std::vector<std::string> cmd_stdout_;
  std::vector<std::string> cmd_stderr_;

  // Storing the stdout/stderr descriptors after output re-targetting
  int stdout_back_up_;
  int stderr_back_up_;

  // Pipes for stdout/stderr from the child process
  int stdout_fd_[2];
  int stderr_fd_[2];

  // Retargets the streams of the process into the above pipes;
  // Closes the terminal descriptors but stores the current ones in the back_up_
  // @return false if failed, true otherwise
  bool LinkPipesToStreams();

  // Restores the stdout/stderr
  void UnlinkPipes();

  // Child portion after fork()
  //
  // Link pipes
  // Execute command
  // Notify on error
  void ExecuteChild();

  // Parent portion after fork()
  //
  // Wait for child
  // Check exit status
  // Retrieve the output
  void ExecuteParent();

  // Reads everything available on the file descriptor
  //
  // Will return empty lines as an empty string
  // @param pipe_fd File descriptor to read
  // @return List of lines without new_line character at the end
  std::vector<std::string> FlushPipe(int pipe_fd);
};

// Provides interface to filesystem
class FileSystemHelper {
 public:
  FileSystemHelper() = default;
  virtual ~FileSystemHelper() = default;

  virtual bool CheckHashSumFile(const std::string& path,
                              const std::string& old_hash,
                              ::gnoi::types::HashType_HashMethod method) const;

  virtual std::string GetHashSum(
      std::istream& istream, ::gnoi::types::HashType_HashMethod method) const;

  // Create temporary directory and return it name
  // @return Temporary directory name in std:string
  virtual std::string CreateTempDir() const;

  // Create temporary file and return it name
  // @return Temporary file name in std:string
  virtual std::string TempFileName(std::string path = std::string()) const;

  // Removes a dir from the given path.
  // Returns error if the path does not exist
  // or the path is a file.
  virtual ::util::Status RemoveDir(const std::string& path) const;

  virtual ::util::Status RemoveFile(const std::string& path) const;

  virtual bool PathExists(const std::string& path) const;

  virtual ::util::Status CopyFile(const std::string& src,
                                  const std::string& dst) const;

  virtual ::util::Status StringToFile(const std::string& data,
                                      const std::string& file_name,
                                      bool append = false) const;
};

// Wrapper/fabric class for the Admin Service utils;
// To use, retrieve the desired object via Get___Helper
class AdminServiceUtilsInterface {
 public:
  AdminServiceUtilsInterface() : file_system_helper_(new FileSystemHelper) {}
  virtual ~AdminServiceUtilsInterface() = default;

  virtual std::shared_ptr<AdminServiceShellHelper>
  GetShellHelper(const std::string& command);

  // Retrieves time since epoch in ns
  virtual uint64_t GetTime();

  virtual std::shared_ptr<FileSystemHelper>
  GetFileSystemHelper();

  // Reboot the system
  virtual ::util::Status Reboot();

 private:
  std::shared_ptr<FileSystemHelper> file_system_helper_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_ADMIN_UTILS_INTERFACE_H_
