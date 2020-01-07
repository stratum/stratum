// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
