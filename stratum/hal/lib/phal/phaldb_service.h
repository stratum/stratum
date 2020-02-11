/*
 * Copyright 2019 Google LLC
 * Copyright 2019 Dell EMC
 * Copyright 2019-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_PHAL_PHALDB_SERVICE_H_
#define STRATUM_HAL_LIB_PHAL_PHALDB_SERVICE_H_

#include <pthread.h>

#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "grpcpp/grpcpp.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/channel_writer_wrapper.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/switch_interface.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/db.grpc.pb.h"

namespace stratum {
namespace hal {
namespace phal {

// The "PhalDbService" class implements PhalDb::Service. It handles all
// the RPCs that are part of the Phal DB API API.
class PhalDbService final : public PhalDb::Service {
 public:
  explicit PhalDbService(AttributeDatabaseInterface* attribute_db_interface);

  virtual ~PhalDbService();

  // Sets up the service in coldboot and warmboot mode.
  ::util::Status Setup(bool warmboot);

  // Tears down the class. Called in both warmboot or coldboot mode. It will
  // not alter any state on the hardware when called.
  ::util::Status Teardown();

  // Get a database entry
  ::grpc::Status Get(::grpc::ServerContext* context, const GetRequest* req,
                     GetResponse* resp) override;

  // Subscribe to a database entry
  ::grpc::Status Subscribe(
      ::grpc::ServerContext* context, const SubscribeRequest* req,
      ::grpc::ServerWriter<SubscribeResponse>* resp) override;

  // Set a database entry
  ::grpc::Status Set(::grpc::ServerContext* context, const SetRequest* req,
                     SetResponse* resp) override;

  // PhalDbService is neither copyable nor movable.
  PhalDbService(const PhalDbService&) = delete;
  PhalDbService& operator=(const PhalDbService&) = delete;
  PhalDbService& operator=(PhalDbService&&) = default;

 private:
  // Actual implementations of the calls. To be moved into an impl.
  ::util::Status DoGet(::grpc::ServerContext* context, const GetRequest* req,
                       GetResponse* resp);

  ::util::Status DoSet(::grpc::ServerContext* context, const SetRequest* req,
                       SetResponse* resp);

  ::util::Status DoSubscribe(::grpc::ServerContext* context,
                             const SubscribeRequest* req,
                             ::grpc::ServerWriter<SubscribeResponse>* stream);

  // AttributeDB Interface
  AttributeDatabaseInterface* attribute_db_interface_;

  // Mutex which protects the creation and destruction of the
  // subscriber channels map.
  mutable absl::Mutex subscriber_thread_lock_;

  // Map of subscriber channels (key is thread id, given that
  // each grpc request will have a different tid.
  std::map<pthread_t, std::shared_ptr<Channel<PhalDB>>> subscriber_channels_
      GUARDED_BY(subscriber_thread_lock_);

  friend class PhalDbServiceTest;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_PHALDB_SERVICE_H_
