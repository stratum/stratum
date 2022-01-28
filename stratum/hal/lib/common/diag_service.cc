// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/diag_service.h"

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

DiagService::DiagService(OperationMode mode, SwitchInterface* switch_interface,
                         AuthPolicyChecker* auth_policy_checker,
                         ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status DiagService::Setup(bool warmboot) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status DiagService::Teardown() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::grpc::Status DiagService::StartBERT(::grpc::ServerContext* context,
                                      const ::gnoi::diag::StartBERTRequest* req,
                                      ::gnoi::diag::StartBERTResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status DiagService::StopBERT(::grpc::ServerContext* context,
                                     const ::gnoi::diag::StopBERTRequest* req,
                                     ::gnoi::diag::StopBERTResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status DiagService::GetBERTResult(
    ::grpc::ServerContext* context,
    const ::gnoi::diag::GetBERTResultRequest* req,
    ::gnoi::diag::GetBERTResultResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
