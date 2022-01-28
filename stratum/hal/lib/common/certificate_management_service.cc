// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/common/certificate_management_service.h"

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/logging.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

namespace stratum {
namespace hal {

using RotateCertificateServerReaderWriter =
    ::grpc::ServerReaderWriter< ::gnoi::certificate::RotateCertificateResponse,
                                ::gnoi::certificate::RotateCertificateRequest>;
using InstallCertificateServerReaderWriter =
    ::grpc::ServerReaderWriter< ::gnoi::certificate::InstallCertificateResponse,
                                ::gnoi::certificate::InstallCertificateRequest>;

CertificateManagementService::CertificateManagementService(
    OperationMode mode, SwitchInterface* switch_interface,
    AuthPolicyChecker* auth_policy_checker, ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status CertificateManagementService::Setup(bool warmboot) {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::util::Status CertificateManagementService::Teardown() {
  // TODO(unknown): Implement this.
  return ::util::OkStatus();
}

::grpc::Status CertificateManagementService::Rotate(
    ::grpc::ServerContext* context,
    RotateCertificateServerReaderWriter* stream) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::Install(
    ::grpc::ServerContext* context,
    InstallCertificateServerReaderWriter* stream) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::GetCertificates(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::GetCertificatesRequest* req,
    ::gnoi::certificate::GetCertificatesResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::RevokeCertificates(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::RevokeCertificatesRequest* req,
    ::gnoi::certificate::RevokeCertificatesResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::CanGenerateCSR(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::CanGenerateCSRRequest* req,
    ::gnoi::certificate::CanGenerateCSRResponse* resp) {
  // TODO(unknown): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace stratum
