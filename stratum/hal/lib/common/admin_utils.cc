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

#include "stratum/hal/lib/common/admin_utils_interface.h"

#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <utility>
#include <regex>
#include <sstream>
#include <thread>

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

}

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

} // namespace hal
} // namespace stratum
