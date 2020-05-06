// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_PROCMON_PROCMON_SERVICE_IMPL_H_
#define STRATUM_PROCMON_PROCMON_SERVICE_IMPL_H_

#include "grpcpp/grpcpp.h"

#include "stratum/procmon/procmon.grpc.pb.h"

namespace stratum {

namespace procmon {

// Procmon service is in charge of the handling the requests (e.g. process
// checkin) from the rest of the processes.
class ProcmonServiceImpl final : public ProcmonService::Service {
 public:
  // TODO(unknown): Pass a pointer to Procmon class instance to this class.
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

}  // namespace stratum

#endif  // STRATUM_PROCMON_PROCMON_SERVICE_IMPL_H_
