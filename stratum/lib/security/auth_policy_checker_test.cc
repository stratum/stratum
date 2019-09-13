// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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


#include <stdio.h>

#include <string>
#include <vector>
#include <map>

#include "stratum/lib/security/auth_policy_checker.h"
#include "gflags/gflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/public/proto/error.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

DECLARE_bool(enable_authorization);
DECLARE_string(membership_info_file_path);
DECLARE_string(auth_policy_file_path);
DECLARE_string(test_tmpdir);

namespace stratum {

using ::testing::Return;
using ::testing::StrictMock;

namespace {

class MockAuthContext : public ::grpc::AuthContext {
 public:
  ~MockAuthContext() override {}
  MOCK_CONST_METHOD0(IsPeerAuthenticated, bool());
  MOCK_CONST_METHOD0(GetPeerIdentity, std::vector<::grpc::string_ref>());
  MOCK_CONST_METHOD0(GetPeerIdentityPropertyName, std::string());
  MOCK_CONST_METHOD1(FindPropertyValues,
                     std::vector<::grpc::string_ref>(const std::string& name));
  MOCK_CONST_METHOD0(begin, ::grpc::AuthPropertyIterator());
  MOCK_CONST_METHOD0(end, ::grpc::AuthPropertyIterator());
  MOCK_METHOD2(AddProperty,
               void(const std::string& key, const ::grpc::string_ref& value));
  MOCK_METHOD1(SetPeerIdentityPropertyName, bool(const std::string& name));
};

}  // namespace

class AuthPolicyCheckerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    FLAGS_enable_authorization = true;
    FLAGS_membership_info_file_path =
        FLAGS_test_tmpdir + "/membership_info.proto.txt";
    FLAGS_auth_policy_file_path =
        FLAGS_test_tmpdir + "/authorization_policy.proto.txt";
    if (PathExists(FLAGS_membership_info_file_path)) {
      ASSERT_OK(RemoveFile(FLAGS_membership_info_file_path));
    }
    if (PathExists(FLAGS_auth_policy_file_path)) {
      ASSERT_OK(RemoveFile(FLAGS_auth_policy_file_path));
    }
  }

  void TearDown() override {
    ASSERT_OK(auth_policy_checker_->Shutdown());
    ASSERT_EQ(0, auth_policy_checker_->watcher_thread_id_);
    absl::WriterMutexLock l(&auth_policy_checker_->shutdown_lock_);
    ASSERT_TRUE(auth_policy_checker_->shutdown_);
  }

  // Helper to initialize the class. The input 'start_watcher_thread' controls
  // whether we want to start the watcher thread or not.
  void Initialize(bool start_watcher_thread) {
    if (start_watcher_thread) {
      // CreateInstance() start the watcher thread as well.
      auth_policy_checker_ = AuthPolicyChecker::CreateInstance();
      ASSERT_TRUE(auth_policy_checker_ != nullptr);
      ASSERT_GT(auth_policy_checker_->watcher_thread_id_, 0);
    } else {
      auth_policy_checker_ = absl::WrapUnique(new AuthPolicyChecker());
      ASSERT_EQ(0, auth_policy_checker_->watcher_thread_id_);
    }
    absl::WriterMutexLock l(&auth_policy_checker_->shutdown_lock_);
    ASSERT_FALSE(auth_policy_checker_->shutdown_);
  }

  // Helper to check whether a given username is authorzied to use a given
  // RPC on a given service.
  void CheckAuthorization(
      const std::string& service_name, const std::string& rpc_name,
      const std::map<std::string, bool> username_to_expected) const {
    for (const auto& e : username_to_expected) {
      ::util::Status status =
          auth_policy_checker_->AuthorizeUser(service_name, rpc_name, e.first);
      if (e.second) {
        EXPECT_TRUE(status.ok());
      } else {
        EXPECT_FALSE(status.ok());
        EXPECT_EQ(ERR_PERMISSION_DENIED, status.error_code());
      }
    }
  }

  // TODO(unknown): Implement this.
  static constexpr char kAuthPolicyText1[] = "";

  std::unique_ptr<AuthPolicyChecker> auth_policy_checker_;
};

constexpr char AuthPolicyCheckerTest::kAuthPolicyText1[];

TEST_F(AuthPolicyCheckerTest, AuthorizeFailureForNonAuthenticatedPeer) {
  Initialize(/*start_watcher_thread=*/false);
  ASSERT_OK(WriteStringToFile(kAuthPolicyText1, FLAGS_auth_policy_file_path));
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());

  // TODO(unknown): Implement this.
}

TEST_F(AuthPolicyCheckerTest, AuthorizeFailureForEmptyContext) {
  Initialize(/*start_watcher_thread=*/false);
  ASSERT_OK(WriteStringToFile(kAuthPolicyText1, FLAGS_auth_policy_file_path));
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());

  // TODO(unknown): Implement this.
}

TEST_F(AuthPolicyCheckerTest, AuthorizeFailureForMoreThanOneIdentities) {
  Initialize(/*start_watcher_thread=*/false);
  ASSERT_OK(WriteStringToFile(kAuthPolicyText1, FLAGS_auth_policy_file_path));
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());

  // TODO(unknown): Implement this.
}

TEST_F(AuthPolicyCheckerTest, AuthorizeFailureForUnauthorizedUser) {
  Initialize(/*start_watcher_thread=*/false);
  ASSERT_OK(WriteStringToFile(kAuthPolicyText1, FLAGS_auth_policy_file_path));
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());

  // TODO(unknown): Implement this.
}

TEST_F(AuthPolicyCheckerTest, AuthorizeSuccess) {
  Initialize(/*start_watcher_thread=*/false);
  ASSERT_OK(WriteStringToFile(kAuthPolicyText1, FLAGS_auth_policy_file_path));
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());

  // TODO(unknown): Implement this.
}

TEST_F(AuthPolicyCheckerTest, RefreshPoliciesAndAuthorizeUser) {
  Initialize(/*start_watcher_thread=*/false);

  LOG(INFO) << "Started with no file.";
  ASSERT_OK(auth_policy_checker_->RefreshPolicies());
}

TEST_F(AuthPolicyCheckerTest, ShutdownMultipleTimes) {
  Initialize(/*start_watcher_thread=*/true);
  ASSERT_OK(auth_policy_checker_->Shutdown());
}

// TODO(unknown): Find a better way to deterministically wait for update.
TEST_F(AuthPolicyCheckerTest, WatchForFileChange) {
  Initialize(/*start_watcher_thread=*/true);

  // TODO(unknown): Implement this.
}

}  // namespace stratum
