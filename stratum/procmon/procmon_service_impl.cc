// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

#include "stratum/procmon/procmon_service_impl.h"

namespace stratum {

namespace procmon {

ProcmonServiceImpl::ProcmonServiceImpl() {}

ProcmonServiceImpl::~ProcmonServiceImpl() {}

::grpc::Status ProcmonServiceImpl::Checkin(::grpc::ServerContext* context,
                                           const CheckinRequest* req,
                                           CheckinResponse* resp) {
  // TODO(unknown): Implement this RPC.
  return ::grpc::Status::OK;
}

}  // namespace procmon

}  // namespace stratum
