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

#include "grpcpp/grpcpp.h"

#include <string>
#include <vector>

#include "stratum/public/lib/error.h"

#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "google/rpc/code.pb.h"
#include "google/rpc/status.pb.h"

namespace stratum {

namespace {

TEST(ErrorTest, TestSingleton) {
  const ::util::ErrorSpace* error_space = StratumErrorSpace();

  // Test Error space can find the singleton instance by its name.
  EXPECT_TRUE(error_space != nullptr);
  EXPECT_EQ(error_space, ::util::ErrorSpace::Find(error_space->SpaceName()));

  // Verify singleton behavior. Multiple calls to the singleton constructor will
  // return the same pointer.
  EXPECT_EQ(error_space, StratumErrorSpace());
  EXPECT_EQ(error_space, StratumErrorSpace());
}

// TODO(unknown): We dont have the fancy google3 proto utils which allows for
// looping around enum values. Investigate if we can port them to depot3 or
// we move to google3.
TEST(ErrorTest, TestErrorCodes) {
  const ::util::ErrorSpace* error_space = StratumErrorSpace();
  ::util::Status status;

  // Test success
  status = ::util::Status(error_space, ERR_SUCCESS, "Test");
  EXPECT_OK(status);

  // Test all the other errors.
  for (const ErrorCode c : std::vector<ErrorCode>({ERR_CANCELLED,
                                                   ERR_UNKNOWN,
                                                   ERR_PERMISSION_DENIED,
                                                   ERR_FAILED_PRECONDITION,
                                                   ERR_ABORTED,
                                                   ERR_OUT_OF_RANGE,
                                                   ERR_UNIMPLEMENTED,
                                                   ERR_INTERNAL,
                                                   ERR_DATA_LOSS,
                                                   ERR_UNAUTHENTICATED,
                                                   ERR_TABLE_FULL,
                                                   ERR_TABLE_EMPTY,
                                                   ERR_HARDWARE_ERROR,
                                                   ERR_INVALID_PARAM,
                                                   ERR_ENTRY_NOT_FOUND,
                                                   ERR_ENTRY_EXISTS,
                                                   ERR_OPER_NOT_SUPPORTED,
                                                   ERR_OPER_DISABLED,
                                                   ERR_OPER_TIMEOUT,
                                                   ERR_OPER_STILL_RUNNING,
                                                   ERR_REBOOT_REQUIRED,
                                                   ERR_FEATURE_UNAVAILABLE,
                                                   ERR_NOT_INITIALIZED,
                                                   ERR_NO_RESOURCE,
                                                   ERR_FILE_NOT_FOUND,
                                                   ERR_AT_LEAST_ONE_OPER_FAILED,
                                                   ERR_INVALID_P4_INFO,
                                                   ERR_NO_OP})) {
    status = ::util::Status(error_space, c, "Test");
    EXPECT_NE(::util::OkStatus(), status);
    EXPECT_EQ("Test", status.error_message());
  }
}

TEST(ErrorTest, TestMakeErrorMacro) {
  for (const ErrorCode c : std::vector<ErrorCode>({ERR_CANCELLED,
                                                   ERR_UNKNOWN,
                                                   ERR_PERMISSION_DENIED,
                                                   ERR_FAILED_PRECONDITION,
                                                   ERR_ABORTED,
                                                   ERR_OUT_OF_RANGE,
                                                   ERR_UNIMPLEMENTED,
                                                   ERR_INTERNAL,
                                                   ERR_DATA_LOSS,
                                                   ERR_UNAUTHENTICATED,
                                                   ERR_TABLE_FULL,
                                                   ERR_TABLE_EMPTY,
                                                   ERR_HARDWARE_ERROR,
                                                   ERR_INVALID_PARAM,
                                                   ERR_ENTRY_NOT_FOUND,
                                                   ERR_ENTRY_EXISTS,
                                                   ERR_OPER_NOT_SUPPORTED,
                                                   ERR_OPER_DISABLED,
                                                   ERR_OPER_TIMEOUT,
                                                   ERR_OPER_STILL_RUNNING,
                                                   ERR_REBOOT_REQUIRED,
                                                   ERR_FEATURE_UNAVAILABLE,
                                                   ERR_NOT_INITIALIZED,
                                                   ERR_NO_RESOURCE,
                                                   ERR_FILE_NOT_FOUND,
                                                   ERR_AT_LEAST_ONE_OPER_FAILED,
                                                   ERR_INVALID_P4_INFO,
                                                   ERR_NO_OP})) {
    ::util::Status status = MAKE_ERROR(c) << "Test";
    EXPECT_NE(::util::OkStatus(), status);
    EXPECT_EQ("Test", status.error_message());
    EXPECT_EQ(c, status.error_code());
  }
}

void TestCanonicalCodesHelper(ErrorCode stratum_error_code,
                              ::grpc::StatusCode grpc_error_code,
                              ::google::rpc::Code google_rpc_error_code) {
  const std::string kErrorMsg = "some error!";
  ::util::Status util_status(StratumErrorSpace(), stratum_error_code,
                             kErrorMsg);

  ::grpc::Status grpc_status(
      static_cast<::grpc::StatusCode>(util_status.CanonicalCode()),
      util_status.error_message());

  ::google::rpc::Status google_rpc_status;
  google_rpc_status.set_code(
      static_cast<::google::rpc::Code>(util_status.CanonicalCode()));

  EXPECT_EQ(grpc_error_code, grpc_status.error_code());
  EXPECT_EQ(kErrorMsg, grpc_status.error_message());
  EXPECT_EQ(google_rpc_error_code, google_rpc_status.code());
}

TEST(CommonUtilsTest, TestCanonicalCodes) {
  TestCanonicalCodesHelper(ERR_CANCELLED, ::grpc::StatusCode::CANCELLED,
                           ::google::rpc::CANCELLED);
  TestCanonicalCodesHelper(ERR_UNKNOWN, ::grpc::StatusCode::UNKNOWN,
                           ::google::rpc::UNKNOWN);
  TestCanonicalCodesHelper(ERR_PERMISSION_DENIED,
                           ::grpc::StatusCode::PERMISSION_DENIED,
                           ::google::rpc::PERMISSION_DENIED);
  TestCanonicalCodesHelper(ERR_FAILED_PRECONDITION,
                           ::grpc::StatusCode::FAILED_PRECONDITION,
                           ::google::rpc::FAILED_PRECONDITION);
  TestCanonicalCodesHelper(ERR_ABORTED, ::grpc::StatusCode::ABORTED,
                           ::google::rpc::ABORTED);
  TestCanonicalCodesHelper(ERR_OUT_OF_RANGE, ::grpc::StatusCode::OUT_OF_RANGE,
                           ::google::rpc::OUT_OF_RANGE);
  TestCanonicalCodesHelper(ERR_UNIMPLEMENTED, ::grpc::StatusCode::UNIMPLEMENTED,
                           ::google::rpc::UNIMPLEMENTED);
  TestCanonicalCodesHelper(ERR_DATA_LOSS, ::grpc::StatusCode::DATA_LOSS,
                           ::google::rpc::DATA_LOSS);
// FIXME(boc)   UNAUTHENTICATED is not defined in grpc's status_code_enum.h or
// googleapi's code.proto
//  TestCanonicalCodesHelper(ERR_UNAUTHENTICATED,
//                           ::grpc::StatusCode::UNAUTHENTICATED,
//                           ::google::rpc::UNAUTHENTICATED);
  TestCanonicalCodesHelper(ERR_INTERNAL, ::grpc::StatusCode::INTERNAL,
                           ::google::rpc::INTERNAL);
  TestCanonicalCodesHelper(ERR_HARDWARE_ERROR, ::grpc::StatusCode::INTERNAL,
                           ::google::rpc::INTERNAL);
  TestCanonicalCodesHelper(ERR_INVALID_PARAM,
                           ::grpc::StatusCode::INVALID_ARGUMENT,
                           ::google::rpc::INVALID_ARGUMENT);
  TestCanonicalCodesHelper(ERR_INVALID_P4_INFO,
                           ::grpc::StatusCode::INVALID_ARGUMENT,
                           ::google::rpc::INVALID_ARGUMENT);
  TestCanonicalCodesHelper(ERR_OPER_TIMEOUT,
                           ::grpc::StatusCode::DEADLINE_EXCEEDED,
                           ::google::rpc::DEADLINE_EXCEEDED);
  TestCanonicalCodesHelper(ERR_ENTRY_NOT_FOUND, ::grpc::StatusCode::NOT_FOUND,
                           ::google::rpc::NOT_FOUND);
  TestCanonicalCodesHelper(ERR_ENTRY_EXISTS, ::grpc::StatusCode::ALREADY_EXISTS,
                           ::google::rpc::ALREADY_EXISTS);
  TestCanonicalCodesHelper(ERR_OPER_NOT_SUPPORTED,
                           ::grpc::StatusCode::UNIMPLEMENTED,
                           ::google::rpc::UNIMPLEMENTED);
  TestCanonicalCodesHelper(ERR_OPER_DISABLED, ::grpc::StatusCode::UNIMPLEMENTED,
                           ::google::rpc::UNIMPLEMENTED);
  TestCanonicalCodesHelper(ERR_FEATURE_UNAVAILABLE,
                           ::grpc::StatusCode::UNAVAILABLE,
                           ::google::rpc::UNAVAILABLE);
  TestCanonicalCodesHelper(ERR_NO_RESOURCE,
                           ::grpc::StatusCode::RESOURCE_EXHAUSTED,
                           ::google::rpc::RESOURCE_EXHAUSTED);
  TestCanonicalCodesHelper(ERR_NOT_INITIALIZED,
                           ::grpc::StatusCode::FAILED_PRECONDITION,
                           ::google::rpc::FAILED_PRECONDITION);
  TestCanonicalCodesHelper(ERR_TABLE_FULL, ::grpc::StatusCode::OUT_OF_RANGE,
                           ::google::rpc::OUT_OF_RANGE);
  TestCanonicalCodesHelper(ERR_TABLE_EMPTY, ::grpc::StatusCode::OUT_OF_RANGE,
                           ::google::rpc::OUT_OF_RANGE);
  TestCanonicalCodesHelper(ERR_REBOOT_REQUIRED, ::grpc::StatusCode::UNKNOWN,
                           ::google::rpc::UNKNOWN);
  TestCanonicalCodesHelper(ERR_AT_LEAST_ONE_OPER_FAILED,
                           ::grpc::StatusCode::UNKNOWN, ::google::rpc::UNKNOWN);
}

}  // namespace

}  // namespace stratum
