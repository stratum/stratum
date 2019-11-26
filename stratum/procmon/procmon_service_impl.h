/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef STRATUM_PROCMON_PROCMON_SERVICE_IMPL_H_
#define STRATUM_PROCMON_PROCMON_SERVICE_IMPL_H_

#include <grpc++/grpc++.h>

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
