// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/file_service.h"

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

FileService::FileService(OperationMode mode, SwitchInterface* switch_interface,
                         AuthPolicyChecker* auth_policy_checker,
                         ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status FileService::Setup(bool warmboot) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status FileService::Teardown() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::grpc::Status FileService::Get(
    ::grpc::ServerContext* context, const ::gnoi::file::GetRequest* req,
    ::grpc::ServerWriter<::gnoi::file::GetResponse>* writer) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Put(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::gnoi::file::PutRequest>* reader,
    ::gnoi::file::PutResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Stat(::grpc::ServerContext* context,
                                 const ::gnoi::file::StatRequest* req,
                                 ::gnoi::file::StatResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Remove(::grpc::ServerContext* context,
                                   const ::gnoi::file::RemoveRequest* req,
                                   ::gnoi::file::RemoveResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
