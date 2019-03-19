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

#include "stratum/hal/lib/common/admin_service.h"

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/common/admin_utils_interface.h"

namespace stratum {
namespace hal {

AdminService::AdminService(OperationMode mode,
                           SwitchInterface* switch_interface,
                           AuthPolicyChecker* auth_policy_checker,
                           ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {
  helper_ = absl::make_unique<AdminServiceUtilsInterface>();
}

::util::Status AdminService::Setup(bool warmboot) {
  // TODO: Implement this.
  return ::util::OkStatus();
}

::util::Status AdminService::Teardown() {
  // TODO: Implement this.
  return ::util::OkStatus();
}

::grpc::Status AdminService::Time(::grpc::ServerContext* context,
                                  const ::gnoi::system::TimeRequest* req,
                                  ::gnoi::system::TimeResponse* resp) {
  resp->set_time(helper_->GetTime());
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::Reboot(::grpc::ServerContext* context,
                                    const ::gnoi::system::RebootRequest* req,
                                    ::gnoi::system::RebootResponse* resp) {
  if (req->delay() != 0) {
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "Reboot delay is not supported");
  }

  if (!req->message().empty()) {
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "Reboot message is not supported");
  }

  bool reboot_status = true;
  switch (req->method()) {
    case gnoi::system::RebootMethod::COLD : {
      reboot_status = helper_->GetShellHelper("/sbin/shutdown -r")->Execute();
      break;
    }
    case gnoi::system::RebootMethod::UNKNOWN : {
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                            "Invalid reboot method UNKNOWN.");
    }
    default: {
      return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "Unsupported reboot method " +
                            ::gnoi::system::RebootMethod_Name(req->method()));
    }
  }

  // Return failure if reboot was not successful
  if(!reboot_status) {
      return ::grpc::Status(::grpc::StatusCode::INTERNAL,
                            "Failed to reboot the system.");
  }
  
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::RebootStatus(
    ::grpc::ServerContext* context,
    const ::gnoi::system::RebootStatusRequest* req,
    ::gnoi::system::RebootStatusResponse* resp) {

  resp->set_active(false);
  // TODO: Update other response fields when delayed reboot is implemented
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::CancelReboot(
    ::grpc::ServerContext* context,
    const ::gnoi::system::CancelRebootRequest* req,
    ::gnoi::system::CancelRebootResponse* resp) {
  // TODO: Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
