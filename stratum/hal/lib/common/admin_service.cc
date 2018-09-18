#include "platforms/networking/hercules/hal/lib/common/admin_service.h"

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

AdminService::AdminService(OperationMode mode,
                           SwitchInterface* switch_interface,
                           AuthPolicyChecker* auth_policy_checker,
                           ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status AdminService::Setup(bool warmboot) {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::util::Status AdminService::Teardown() {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::grpc::Status AdminService::Time(::grpc::ServerContext* context,
                                  const ::gnoi::system::TimeRequest* req,
                                  ::gnoi::system::TimeResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::Reboot(::grpc::ServerContext* context,
                                    const ::gnoi::system::RebootRequest* req,
                                    ::gnoi::system::RebootResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::RebootStatus(
    ::grpc::ServerContext* context,
    const ::gnoi::system::RebootStatusRequest* req,
    ::gnoi::system::RebootStatusResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status AdminService::CancelReboot(
    ::grpc::ServerContext* context,
    const ::gnoi::system::CancelRebootRequest* req,
    ::gnoi::system::CancelRebootResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace hercules
}  // namespace google
