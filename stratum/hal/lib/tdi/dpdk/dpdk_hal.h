// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// The Hardware Abstraction Layer (HAL) of the stratum stack.

#ifndef STRATUM_HAL_LIB_TDI_DPDK_DPDK_HAL_H_
#define STRATUM_HAL_LIB_TDI_DPDK_DPDK_HAL_H_

#include <pthread.h>
#include <signal.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/config_monitoring_service.h"
#include "stratum/hal/lib/common/p4_service.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/lib/security/auth_policy_checker.h"

namespace stratum {
namespace hal {

// Class 'DpdkHal' is nothing but a wrapper around all the HAL services, which
// implement the main functionality of HAL and handle all the gRPC calls, and
// the '::gRPC::Server' class object which dispatches the calls etc. The intent
// is to 1) put the common code for dealing with these two classes into one
// place, and 2) control the server side parameters without affecting the rest
// of the code. This class is initialized once and is accessed through its
// singleton instance.
class DpdkHal final {
 public:
  virtual ~DpdkHal();

  // All the pre-setup sanity checks that need to be done before anything else.
  // Typically an error returned from this method is an indicator that we should
  // not continue running DpdkHal.
  ::util::Status SanityCheck();

  // Sets up HAL in coldboot and warmboot mode.
  ::util::Status Setup();

  // Sets up HAL in specified mode.
  ::util::Status Setup(bool warmboot);

  // Tears down HAL. Called as part of both warmboot and coldboot shutdown.
  // In case of warmboot shutdown, the user needs to freeze the stack before
  // shutting down HAL.
  ::util::Status Teardown();

  // Blocking call to start listening on the setup url for RPC calls. Blocks
  // until the server is shutdown, in which case calls Teardown() before exit.
  // Run() is to be called after Setup().
  ::util::Status Run();

  // Called when receiving a SIGINT or SIGTERM by the signal received callback.
  void HandleSignal(int value);

  // Returns the list of errors HAL and all its services have encountered.
  inline std::vector<::util::Status> GetErrors() const {
    return error_buffer_->GetErrors();
  }

  // Clears the list of errors HAL and all its services have encountered.
  inline void ClearErrors() const { return error_buffer_->ClearErrors(); }

  // Returns true if HAL or any of its services have encountered an error.
  inline bool ErrorExists() const { return error_buffer_->ErrorExists(); }

  // Creates the singleton instance. Expected to be called once to initialize
  // the instance.
  static DpdkHal* CreateSingleton(OperationMode mode,
                                  SwitchInterface* switch_interface,
                                  AuthPolicyChecker* auth_policy_checker)
      LOCKS_EXCLUDED(init_lock_);

  // Return the singleton instance to be used in the signal handler..
  static DpdkHal* GetSingleton() LOCKS_EXCLUDED(init_lock_);

  // DpdkHal is neither copyable nor movable.
  DpdkHal(const DpdkHal&) = delete;
  DpdkHal& operator=(const DpdkHal&) = delete;

  // Pipe file descriptors used to deliver signals from the handler to DpdkHal.
  static int pipe_read_fd_;
  static int pipe_write_fd_;

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  DpdkHal(OperationMode mode, SwitchInterface* switch_interface,
          AuthPolicyChecker* auth_policy_checker);

  // Initializes the HAL server and all the services it provides. Called in
  // CreateSingleton() as soon as the class instance is created.
  ::util::Status InitializeServer();

  // Helpers to register/unregister SIGINT or SIGTERM signal handlers.
  ::util::Status RegisterSignalHandlers();
  ::util::Status UnregisterSignalHandlers();

  // Thread function waiting for a signal in the pipe and then initialting the
  // HAL shutdown.
  static void* SignalWaiterThreadFunc(void*);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Pointer to SwitchInterface implementation, which encapsulates all the
  // switch capabilities. Not owned by this class.
  SwitchInterface* switch_interface_;

  // Pointer to AuthPolicyChecker. Not owned by this class.
  AuthPolicyChecker* auth_policy_checker_;

  // The ErrorBuffer instance to keep track of all the critical errors we face.
  // A pointer to this instance is also passed to all the HAL services.
  std::unique_ptr<ErrorBuffer> error_buffer_;

  // Unique pointer to the HAL service classes. Owned by the class.
  std::unique_ptr<ConfigMonitoringService> config_monitoring_service_;
  std::unique_ptr<P4Service> p4_service_;

  // Unique pointer to the gRPC server serving the external RPC connections
  // serviced by ConfigMonitoringService and P4Service. Owned by the class.
  std::unique_ptr<::grpc::Server> external_server_;

  // Map from signals for which we registered handlers to their old handlers.
  // This map is used to restore the signal handlers to their previous state
  // in the class destructor.
  absl::flat_hash_map<int, sighandler_t> old_signal_handlers_;

  // Thread id for the signal waiter thread.
  pthread_t signal_waiter_tid_;

  // The lock used for initialization of the singleton.
  static absl::Mutex init_lock_;

  // The singleton instance.
  static DpdkHal* singleton_ GUARDED_BY(init_lock_);
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_TDI_DPDK_DPDK_HAL_H_
