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

#ifndef STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_H_
#define STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "grpcpp/grpcpp.h"
#include "stratum/glue/status/status.h"

namespace stratum {

// AuthPolicyChecker is in charge of determining whether a username or group is
// authorized to use an RPC defined in a service.
class AuthPolicyChecker {
 public:
  virtual ~AuthPolicyChecker();

  // Returns OK if the peer (given by ::grpc::AuthContext) is authorized.
  // Otherwise returns proper errors.
  virtual ::util::Status Authorize(
      const std::string& service_name, const std::string& rpc_name,
      const ::grpc::AuthContext& auth_context) const;

  // Refreshes the internal policy map(s). Used for forcing an update of the
  // internal policy map(s). Note that the map(s) will also be updated via the
  // watcher thread, which also calls this method.
  virtual ::util::Status RefreshPolicies() LOCKS_EXCLUDED(auth_lock_);

  // Performs shutdown of the class. Note that there is no public Initialize().
  // Initialize() is a private method which is called once when creating an
  // instance of the class.
  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(shutdown_lock_);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<AuthPolicyChecker> CreateInstance();

  // AuthPolicyChecker is neither copyable nor movable.
  AuthPolicyChecker(const AuthPolicyChecker&) = delete;
  AuthPolicyChecker& operator=(const AuthPolicyChecker&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance as well as
  // CreateInstance().
  AuthPolicyChecker();

 private:
  // Types alias for per-service per-rpc authorized user map.
  using PerRpcAuthorizedUsers =
      absl::flat_hash_map<std::string, std::set<std::string>>;
  using PerServicePerRpcAuthorizedUsers =
      absl::flat_hash_map<std::string, PerRpcAuthorizedUsers>;

  // The const key used as the default rpc name in
  // per_service_per_rpc_authorized_users_.
  static constexpr char kDefaultRpc[] = "";

  // Initializes the class. This includes spawning a thread which will watch
  // for changes in the files that include the membership info and auth
  // policies.
  ::util::Status Initialize() LOCKS_EXCLUDED(shutdown_lock_);

  // Called by Authorize() method to check for authorization of a specific
  // username.
  ::util::Status AuthorizeUser(const std::string& service_name,
                               const std::string& rpc_name,
                               const std::string& username) const
      LOCKS_EXCLUDED(auth_lock_);

  // Helper to continuously watch for a change in the files that include the
  // membership info and auth policies. Called in WatcherThreadFunc().
  ::util::Status WatchForFileChange() LOCKS_EXCLUDED(shutdown_lock_);

  // File watcher thread function. Upon being spawned, calls helper method
  // WatchForFileChange() and waits for its completion.
  static void* WatcherThreadFunc(void* arg);

  // The id of the watcher thread. Used for joining the thread when shutting
  // down.
  pthread_t watcher_thread_id_;

  // Set to true to inform the threads to exit.
  bool shutdown_ GUARDED_BY(shutdown_lock_);

  // Per-service per-rpc authorized user map. Updated in RefreshPolicies().
  PerServicePerRpcAuthorizedUsers per_service_per_rpc_authorized_users_
      GUARDED_BY(auth_lock_);

  // Mutex lock for protecting the internal authorized users map.
  mutable absl::Mutex auth_lock_;

  // Mutex lock for protecting shutdown_ variable.
  mutable absl::Mutex shutdown_lock_;

  friend class AuthPolicyCheckerTest;
};

}  // namespace stratum

#endif  // STRATUM_LIB_SECURITY_AUTH_POLICY_CHECKER_H_
