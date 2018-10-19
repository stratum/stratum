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

#include "stratum/hal/lib/common/file_service.h"

#include "base/commandlineflags.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "util/gtl/map_util.h"

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
  // TODO: Implement this.
  return ::util::OkStatus();
}

::util::Status FileService::Teardown() {
  // TODO: Implement this.
  return ::util::OkStatus();
}

::grpc::Status FileService::Get(
    ::grpc::ServerContext* context, const ::gnoi::file::GetRequest* req,
    ::grpc::ServerWriter<::gnoi::file::GetResponse>* writer) {
  // TODO: Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Put(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::gnoi::file::PutRequest>* reader,
    ::gnoi::file::PutResponse* resp) {
  // TODO: Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Stat(::grpc::ServerContext* context,
                                 const ::gnoi::file::StatRequest* req,
                                 ::gnoi::file::StatResponse* resp) {
  // TODO: Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Remove(::grpc::ServerContext* context,
                                   const ::gnoi::file::RemoveRequest* req,
                                   ::gnoi::file::RemoveResponse* resp) {
  // TODO: Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
