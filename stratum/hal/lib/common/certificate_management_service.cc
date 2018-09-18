#include "platforms/networking/hercules/hal/lib/common/certificate_management_service.h"

#include "platforms/networking/hercules/glue/logging.h"
#include "platforms/networking/hercules/lib/macros.h"
#include "platforms/networking/hercules/lib/utils.h"
#include "platforms/networking/hercules/public/lib/error.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "util/gtl/map_util.h"

namespace google {
namespace hercules {
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
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::util::Status CertificateManagementService::Teardown() {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::grpc::Status CertificateManagementService::Rotate(
    ::grpc::ServerContext* context,
    RotateCertificateServerReaderWriter* stream) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::Install(
    ::grpc::ServerContext* context,
    InstallCertificateServerReaderWriter* stream) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::GetCertificates(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::GetCertificatesRequest* req,
    ::gnoi::certificate::GetCertificatesResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::RevokeCertificates(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::RevokeCertificatesRequest* req,
    ::gnoi::certificate::RevokeCertificatesResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status CertificateManagementService::CanGenerateCSR(
    ::grpc::ServerContext* context,
    const ::gnoi::certificate::CanGenerateCSRRequest* req,
    ::gnoi::certificate::CanGenerateCSRResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace hercules
}  // namespace google
