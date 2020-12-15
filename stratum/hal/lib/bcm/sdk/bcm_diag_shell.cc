// Copyright 2018-2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

/*
 * The Broadcom Switch API header code upon which this file depends is:
 * Copyright 2007-2020 Broadcom Inc.
 *
 * This file depends on Broadcom's OpenNSA SDK.
 * Additional license terms for OpenNSA are available from Broadcom or online:
 *     https://www.broadcom.com/products/ethernet-connectivity/software/opennsa
 */

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

extern "C" {
#include "stratum/hal/lib/bcm/sdk_build_undef.h"  // NOLINT
#include "sdk_build_flags.h"                      // NOLINT
// TODO(bocon) we might be able to prune some of these includes
#include "appl/diag/bslmgmt.h"
#include "appl/diag/opennsa_diag.h"
#include "bcm/error.h"
#include "bcm/init.h"
#include "bcm/knet.h"
#include "bcm/link.h"
#include "bcm/policer.h"
#include "bcm/port.h"
#include "bcm/stack.h"
#include "bcm/stat.h"
#include "bcm/types.h"
#include "kcom.h"  // NOLINT
#include "sal/core/libc.h"
#include "shared/bsl.h"
#include "soc/opensoc.h"
#include "stratum/hal/lib/bcm/sdk_build_undef.h"  // NOLINT
}

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
  // Grab the lock. Nobody should be able to call this function while we are
  // creating the thread.
  absl::WriterMutexLock l(&server_lock_);

  if (server_started_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "The diag shell server is already started.";
  }

  // set main thread to nullptr and let daemon thread become sdk main.
  sal_thread_main_set(nullptr);

  // Spawn the server thread.
  int ret = pthread_create(&server_thread_id_, nullptr,
                           &BcmDiagShell::ServerThreadFunc, this);
  if (ret != 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to spawn the diag shell server thread. Err: " << ret
           << ".";
  }

  server_started_ = true;

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
  int reuse_addr = 1;
  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(FLAGS_bcm_diag_shell_port);
  server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  // Initialize diag shell.
  diag_init();

  // daemon thread as sdk main thread
  sal_thread_main_set(sal_thread_self());

  // This loop will run forever until there is an error, or
  // the whole HAL process exits and takes this thread with it.
  while (true) {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr,
               sizeof(reuse_addr));
    if (bind(server_socket_, reinterpret_cast<struct sockaddr*>(&server),
             sizeof(server)) < 0) {
      LOG(ERROR) << "Cannot bind sockaddress to listening socket: "
                 << strerror(errno);
      close(server_socket_);
      sleep(1);
      continue;
    }
    listen(server_socket_, 1);
    client_socket_ = accept(server_socket_, nullptr, nullptr);
    if (client_socket_ < 0) {
      VLOG(1) << "Failed to accept client connection: " << strerror(errno);
      close(server_socket_);
      sleep(1);
      continue;
    }
    close(server_socket_);
    if (openpty(&pty_master_fd_, &pty_slave_fd_, nullptr, nullptr, nullptr) <
        0) {
      VLOG(1) << "Failure in openpty(): " << strerror(errno);
      close(client_socket_);
      sleep(1);
      continue;
    }

    // configure pty so that Control-C from control card will not kill linecard
    // switch HAL.  setting c_iflag or c_lflag is not enough, need to set
    // c_cc[VINTR] to a very unlikely value.
    struct termios termio;
    tcgetattr(pty_slave_fd_, &termio);
    termio.c_iflag &= ~BRKINT;
    termio.c_iflag |= IGNCR;
    termio.c_lflag &= ~ISIG;
    termio.c_cc[VINTR] = -1;
    tcsetattr(pty_slave_fd_, TCSANOW, &termio);

    // Redirect stdin/stdout to the telnet connection.
    // This will affect the whole LCHAL, but stdin/stdout in LCHAL aren't
    // intended for other purposes.
    int old_stdin = dup(stdin->_fileno);
    dup2(pty_slave_fd_, stdin->_fileno);
    int old_stdout = dup(stdout->_fileno);
    dup2(pty_slave_fd_, stdout->_fileno);
    close(pty_slave_fd_);

    {
      absl::WriterMutexLock l(&shell_lock_);
      if (pthread_create(&shell_thread_id_, nullptr, &ShellThreadFunc, this)) {
        VLOG(1) << "Failed to create diag shell thread!";
        return;
      }
    }

    // Force the telnet client enter character mode.
    WriteToTelnetClient(kTelnetWillSGA, sizeof(kTelnetWillSGA));
    WriteToTelnetClient(kTelnetWillEcho, sizeof(kTelnetWillEcho));
    WriteToTelnetClient(kTelnetDontEcho, sizeof(kTelnetDontEcho));

    // Start processing data from telnet client.
    ForwardTelnetSession();

    // Clean up.
    close(pty_master_fd_);
    {
      absl::WriterMutexLock l(&shell_lock_);
      pthread_join(shell_thread_id_, nullptr);
      shell_thread_id_ = 0;  // reset the thread id.
    }

    // Restore stdin/stdout.
    dup2(old_stdin, stdin->_fileno);
    dup2(old_stdout, stdout->_fileno);
    close(old_stdin);
    close(old_stdout);
    close(client_socket_);
  }
}

void BcmDiagShell::RunDiagShell() {
  LOG(INFO) << "Starting Broadcom Diag Shell.";
  LOG(INFO) << "Broadcom Diag Shell exits.";

  // Terminate the telnet connection, so that telnet client will terminate,
  // and also BcmDiagShellThread will wake up from select() and terminate.
  shutdown(client_socket_, SHUT_RDWR);
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
