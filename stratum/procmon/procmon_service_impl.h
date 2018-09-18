#ifndef PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_SERVICE_IMPL_H_
#define PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_SERVICE_IMPL_H_

#include <grpc++/grpc++.h>

#include "platforms/networking/hercules/procmon/procmon.grpc.pb.h"

namespace google {
namespace hercules {
namespace procmon {

// Procmon service is in charge of the handling the requests (e.g. process
// checkin) from the rest of the processes.
class ProcmonServiceImpl final : public ProcmonService::Service {
 public:
  // TODO(aghaffar): Pass a pointer to Procmon class instance to this class.
  ProcmonServiceImpl();
  ~ProcmonServiceImpl() override;

  // Implements the Checkin RPC.
  ::grpc::Status Checkin(::grpc::ServerContext* context,
                         const CheckinRequest* req,
                         CheckinResponse* resp) override;

  // ProcmonServiceImpl is neither copyable nor movable.
  ProcmonServiceImpl(const ProcmonServiceImpl&) = delete;
  ProcmonServiceImpl& operator=(const ProcmonServiceImpl&) = delete;
};

}  // namespace procmon
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_PROCMON_PROCMON_SERVICE_IMPL_H_
