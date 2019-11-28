/*
 * Copyright 2018 Google LLC
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


#ifndef STRATUM_HAL_LIB_COMMON_PHALDB_SERVICE_H_
#define STRATUM_HAL_LIB_COMMON_PHALDB_SERVICE_H_

#include <grpcpp/grpcpp.h>
#include <pthread.h>

#include <memory>
#include <sstream>
#include <string>
#include <map>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/channel_writer_wrapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/error_buffer.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/db.grpc.pb.h"

namespace stratum {
namespace hal {

// The "PhalDBService" class implements PhalDBService::Service. It handles all
// the RPCs that are part of the Phal DB API API.
class PhalDBService final : public ::stratum::hal::phal::PhalDBSvc::Service {
 public:
  PhalDBService(OperationMode mode, PhalInterface* phal_interface,
                AuthPolicyChecker* auth_policy_checker,
                ErrorBuffer* error_buffer);

  virtual ~PhalDBService();

  // Sets up the service in coldboot and warmboot mode.
  ::util::Status Setup(bool warmboot);

  // Tears down the class. Called in both warmboot or coldboot mode. It will
  // not alter any state on the hardware when called.
  ::util::Status Teardown();

  // Get a database entry
  ::grpc::Status Get(::grpc::ServerContext* context,
      const ::stratum::hal::phal::GetRequest* req,
      ::stratum::hal::phal::GetResponse* resp) override;

  // Subscribe to a database entry
  ::grpc::Status Subscribe(::grpc::ServerContext* context,
      const ::stratum::hal::phal::SubscribeRequest* req,
      ::grpc::ServerWriter<::stratum::hal::phal::SubscribeResponse>* resp)
        override;

  // Set a database entry
  ::grpc::Status Set(::grpc::ServerContext* context,
      const ::stratum::hal::phal::SetRequest* req,
      ::stratum::hal::phal::SetResponse* resp) override;

  // PhalDBService is neither copyable nor movable.
  PhalDBService(const PhalDBService&) = delete;
  PhalDBService& operator=(const PhalDBService&) = delete;

 private:
  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Hercules stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Hercules stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Hercules stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Pointer to AuthPolicyChecker. Not owned by this class.
  AuthPolicyChecker* auth_policy_checker_;

  // Pointer to ErrorBuffer to save any critical errors we encounter. Not owned
  // by this class.
  ErrorBuffer* error_buffer_;

  // PhalDB Adapter
  PhalInterface* phal_interface_;

  // Mutex which protects the creation and destruction of the
  // subscriber channels map.
  mutable absl::Mutex subscriber_thread_lock_;

  // Map of subscriber channels (key is thread id, given that
  // each grpc request will have a different tid.
  std::map<pthread_t, std::shared_ptr<Channel<::stratum::hal::phal::PhalDB>>>
        subscriber_channels_ GUARDED_BY(subscriber_thread_lock_);

  friend class PhalDBServiceTest;
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_PHALDB_SERVICE_H_
