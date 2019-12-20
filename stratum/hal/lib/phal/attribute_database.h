/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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


#ifndef STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_
#define STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_

#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <set>
#include <utility>

#include "google/protobuf/message.h"
#include "absl/container/flat_hash_set.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/db.pb.h"
// #include "stratum/hal/lib/phal/google_platform/google_switch_configurator.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/phaldb_service.h"
#include "stratum/hal/lib/phal/system_interface.h"
#include "stratum/hal/lib/phal/switch_configurator.h"
#include "stratum/hal/lib/phal/threadpool_interface.h"
#include "stratum/hal/lib/phal/udev_event_handler.h"
#include "stratum/lib/channel/channel.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"

namespace stratum {
namespace hal {
namespace phal {

class DatabaseQuery;

// The internal implementation of AttributeDatabaseInterface.
//
// This interface will change as AttributeDatabaseInterface changes.
class AttributeDatabase : public AttributeDatabaseInterface {
 public:
  ~AttributeDatabase() override;
  AttributeDatabase(const AttributeDatabase& other) = delete;
  AttributeDatabase& operator=(const AttributeDatabase& other) = delete;

  // Creates a new attribute database that runs on a google-developed switch
  // platform. <deprecated>
  // static ::util::StatusOr<std::unique_ptr<AttributeDatabase>> MakeGoogle(
  //    const std::string& legacy_phal_config_path,
  //    const SystemInterface* system_interface);

  // Creates a new Phal attribute database
  static ::util::StatusOr<std::unique_ptr<AttributeDatabase>> MakePhalDB(
      std::unique_ptr<SwitchConfigurator> configurator);

  ::util::Status Set(const AttributeValueMap& values) override
      LOCKS_EXCLUDED(set_lock_);
  ::util::StatusOr<std::unique_ptr<Query>> MakeQuery(
      const std::vector<Path>& query_paths) override;

 private:
  friend class AttributeDatabaseTest;
  friend class DatabaseQuery;

  AttributeDatabase(std::unique_ptr<AttributeGroup> root,
                    std::unique_ptr<ThreadpoolInterface> threadpool)
      : root_(std::move(root)), threadpool_(std::move(threadpool)) {}

  // Creates a new attribute database that uses the given group as its root node
  // and executes queries on the given threadpool. MakeGoogle or MakePhalDB
  // should typically be called rather than this function. If
  // run_polling_thread is false, no streaming query polling will occur
  // unless PollQueries is called manually.
  static ::util::StatusOr<std::unique_ptr<AttributeDatabase>> Make(
      std::unique_ptr<AttributeGroup> root,
      std::unique_ptr<ThreadpoolInterface> threadpool,
      bool run_polling_thread = true);

  // Starts the thread responsible for polling the attribute database. Used to
  // facilitate streaming queries.
  ::util::Status SetupPolling();
  // If the polling thread is running, safely shuts it down.
  void TeardownPolling();
  // Shut down the PhalDB service.
  void ShutdownService();
  // Repeatedly polls queries until polling_thread_running_ is set to false.
  // Called directly by pthread_create.
  static void* RunPollingThread(void* attribute_database_ptr);
  // Calculates the next time we should poll the attribute database for
  // streaming query updates.
  absl::Time GetNextPollingTime() EXCLUSIVE_LOCKS_REQUIRED(polling_lock_);
  // Polls the attribute database to see if any streaming queries
  // should be sent an update.
  ::util::Status PollQueries() EXCLUSIVE_LOCKS_REQUIRED(polling_lock_);
  // For each streaming query that is marked as updated, sends a message to all
  // subscribers.
  ::util::Status FlushQueries() EXCLUSIVE_LOCKS_REQUIRED(polling_lock_);

  // The root node of the attribute tree maintained by this database.
  std::unique_ptr<AttributeGroup> root_;
  // The threadpool used to parallelize database queries.
  std::unique_ptr<ThreadpoolInterface> threadpool_;
  // The udev handler for detecting hardware state changes that affect the
  // database structure.
  std::unique_ptr<UdevEventHandler> udev_;
  // The configurator used for switches.
  std::unique_ptr<SwitchConfigurator> switch_configurator_;

  // The thread to handle polling for streaming queries.
  pthread_t polling_thread_id_;
  // A lock to manage interactions with the polling thread.
  absl::Mutex polling_lock_;
  // This condition variable is used to implement interruptable sleeps. The
  // polling thread should sleep until the next polling interval has elapsed,
  // but this sleep should be interrupted if a new subscriber is added and the
  // polling interval changes.
  absl::CondVar polling_condvar_;
  // This boolean is set to true when the polling thread starts. The polling
  // thread will continue running until this is set to false.
  bool polling_thread_running_ GUARDED_BY(polling_lock_) = false;
  // The set of all queries that we may need to poll.
  absl::flat_hash_set<DatabaseQuery*> polling_queries_
      GUARDED_BY(polling_lock_);
  // A lock to serialize all calls to Set(...).
  absl::Mutex set_lock_;
  // The PhalDb service exposing the database, mainly for debugging.
  // Owned by the class.
  std::unique_ptr<::grpc::Server> external_server_;
  // Unique pointer to the gRPC server serving the internal RPC connections
  // serviced by PhalDbService. Owned by the class.
  std::unique_ptr<PhalDbService> phal_db_service_;
};

// DatabaseQuery is a wrapper for AttributeGroupQuery that transforms query
// responses from google::protobuf::Message into PhalDB. It also handles
// polling for streaming queries.
class DatabaseQuery : public Query {
 public:
  DatabaseQuery(const DatabaseQuery& other) = delete;
  DatabaseQuery& operator=(const DatabaseQuery& other) = delete;
  ~DatabaseQuery() override;

  // Query functions:
  ::util::StatusOr<std::unique_ptr<PhalDB>> Get() override;
  ::util::Status Subscribe(std::unique_ptr<ChannelWriter<PhalDB>> subscriber,
                           absl::Duration polling_interval) override;

  // Polls this query to see if the result has changed since the last time Poll
  // was called. If the result has changed, sets the update bit in the internal
  // AttributeGroupQuery.
  ::util::Status Poll(absl::Time poll_time);
  AttributeGroupQuery* InternalQuery() { return &query_; }
  // Returns the next time we're supposed to poll this query, based on the
  // polling intervals requested by subscribers.
  absl::Time GetNextPollingTime();
  // Executes this query and sends the result to every subscriber. If any
  // subscriber channels have closed, performs all necessary cleanup.
  ::util::Status UpdateSubscribers();

 private:
  friend class AttributeDatabase;

  DatabaseQuery(AttributeDatabase* database, AttributeGroup* root_group,
                ThreadpoolInterface* threadpool);

  AttributeDatabase* database_;
  AttributeGroupQuery query_;

  // For streaming queries, this query will be polled on some interval. Each
  // subscriber may specify a different interval, so we use the shortest one.
  // Calculates this interval and stores it in polling_interval_.
  void RecalculatePollingInterval();

  // Keeps track of all subscribers to this query, as well as the polling
  // interval they requested.
  std::vector<std::pair<std::unique_ptr<ChannelWriter<PhalDB>>, absl::Duration>>
      subscribers_;
  // The minimum polling interval requested by any subscriber to this query.
  absl::Duration polling_interval_ = absl::InfiniteDuration();

  absl::Time last_polling_time_;
  std::unique_ptr<PhalDB> last_polling_result_;
};

}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ATTRIBUTE_DATABASE_H_
