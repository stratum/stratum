#include "platforms/networking/hercules/hal/lib/common/diag_service.h"

#include "base/commandlineflags.h"
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

DiagService::DiagService(OperationMode mode, SwitchInterface* switch_interface,
                         AuthPolicyChecker* auth_policy_checker,
                         ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status DiagService::Setup(bool warmboot) {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::util::Status DiagService::Teardown() {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::grpc::Status DiagService::StartBERT(::grpc::ServerContext* context,
                                      const ::gnoi::diag::StartBERTRequest* req,
                                      ::gnoi::diag::StartBERTResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status DiagService::StopBERT(::grpc::ServerContext* context,
                                     const ::gnoi::diag::StopBERTRequest* req,
                                     ::gnoi::diag::StopBERTResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status DiagService::GetBERTResult(
    ::grpc::ServerContext* context,
    const ::gnoi::diag::GetBERTResultRequest* req,
    ::gnoi::diag::GetBERTResultResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace hercules
}  // namespace google
