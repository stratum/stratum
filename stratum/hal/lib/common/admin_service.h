/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_
#define STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/common/admin_utils_interface.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/timer_daemon.h"
#include "system/system.grpc.pb.h"

namespace stratum {
namespace hal {

constexpr int kDefaultRebootDelay = 1000;  // ms
using HalSignalHandle = std::function<void(int)>;

// AdminService is an implementation of gnoi::system::System gRPC service and
// is in charge of providing system-level administaration functionalities.
class AdminService final : public ::gnoi::system::System::Service {
 public:
  // Input parameters:
  // mode: The mode of operation.
  // switch_interface: The pointer to the implementation of SwitchInterface for
  //     all the low-level platform-specific operations.
  // auth_policy_checker: for per RPC authorization policy checks.
  // error_buffer: pointer to an ErrorBuffer for logging all critical errors.
  AdminService(OperationMode mode, SwitchInterface* switch_interface,
               AuthPolicyChecker* auth_policy_checker,
               ErrorBuffer* error_buffer,
               HalSignalHandle hal_signal_handle);
  ~AdminService() override {}

  // Sets up the service in coldboot or warmboot mode.
  ::util::Status Setup(bool warmboot);

  // Tears down the class. Called in both warmboot or coldboot mode.
  ::util::Status Teardown();

  // Please see //openconfig/gnoi/system/system.proto for the
  // documentation of the RPCs.
  ::grpc::Status Time(::grpc::ServerContext* context,
                      const ::gnoi::system::TimeRequest* req,
                      ::gnoi::system::TimeResponse* resp) override;

  ::grpc::Status Reboot(::grpc::ServerContext* context,
                        const ::gnoi::system::RebootRequest* req,
                        ::gnoi::system::RebootResponse* resp) override;

  ::grpc::Status RebootStatus(
      ::grpc::ServerContext* context,
      const ::gnoi::system::RebootStatusRequest* req,
      ::gnoi::system::RebootStatusResponse* resp) override;

  ::grpc::Status CancelReboot(
      ::grpc::ServerContext* context,
      const ::gnoi::system::CancelRebootRequest* req,
      ::gnoi::system::CancelRebootResponse* resp) override;

  ::grpc::Status SetPackage(
      ::grpc::ServerContext* context,
      ::grpc::ServerReader<::gnoi::system::SetPackageRequest>* reader,
      ::gnoi::system::SetPackageResponse* resp) override;

  // AdminService is neither copyable nor movable.
  AdminService(const AdminService&) = delete;
  AdminService& operator=(const AdminService&) = delete;

 private:
  // Checks if we received valid initial SetPackage message and provided
  // Package can be accepted and processed
  ::grpc::Status ValidatePackageMessage(const gnoi::system::Package& package);

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  const OperationMode mode_;

  // Pointer to SwitchInterface implementation, which encapsulates all the
  // switch capabilities. Not owned by this class.
  SwitchInterface* switch_interface_;

  // Pointer to AuthPolicyChecker. Not owned by this class.
  AuthPolicyChecker* auth_policy_checker_;

  // Pointer to ErrorBuffer to save any critical errors we encounter. Not owned
  // by this class.
  ErrorBuffer* error_buffer_;

  // Pointer to Utils object to encapsulate helper functions for the
  // implementation.
  std::unique_ptr<AdminServiceUtilsInterface> helper_;

  // lock for reboot operations
  mutable absl::Mutex reboot_lock_;

  // Timer for reboot
  TimerDaemon::DescriptorPtr reboot_timer_;

  // Number of reboots since active.
  uint32 reboot_count_;

  // Service test. Updates the helper with a mock object.
  friend class AdminServiceTest;

  // Function which sends signal to the HAL
  HalSignalHandle hal_signal_handle_;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_
