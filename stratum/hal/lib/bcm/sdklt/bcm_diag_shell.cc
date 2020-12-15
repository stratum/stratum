// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_diag_shell.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <limits.h>
#include <pty.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "absl/base/macros.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"

DEFINE_int32(bcm_diag_shell_port, 5020,
             "Port to listen to for user telnet sessions.");

namespace stratum {
namespace hal {
namespace bcm {

BcmDiagShell::BcmDiagShell()
    : server_started_(false), server_thread_id_(0), shell_thread_id_(0) {}

BcmDiagShell::~BcmDiagShell() {}

// Initialization of the static vars.
constexpr unsigned char BcmDiagShell::kTelnetWillSGA[];
constexpr unsigned char BcmDiagShell::kTelnetWillEcho[];
constexpr unsigned char BcmDiagShell::kTelnetDontEcho[];
BcmDiagShell* BcmDiagShell::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BcmDiagShell::init_lock_(absl::kConstInit);

::util::Status BcmDiagShell::StartServer() {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

pthread_t BcmDiagShell::GetDiagShellThreadId() const {
  absl::ReaderMutexLock l(&shell_lock_);
  return shell_thread_id_;
}

BcmDiagShell* BcmDiagShell::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BcmDiagShell();
  }

  return singleton_;
}

void* BcmDiagShell::ServerThreadFunc(void* arg) {
  BcmDiagShell* bcm_diag_shell = static_cast<BcmDiagShell*>(arg);
  bcm_diag_shell->RunServer();
  return nullptr;
}

void* BcmDiagShell::ShellThreadFunc(void* arg) {
  BcmDiagShell* bcm_diag_shell = static_cast<BcmDiagShell*>(arg);
  bcm_diag_shell->RunDiagShell();
  return nullptr;
}

// The contents of the rest of these functions are almost identical to the
// corresponding functions in bcm_sdk_manager.cc under stack/hal/lib/bcm.
// These part is working as expected and there is not reason or intend to
// change anything in them.
void BcmDiagShell::RunServer() {
  // TODO(unknown): Implement this function.
}

void BcmDiagShell::RunDiagShell() {
  // TODO(unknown): Implement this function.
}

void BcmDiagShell::ForwardTelnetSession() {
  fd_set read_fds;
  int max_fd1 =
      (client_socket_ > pty_master_fd_ ? client_socket_ : pty_master_fd_) + 1;
  unsigned char pty_buffer[kNumberOfBytesRead + 1];
  int bytes;

  while (true) {
    FD_ZERO(&read_fds);
    FD_SET(client_socket_, &read_fds);
    FD_SET(pty_master_fd_, &read_fds);
    if (select(max_fd1, &read_fds, nullptr, nullptr, nullptr) <= 0) {
      LOG(ERROR) << "Failure in select(): " << strerror(errno);
      break;
    }
    if (FD_ISSET(client_socket_, &read_fds)) {
      // forward data from telnet to pty.
      if (ProcessTelnetInput() < 0) {
        // client has closed the telnet session.
        break;
      }
    }
    if (FD_ISSET(pty_master_fd_, &read_fds)) {
      bytes = read(pty_master_fd_, pty_buffer, kNumberOfBytesRead);
      if (bytes <= 0) {
        // pty close by diag shell.
        break;
      } else {
        // forward data to client.
        WriteToTelnetClient(pty_buffer, bytes);
      }
    }
  }
}

void BcmDiagShell::ProcessTelnetCommand() {
  unsigned char command[3] = {kTelnetCmd, 0, 0};
  std::string info = "BcmDiagShell: received TelnetCmd ";

  if (ReadNextTelnetCommandByte(&command[1]) != 1) {
    LOG(ERROR) << "Received incomplete telnet command.";
    return;
  }

  // we only support Echo and SGA options, so negate other options.
  switch (command[1]) {
    case kTelnetWill:
      info += "WILL ";
      command[1] = kTelnetDont;
      break;
    case kTelnetWont:
      info += "WONT ";
      command[1] = kTelnetDont;
      break;
    case kTelnetDo:
      info += "DO ";
      command[1] = kTelnetWont;
      break;
    case kTelnetDont:
      info += "DONT ";
      command[1] = kTelnetWont;
      break;
    default:
      VLOG(1) << "Received 2 character telnet command.";
      return;
  }

  if (ReadNextTelnetCommandByte(&command[2]) != 1) {
    LOG(ERROR) << "Received incomplete telnet command.";
    return;
  }

  // ignore response to our own commands
  switch (command[2]) {
    case kTelnetEcho:
      VLOG(1) << info << "ECHO.";
      return;
    case kTelnetSGA:
      VLOG(1) << info << "SGA.";
      return;
  }

  // send the negated response to pty
  WriteToPtyMaster(command, 3);
}

int BcmDiagShell::ProcessTelnetInput() {
  // read from telnet session.
  net_buffer_count_ = read(client_socket_, net_buffer_, kNumberOfBytesRead);
  data_start_ = net_buffer_offset_ = 0;

  if (net_buffer_count_ <= 0) {
    WriteToPtyMaster("quit\n", 6);
    // This doesn't actually make sh_process exit.
    // It may have some side-effects, so we'll leave it here.
    // closing pty_master_fd_ is the real cause to make sh_process exit.
    return -1;
  }

  // check for telnet command and process it.
  while (net_buffer_offset_ < net_buffer_count_) {
    if (net_buffer_[net_buffer_offset_] == kTelnetCmd) {
      SendTelnetDataToPty();
      net_buffer_offset_++;
      ProcessTelnetCommand();
      data_start_ = net_buffer_offset_;
    } else {
      net_buffer_offset_++;
    }
  }

  SendTelnetDataToPty();
  return 0;
}

// either read from telnet buffer or from telnet session.  Reading from telnet
// session should not be blocked, assuming integrity of telnet client.
int BcmDiagShell::ReadNextTelnetCommandByte(unsigned char* data) {
  if (net_buffer_offset_ < net_buffer_count_) {
    *data = net_buffer_[net_buffer_offset_++];
    return 1;
  } else {
    return read(client_socket_, data, 1);
  }
}

// forward data in telnet buffer to pty.
void BcmDiagShell::SendTelnetDataToPty() {
  if (data_start_ < net_buffer_offset_) {
    WriteToPtyMaster(&net_buffer_[data_start_],
                     net_buffer_offset_ - data_start_);
  }
}

void BcmDiagShell::WriteToTelnetClient(const void* data, size_t size) {
  // Set MSG_NOSIGNAL flag to igonre SIGPIPE. b/6362602
  if (send(client_socket_, data, size, MSG_NOSIGNAL) < 0) {
    VLOG(1) << "Failed to send data to the telnet client: " << strerror(errno);
  }
}

void BcmDiagShell::WriteToPtyMaster(const void* data, size_t size) {
  if (write(pty_master_fd_, data, size) < 0) {
    VLOG(1) << "Failed to send data to the pty master: " << strerror(errno);
  }
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
