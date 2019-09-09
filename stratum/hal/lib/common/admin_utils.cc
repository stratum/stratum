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


#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <openssl/sha.h>
#include <openssl/md5.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <utility>
#include <regex>  // NOLINT
#include <sstream>
#include <thread>  // NOLINT

#include "stratum/hal/lib/common/admin_utils_interface.h"
#include "stratum/glue/logging.h"
#include "stratum/public/lib/error.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "absl/memory/memory.h"

namespace {

std::vector<std::string> splitLineByRegex(const std::string& s,
                                          const std::string& regex) {
  const std::regex re{regex};

  std::vector<std::string> ret{
      std::sregex_token_iterator(s.begin(), s.end(), re, -1),
      std::sregex_token_iterator()
  };

  return ret;
}

}  // namespace

namespace stratum {
namespace hal {

void AdminServiceShellHelper::ExecuteChild() {
  std::vector<char*> argv;

  // fill the argv with parts of cmdline
  for (const auto& arg : splitLineByRegex(command_, "\\ +")) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(NULL);

  if (!LinkPipesToStreams()) {
    exit(0);
  }

  int errno_copy = 0;
  if (execvp(argv.at(0), argv.data())) {
    errno_copy = errno;
    UnlinkPipes();
    LOG(ERROR) << "Can't run the command, error: "
               << std::string(std::strerror(errno_copy));
  }
  exit(errno_copy);
}

std::vector<std::string> AdminServiceShellHelper::FlushPipe(int pipe_fd) {
  const size_t buffer_size = 128;
  std::vector<std::string> ret;

  std::stringstream result;
  std::array<char, buffer_size> buffer{};

  while (true) {
    auto bytes_read = read(pipe_fd, buffer.data(), buffer_size-1);
    if (bytes_read <= 0) {
      break;
    }

    buffer[bytes_read] = '\0';
    result << buffer.data();
  }

  std::string line;
  while (std::getline(result, line)) {
    ret.push_back(line);
  }

  return ret;
}

void AdminServiceShellHelper::ExecuteParent() {
  close(stdout_fd_[1]);
  close(stderr_fd_[1]);

  // Wait for child and get the exit status
  int status;
  wait(&status);
  if (WIFEXITED(status)) {
    cmd_return_code_ = WEXITSTATUS(status);

    cmd_stdout_ = FlushPipe(stdout_fd_[0]);
    cmd_stderr_ = FlushPipe(stderr_fd_[0]);
  } else {
    cmd_return_code_ = -1;
    LOG(ERROR) << "Unexpected exit of the child process";
  }

  close(stdout_fd_[0]);
  close(stderr_fd_[0]);
}

bool AdminServiceShellHelper::LinkPipesToStreams() {
  stdout_back_up_ = dup(STDOUT_FILENO);
  stderr_back_up_ = dup(STDERR_FILENO);

  if (stdout_fd_[1] != STDOUT_FILENO ||
      close(stdout_fd_[0]) ||
      close(stdout_fd_[1])) {
    if (dup2(stdout_fd_[1], STDOUT_FILENO) != STDOUT_FILENO) {
      LOG(ERROR) << "Failed to link stdout pipe";
      return false;
    }
  } else {
    return false;
  }

  if (stderr_fd_[1] != STDERR_FILENO ||
      close(stderr_fd_[0]) ||
      close(stderr_fd_[1])) {
    if (dup2(stderr_fd_[1], STDERR_FILENO) != STDERR_FILENO) {
      LOG(ERROR) << "Failed to link stderr pipe";
      return false;
    }
  } else {
    return false;
  }

  return true;
}

void AdminServiceShellHelper::UnlinkPipes()  {
  if (stdout_back_up_ != STDOUT_FILENO || close(stdout_fd_[1])) {
    if (dup2(stdout_back_up_, STDOUT_FILENO) != STDOUT_FILENO) {
      LOG(ERROR) << "Failed to restore stdout descriptor";
    }
  }

  if (stderr_back_up_ != STDERR_FILENO || close(stderr_fd_[1])) {
    if (dup2(stderr_back_up_, STDERR_FILENO) != STDERR_FILENO) {
      LOG(ERROR) << "Failed to restore stderr descriptor";
    }
  }
}

bool AdminServiceShellHelper::Execute() {
  if (pipe(stdout_fd_)) {
    LOG(ERROR) << "Failed to open stdout pipe for cmd";
    return false;
  }
  if (pipe(stderr_fd_)) {
    LOG(ERROR) << "Failed to open stderr pipe for cmd";
    close(stdout_fd_[0]);
    close(stdout_fd_[1]);
    return false;
  }

  auto pid = fork();
  if (pid == 0) {
    // CHILD
    ExecuteChild();
  } else {
    // PARENT
    ExecuteParent();
  }

  return cmd_return_code_ == 0;
}

std::vector<std::string> AdminServiceShellHelper::GetStdout() {
  return cmd_stdout_;
}

std::vector<std::string> AdminServiceShellHelper::GetStderr() {
  return cmd_stderr_;
}

int AdminServiceShellHelper::GetReturnCode() {
  return cmd_return_code_;
}

std::shared_ptr<AdminServiceShellHelper>
AdminServiceUtilsInterface::GetShellHelper(const std::string& command) {
  return std::make_shared<AdminServiceShellHelper>(command);
}

uint64_t AdminServiceUtilsInterface::GetTime() {
  auto now = std::chrono::system_clock::now();
  auto duration = now.time_since_epoch();
  return static_cast<uint64_t>(duration.count());
}

std::shared_ptr<FileSystemHelper>
AdminServiceUtilsInterface::GetFileSystemHelper() {
  return file_system_helper_;
}

bool FileSystemHelper::PathExists(const std::string& path) const {
  return ::stratum::PathExists(path);
}

std::string FileSystemHelper::CreateTempDir() const {
  char dir_name_template[] = "/tmp/stratumXXXXXX";
  auto tmp_dir_name = mkdtemp(dir_name_template);
  if (tmp_dir_name == nullptr) {
    LOG(ERROR) << "Can't create temporary directory. Error: "
               << std::string(std::strerror(errno));
    // TODO(unknown): throw some exception
    return std::string("/tmp");
  }
  return tmp_dir_name;
}

std::string FileSystemHelper::TempFileName(std::string path) const {
  if (path.empty()) {
    path = CreateTempDir();
  }
  return path + std::string("/temp_file");
}

bool FileSystemHelper::CheckHashSumFile(
    const std::string& path,
    const std::string& old_hash,
    ::gnoi::types::HashType_HashMethod method) const {

  std::ifstream istream(path, std::ios::binary);
  return old_hash == GetHashSum(istream, method);
}

std::string FileSystemHelper::GetHashSum(
    std::istream& istream,
    ::gnoi::types::HashType_HashMethod method) const {

  const int BUFFER_SIZE = 1024;
  std::vector<char> buffer(BUFFER_SIZE, 0);
  unsigned char* hash = nullptr;
  size_t digest_len = 0;
  switch (method) {
    case ::gnoi::types::HashType_HashMethod_SHA256: {
      digest_len = SHA256_DIGEST_LENGTH;
      unsigned char hash_SHA256[SHA256_DIGEST_LENGTH];
      SHA256_CTX sha256;
      SHA256_Init(&sha256);
      while (istream.good()) {
        istream.read(buffer.data(), BUFFER_SIZE);
        SHA256_Update(&sha256, buffer.data(), istream.gcount());
      }

      SHA256_Final(hash_SHA256, &sha256);
      hash = hash_SHA256;
      break;
    }
    case ::gnoi::types::HashType_HashMethod_SHA512: {
      digest_len = SHA512_DIGEST_LENGTH;
      unsigned char hash_SHA512[SHA512_DIGEST_LENGTH];
      SHA512_CTX sha512;
      SHA512_Init(&sha512);
      while (istream.good()) {
        istream.read(buffer.data(), BUFFER_SIZE);
        SHA512_Update(&sha512, buffer.data(), istream.gcount());
      }

      SHA512_Final(hash_SHA512, &sha512);
      hash = hash_SHA512;
      break;
    }
    case ::gnoi::types::HashType_HashMethod_MD5: {
      digest_len = MD5_DIGEST_LENGTH;
      unsigned char hash_MD5[MD5_DIGEST_LENGTH];
      MD5_CTX md5;
      MD5_Init(&md5);
      while (istream.good()) {
        istream.read(buffer.data(), BUFFER_SIZE);
        MD5_Update(&md5, buffer.data(), istream.gcount());
      }

      MD5_Final(hash_MD5, &md5);
      hash = hash_MD5;
      break;
    }
    case ::gnoi::types::HashType_HashMethod_UNSPECIFIED: {
      LOG(WARNING) << "HashType_HashMethod_UNSPECIFIED";
      return std::string();
    }
    default:break;
  }

  // conver char array to hexstring
  std::stringstream ss;
  for (uint i = 0; i < digest_len; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0') <<
    static_cast<int>(hash[i]);
  }
  return ss.str();
}

::util::Status FileSystemHelper::StringToFile(
    const std::string& data,
    const std::string& file_name,
    bool append) const {

  return ::stratum::WriteStringToFile(data, file_name, append);
}

::util::Status FileSystemHelper::CopyFile(
    const std::string& src,
    const std::string& dst) const {

  std::ofstream outfile;
  std::ifstream infile;

  outfile.open(dst.c_str(), std::fstream::trunc | std::fstream::binary);
  infile.open(src.c_str(), std::fstream::binary);

  if (!outfile.is_open()) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when opening " << dst << ".";
  }

  if (!infile.is_open()) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when opening " << src << ".";
  }

  outfile << infile.rdbuf();

  outfile.close();
  infile.close();

  return ::util::OkStatus();
}

::util::Status FileSystemHelper::RemoveDir(const std::string& path) const {
  CHECK_RETURN_IF_FALSE(!path.empty());
  CHECK_RETURN_IF_FALSE(PathExists(path)) << path << " does not exist.";
  CHECK_RETURN_IF_FALSE(IsDir(path)) << path << " is not a dir.";
  // TODO(unknown): Is Dir Empty ?
  int ret = remove(path.c_str());
  if (ret != 0) {
    return MAKE_ERROR(ERR_INTERNAL)
        << "Failed to remove '" << path << "'. Return value: " << ret << ".";
  }
  return ::util::OkStatus();
}

::util::Status FileSystemHelper::RemoveFile(const std::string& path) const {
  return ::stratum::RemoveFile(path);
}

// Reboot the system
::util::Status AdminServiceUtilsInterface::Reboot() {
  int reboot_return_val = reboot(RB_AUTOBOOT);
  // Return failure if reboot was not successful
  if (reboot_return_val != 0) {
    LOG(ERROR) << "Failed to reboot the system: " << strerror(errno);
    MAKE_ERROR(ERR_INTERNAL)
        << "Failed to reboot the system: " << strerror(errno);
  }
  return ::util::OkStatus();
}

}  // namespace hal
}  // namespace stratum
