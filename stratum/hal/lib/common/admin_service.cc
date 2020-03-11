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

#include <string>

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
                           ErrorBuffer* error_buffer,
                           HalSignalHandle hal_signal_handle)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)),
      reboot_count_(0),
      hal_signal_handle_(hal_signal_handle) {
  helper_ = absl::make_unique<AdminServiceUtilsInterface>();
}

::util::Status AdminService::Setup(bool warmboot) {
  if (TimerDaemon::Start() != ::util::OkStatus()) {
    MAKE_ERROR(ERR_INTERNAL) << "Could not start the timer subsystem.";
  }
  return ::util::OkStatus();
}

::util::Status AdminService::Teardown() {
  absl::ReaderMutexLock l(&reboot_lock_);
  if (TimerDaemon::Stop() != ::util::OkStatus()) {
    LOG(ERROR) << "Could not stop the timer subsystem.";
  }
  if (reboot_timer_) {
    this->helper_->Reboot();
  }
  return ::util::OkStatus();
}

::grpc::Status AdminService::Time(::grpc::ServerContext* context,
                                  const ::gnoi::system::TimeRequest* req,
                                  ::gnoi::system::TimeResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, AdminService, Time,
                           context);
  resp->set_time(helper_->GetTime());
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::Reboot(::grpc::ServerContext* context,
                                    const ::gnoi::system::RebootRequest* req,
                                    ::gnoi::system::RebootResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, AdminService, Reboot,
                           context);
  absl::WriterMutexLock l(&reboot_lock_);

  if (reboot_timer_) {
    // reject reboot request if there is a pending reboot request.
    return ::grpc::Status(::grpc::StatusCode::ALREADY_EXISTS,
                          "Pending reboot exists.");
  }

  // Delay from gNOI is nanosecond based, convert it to ms for TimerDaemon
  uint64 delay = req->delay() / 1000000;
  if (delay == 0) {
    delay = kDefaultRebootDelay;
  }
  if (!req->message().empty()) {
    // TODO(Yi): use reboot(int, int, int, void*) for reboot message
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                            "Reboot message is not supported");
  }
  switch (req->method()) {
    case gnoi::system::RebootMethod::COLD : {
      ++reboot_count_;
      TimerDaemon::RequestOneShotTimer(delay, [this]() {
        hal_signal_handle_(SIGINT);
        return ::util::OkStatus();
      }, &reboot_timer_);
      LOG(INFO) << "Rebooting in " << delay << " ms.";
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
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::RebootStatus(
    ::grpc::ServerContext* context,
    const ::gnoi::system::RebootStatusRequest* req,
    ::gnoi::system::RebootStatusResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, AdminService, RebootStatus,
                           context);
  if (!reboot_timer_) {
    resp->set_active(false);
    return ::grpc::Status::OK;
  }

  absl::ReaderMutexLock l(&reboot_lock_);
  uint64 now = absl::ToUnixNanos(absl::Now());
  uint64 when = absl::ToUnixNanos(reboot_timer_->due_time_);
  uint64 wait = when - now;
  resp->set_active(true);
  resp->set_wait(wait);
  resp->set_when(when);
  resp->set_count(reboot_count_);
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::CancelReboot(
    ::grpc::ServerContext* context,
    const ::gnoi::system::CancelRebootRequest* req,
    ::gnoi::system::CancelRebootResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, AdminService, CancelReboot,
                           context);
  absl::WriterMutexLock l(&reboot_lock_);
  reboot_timer_.reset();
  LOG(INFO) << "Reboot canceled";
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::ValidatePackageMessage(
    const gnoi::system::Package& package) {

  if (package.activate()) {
    // TODO(unknown): remove when ActivatePackage will be implemented
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                          "Package activation not supported");
  }

  if (!package.version().empty()) {
    // TODO(unknown): remove when SetPackageVersion will be implemented
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                          "Package version not supported");
  }

  if (package.has_remote_download()) {
    // TODO(unknown): remove when remote download will be implemented
    return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                          "Remote download not supported");
  }

  if (package.filename().empty()) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "File name not specified.");
  }

  if (package.filename()[0] != '/') {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Received relative file path.");
  }

  if (!helper_->GetFileSystemHelper()->PathExists(
          DirName(package.filename()))) {
    return ::grpc::Status(
        ::grpc::StatusCode::NOT_FOUND,
        ("Directory " + package.filename() + " doesn't exist"));
  }

  return ::grpc::Status::OK;
}

::grpc::Status AdminService::SetPackage(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::gnoi::system::SetPackageRequest>* reader,
    ::gnoi::system::SetPackageResponse* resp) {
  RETURN_IF_NOT_AUTHORIZED(auth_policy_checker_, AdminService, SetPackage,
                           context);
  ::gnoi::system::SetPackageRequest msg;
  auto fs_helper = helper_->GetFileSystemHelper();

  if (!reader->Read(&msg)) {
    return ::grpc::Status(::grpc::StatusCode::ABORTED,
                          "Failed to read gRPC stream");
  }

  if (msg.request_case() != ::gnoi::system::SetPackageRequest::kPackage) {
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "Initial message must specify package.");
  }

  auto package = msg.package();
  auto status = ValidatePackageMessage(package);
  if (!status.ok()) {
    return status;
  }

  std::string tmp_dir_name = fs_helper->CreateTempDir();
  std::string tmp_file_name = fs_helper->TempFileName(tmp_dir_name);

  if (!package.has_remote_download()) {
    // receive file trough stream
    while (reader->Read(&msg)) {
      if (msg.request_case() == ::gnoi::system::SetPackageRequest::kContents) {
        fs_helper->StringToFile(msg.contents(), tmp_file_name, true);
      } else {
        break;
      }
    }

  } else {
    // TODO(unknown) uncomment when remote download will be implemented
    // // read next msg from stream (hash msg)
    // if (!reader->Read(&msg)) {
    //   return ::grpc::Status(::grpc::StatusCode::ABORTED, "Broken Stream");
    // }
  }

  if (msg.request_case() != ::gnoi::system::SetPackageRequest::kHash) {
    fs_helper->RemoveFile(tmp_file_name);
    fs_helper->RemoveDir(tmp_dir_name);
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "The last message must have hash");
  }

  if (msg.hash().method() == ::gnoi::types::HashType_HashMethod_UNSPECIFIED) {
    fs_helper->RemoveFile(tmp_file_name);
    fs_helper->RemoveDir(tmp_dir_name);
    return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                          "The hash method must be specified");
  }

  if (!fs_helper->CheckHashSumFile(tmp_file_name,
                                   msg.hash().hash(),
                                   msg.hash().method())) {
    fs_helper->RemoveFile(tmp_file_name);
    fs_helper->RemoveDir(tmp_dir_name);
    return ::grpc::Status(::grpc::StatusCode::DATA_LOSS,
                          "Invalid Hash Sum of received file");
  }

  fs_helper->CopyFile(tmp_file_name, package.filename());
  fs_helper->RemoveFile(tmp_file_name);
  fs_helper->RemoveDir(tmp_dir_name);

  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
