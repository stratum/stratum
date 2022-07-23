// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/security/auth_policy_checker.h"

#include <errno.h>
#include <stdlib.h>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "google/protobuf/message.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

// TODO(unknown): Set the default to true when feature is fully available.
DEFINE_bool(enable_authorization, false,
            "Whether to enable per service per RPC authorization checking. The "
            "default must be true. Set to false only for testing purposes.");
DEFINE_string(membership_info_file_path,
              ::stratum::kDefaultMembershipInfoFilePath,
              "Path to MembershipInfo proto. Used only if "
              "FLAGS_enable_authorization is true.");
DEFINE_string(auth_policy_file_path, ::stratum::kDefaultAuthPolicyFilePath,
              "Path to AuthorizationPolicy proto. Used only if "
              "FLAGS_enable_authorization is true.");
DEFINE_int32(file_change_poll_timeout_ms, 100,
             "Time in ms used as the timeout for file event polling.");

namespace stratum {

constexpr char AuthPolicyChecker::kDefaultRpc[];

namespace {

// A helper function that reads a proto message from a text file if a valid
// file exists, otherwise just returns an error and cleara the given message.
void ReadProtoIfValidFileExists(const std::string& path,
                                ::google::protobuf::Message* message) {
  if (PathExists(path)) {
    ::util::Status status = ReadProtoFromTextFile(path, message);
    if (!status.ok()) {
      LOG(ERROR) << "Invalid/corrupted file at '" << path << ": "
                 << status.error_message();
      message->Clear();
    }
  } else {
    LOG(ERROR) << "File '" << path << "' not found.";
  }
}

}  // namespace

AuthPolicyChecker::AuthPolicyChecker()
    : watcher_thread_id_(),
      shutdown_(false),
      per_service_per_rpc_authorized_users_() {}

AuthPolicyChecker::~AuthPolicyChecker() {}

::util::Status AuthPolicyChecker::Authorize(
    const std::string& service_name, const std::string& rpc_name,
    const ::grpc::AuthContext& auth_context) const {
  // TODO(unknown): Implement this.

  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::RefreshPolicies() {
  // TODO(unknown): Implement this.

  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::Shutdown() {
  {
    absl::WriterMutexLock l(&shutdown_lock_);
    if (shutdown_) return ::util::OkStatus();
    shutdown_ = true;
  }
  if (watcher_thread_id_ && pthread_join(watcher_thread_id_, nullptr) != 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "Failed to join file watcher thread.";
  }
  watcher_thread_id_ = pthread_t{};

  return ::util::OkStatus();
}

std::unique_ptr<AuthPolicyChecker> AuthPolicyChecker::CreateInstance() {
  auto instance = absl::WrapUnique(new AuthPolicyChecker());
  ::util::Status status = instance->Initialize();
  if (!status.ok()) {
    LOG(ERROR) << "Failed to initialize the AuthPolicyChecker instance: "
               << status.error_message();
    return nullptr;
  }

  return instance;
}

::util::Status AuthPolicyChecker::Initialize() {
  int ret =
      pthread_create(&watcher_thread_id_, nullptr, WatcherThreadFunc, nullptr);
  if (ret) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to create file watcher thread with error " << ret << ".";
  }

  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::AuthorizeUser(
    const std::string& service_name, const std::string& rpc_name,
    const std::string& username) const {
  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::WatchForFileChange() {
  return ::util::OkStatus();
}

void* AuthPolicyChecker::WatcherThreadFunc(void* arg) { return nullptr; }

}  // namespace stratum
