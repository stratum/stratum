// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/lib/security/auth_policy_checker.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/types.h>

#include "gflags/gflags.h"
#include "google/protobuf/message.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/gtl/map_util.h"

// TODO(unknown): Set the default to true when feature is fully available.
DEFINE_bool(enable_authorization, false,
            "Whether to enable per service per RPC authorization checking. The "
            "default must be true. Set to false only for testing purposes.");
DEFINE_string(membership_info_file_path,
              ::stratum::kDefaultMembershipInfoFilePath,
              "Path to MembershipInfo proto. Used only if "
              "FLAGS_enable_authorization is true.");
DEFINE_string(auth_policy_file_path,
              ::stratum::kDefaultAuthPolicyFilePath,
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

// Helpers for adding and removing watch for a file/dir.
::util::StatusOr<int> AddWatchHelper(int fd, const std::string& path,
                                     uint32 mask) {
  int wd = inotify_add_watch(fd, path.c_str(), mask);
  if (wd <= 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "inotify_add_watch() failed for path '" << path << "', and mask '"
           << mask << "'. errno: " << errno << ".";
  }

  return wd;
}

::util::Status RemoveWatchHelper(int fd, int wd) {
  CHECK_RETURN_IF_FALSE(fd > 0) << "Invalid fd: " << fd << ".";
  if (wd > 0) inotify_rm_watch(fd, wd);

  return ::util::OkStatus();
}

// This method creates watch descritor for watching change in the directory
// containing the file whose path is given as input. We add watch for the
// directory so that we can detect the file creation/deletion/move as well as
// modify.
::util::StatusOr<int> AddWatchForFileChange(int ifd, const std::string& path) {
  std::string dir = DirName(path);
  CHECK_RETURN_IF_FALSE(PathExists(dir)) << "Dir '" << dir << "' not found.";
  CHECK_RETURN_IF_FALSE(IsDir(dir)) << "'" << dir << "' is not a directory.";
  ASSIGN_OR_RETURN(
      int wd,
      AddWatchHelper(ifd, dir, IN_CREATE | IN_DELETE | IN_MOVE | IN_MODIFY));

  return wd;
}

// This method create an epoll file descriptor which will later be used to check
// for the file/dir change in a non blocking mannger. The FD created by
// inotify_init() is passed to this method.
::util::StatusOr<int> AddPollForFileChange(int ifd) {
  struct epoll_event event;
  int efd = epoll_create1(0);
  if (efd <= 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "epoll_create1() failed. errno: " << errno << ".";
  }

  event.data.fd = ifd;  // not even used.
  event.events = EPOLLIN;
  if (epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &event) != 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "epoll_ctl() failed. errno: " << errno << ".";
  }

  return efd;
}

// Pretty prints the filed event on a specific file.
void PrintFileEvent(const std::string& path, uint32 mask) {
  if (mask & IN_CREATE) {
    LOG(INFO) << "File '" << path << "' created!";
  } else if (mask & IN_MODIFY) {
    LOG(INFO) << "File '" << path << "' modified!";
  } else if (mask & IN_MOVE) {
    LOG(INFO) << "File '" << path << "' moved!";
  } else if (mask & IN_DELETE) {
    LOG(INFO) << "File '" << path << "' deleted!";
  } else {
    LOG(WARNING) << "Unknown event on file '" << path << "'!";
  }
}

}  // namespace

AuthPolicyChecker::AuthPolicyChecker()
    : watcher_thread_id_(0),
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
  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::AuthorizeUser(
    const std::string& service_name, const std::string& rpc_name,
    const std::string& username) const {
  return ::util::OkStatus();
}

::util::Status AuthPolicyChecker::WatchForFileChange() {
  return ::util::OkStatus();;
}

void* AuthPolicyChecker::WatcherThreadFunc(void* arg) {
  return nullptr;
}

}  // namespace stratum
