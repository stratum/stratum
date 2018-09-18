#include "platforms/networking/hercules/hal/lib/common/file_service.h"

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

FileService::FileService(OperationMode mode, SwitchInterface* switch_interface,
                         AuthPolicyChecker* auth_policy_checker,
                         ErrorBuffer* error_buffer)
    : mode_(mode),
      switch_interface_(ABSL_DIE_IF_NULL(switch_interface)),
      auth_policy_checker_(ABSL_DIE_IF_NULL(auth_policy_checker)),
      error_buffer_(ABSL_DIE_IF_NULL(error_buffer)) {}

::util::Status FileService::Setup(bool warmboot) {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::util::Status FileService::Teardown() {
  // TODO(aghaffar): Implement this.
  return ::util::OkStatus();
}

::grpc::Status FileService::Get(
    ::grpc::ServerContext* context, const ::gnoi::file::GetRequest* req,
    ::grpc::ServerWriter<::gnoi::file::GetResponse>* writer) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Put(
    ::grpc::ServerContext* context,
    ::grpc::ServerReader<::gnoi::file::PutRequest>* reader,
    ::gnoi::file::PutResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Stat(::grpc::ServerContext* context,
                                 const ::gnoi::file::StatRequest* req,
                                 ::gnoi::file::StatResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

::grpc::Status FileService::Remove(::grpc::ServerContext* context,
                                   const ::gnoi::file::RemoveRequest* req,
                                   ::gnoi::file::RemoveResponse* resp) {
  // TODO(aghaffar): Implement this.
  return ::grpc::Status::OK;
}

}  // namespace hal
}  // namespace hercules
}  // namespace google
