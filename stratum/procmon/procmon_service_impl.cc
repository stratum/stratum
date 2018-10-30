#include "stratum/procmon/procmon_service_impl.h"

namespace google {
namespace hercules {
namespace procmon {

ProcmonServiceImpl::ProcmonServiceImpl() {}

ProcmonServiceImpl::~ProcmonServiceImpl() {}

::grpc::Status ProcmonServiceImpl::Checkin(::grpc::ServerContext* context,
                                           const CheckinRequest* req,
                                           CheckinResponse* resp) {
  // TODO(aghaffar): Implement this RPC.
  return ::grpc::Status::OK;
}

}  // namespace procmon
}  // namespace hercules
}  // namespace google
