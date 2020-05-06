// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_BCM_BCM_DIAG_SHELL_H_
#define STRATUM_HAL_LIB_BCM_BCM_DIAG_SHELL_H_

#include <pthread.h>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmDiagShell" class is a self-contained class which is used to bring up
// BCM diag shell. This class is initialized once and is accessed through its
// singleton instance.
class BcmDiagShell {
 public:
  virtual ~BcmDiagShell();

  // The only main public method. Starts the diag server and listens for the
  // telnet connection. There is no StopServer() method. We assume that when
  // the server starts, it will listen for connection for ever, until HAL
  // exits.
  ::util::Status StartServer() LOCKS_EXCLUDED(server_lock_);

  // Thread id for the currently running diag shell thread.
  pthread_t GetDiagShellThreadId() const LOCKS_EXCLUDED(shell_lock_);

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static BcmDiagShell* CreateSingleton() LOCKS_EXCLUDED(init_lock_);

  // BcmDiagShell is neither copyable nor movable.
  BcmDiagShell(const BcmDiagShell&) = delete;
  BcmDiagShell& operator=(const BcmDiagShell&) = delete;

 private:
  // Telnet-related constants.
  static constexpr unsigned char kTelnetCmd = 255;
  static constexpr unsigned char kTelnetDont = 254;
  static constexpr unsigned char kTelnetDo = 253;
  static constexpr unsigned char kTelnetWont = 252;
  static constexpr unsigned char kTelnetWill = 251;
  static constexpr unsigned char kTelnetEcho = 1;
  static constexpr unsigned char kTelnetSGA = 3;
  static constexpr unsigned char kTelnetWillSGA[] = {
      kTelnetCmd, kTelnetWill, kTelnetSGA
  };
  static constexpr unsigned char kTelnetWillEcho[] = {
    kTelnetCmd, kTelnetWill, kTelnetEcho
  };
  static constexpr unsigned char kTelnetDontEcho[] = {
    kTelnetCmd, kTelnetDont, kTelnetEcho
  };
  static constexpr int kNumberOfBytesRead = 82;

  // Private constructor.
  BcmDiagShell();

  // Server thread function passed to pthread_create.
  static void* ServerThreadFunc(void* arg);

  // Diag shell thread function passed to pthread_create.
  static void* ShellThreadFunc(void* arg);

  // Called in ServerThreadFunc() to run the diag server.
  void RunServer();

  // Called in ShellThreadFunc() to start the shell process.
  void RunDiagShell();

  // Helper functions to deal with telnet connection.
  void ForwardTelnetSession();
  void ProcessTelnetCommand();
  int ProcessTelnetInput();
  int ReadNextTelnetCommandByte(unsigned char* data);
  void SendTelnetDataToPty();
  void WriteToTelnetClient(const void* data, size_t size);
  void WriteToPtyMaster(const void* data, size_t size);

  // The lock used for initialization of the singleton.
  static absl::Mutex init_lock_;

  // The lock used to synchronize creating the server thread.
  mutable absl::Mutex server_lock_;

  // The lock used to synchronize writing the shell thread id and reading it.
  mutable absl::Mutex shell_lock_;

  // The singleton instance.
  static BcmDiagShell* singleton_ GUARDED_BY(init_lock_);

  // Shows whether the server thread has been started.
  bool server_started_ GUARDED_BY(server_lock_);

  // Server thread id.
  pthread_t server_thread_id_ GUARDED_BY(server_lock_);

  // Diag shell thread id (for the currently running shell).
  pthread_t shell_thread_id_ GUARDED_BY(shell_lock_);

  // Server socket used for listening to telnet clients.
  int server_socket_;

  // Client threads, assigned to the one and only active telnet client
  // connected.
  int client_socket_;

  // Misc vars used by the helper function to deal with telnet connections.
  int pty_master_fd_;
  int pty_slave_fd_;
  char net_buffer_[kNumberOfBytesRead];
  int net_buffer_count_;
  int net_buffer_offset_;
  int data_start_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_DIAG_SHELL_H_
