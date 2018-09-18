#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_DIAG_SERVICE_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_DIAG_SERVICE_H_

#include <grpc++/grpc++.h>

#include <memory>

#include "platforms/networking/hercules/glue/gnoi/diag.grpc.pb.h"
#include "platforms/networking/hercules/hal/lib/common/common.pb.h"
#include "platforms/networking/hercules/hal/lib/common/error_buffer.h"
#include "platforms/networking/hercules/hal/lib/common/switch_interface.h"
#include "platforms/networking/hercules/lib/security/auth_policy_checker.h"
#include "absl/base/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "util/task/status.h"

namespace google {
namespace hercules {
namespace hal {

// DiagService is an implementation of gnoi::diag::Diag gRPC service and
// is in charge of providing APIs for BERT/Burning/etc.
class DiagService final : public ::gnoi::diag::Diag::Service {
 public:
  // Input parameters:
  // mode: The mode of operation.
  // switch_interface: The pointer to the implementation of SwitchInterface for
  //     all the low-level platform-specific operations.
  // auth_policy_checker: for per RPC authorization policy checks.
  // error_buffer: pointer to an ErrorBuffer for logging all critical errors.
  DiagService(OperationMode mode, SwitchInterface* switch_interface,
              AuthPolicyChecker* auth_policy_checker,
              ErrorBuffer* error_buffer);
  ~DiagService() override {}

  // Sets up the service in coldboot or warmboot mode.
  ::util::Status Setup(bool warmboot);

  // Tears down the class. Called in both warmboot or coldboot mode.
  ::util::Status Teardown();

  // Please see //openconfig/gnoi/diag/diag.proto for the
  // documentation of the RPCs.
  ::grpc::Status StartBERT(::grpc::ServerContext* context,
                           const ::gnoi::diag::StartBERTRequest* req,
                           ::gnoi::diag::StartBERTResponse* resp) override;

  ::grpc::Status StopBERT(::grpc::ServerContext* context,
                          const ::gnoi::diag::StopBERTRequest* req,
                          ::gnoi::diag::StopBERTResponse* resp) override;

  ::grpc::Status GetBERTResult(
      ::grpc::ServerContext* context,
      const ::gnoi::diag::GetBERTResultRequest* req,
      ::gnoi::diag::GetBERTResultResponse* resp) override;

  // DiagService is neither copyable nor movable.
  DiagService(const DiagService&) = delete;
  DiagService& operator=(const DiagService&) = delete;

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
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_COMMON_DIAG_SERVICE_H_
