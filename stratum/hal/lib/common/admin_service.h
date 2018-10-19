/*
 * Copyright 2018 Google LLC
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

#ifndef THIRD_PARTY_STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_
#define THIRD_PARTY_STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_

#include <grpc++/grpc++.h>

#include <memory>

#include "stratum/glue/gnoi/system.grpc.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "absl/base/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "util/task/status.h"

namespace stratum {
namespace hal {

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
               ErrorBuffer* error_buffer);
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

  // AdminService is neither copyable nor movable.
  AdminService(const AdminService&) = delete;
  AdminService& operator=(const AdminService&) = delete;

 private:
  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Hercules stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Hercules stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Hercules stack runs in simulation mode.
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
};

}  // namespace hal
}  // namespace stratum

#endif  // THIRD_PARTY_STRATUM_HAL_LIB_COMMON_ADMIN_SERVICE_H_
