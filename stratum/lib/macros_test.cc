// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/macros.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {

using ::testing::HasSubstr;

class CommonMacrosTest : public ::testing::Test {
 protected:
  ::util::Status FuncWithCheckReturnIfFalse(bool cond, const std::string& msg) {
    RET_CHECK(cond) << msg;
    return ::util::OkStatus();
  }

  ::util::Status FuncWithReturnIfError(bool cond, const std::string& msg) {
    RETURN_IF_ERROR(FuncWithCheckReturnIfFalse(cond, msg));
    return ::util::OkStatus();
  }

  ::util::Status FuncWithMakeError(const std::string& msg) {
    return MAKE_ERROR(ERR_INTERNAL) << msg;
  }
};

TEST_F(CommonMacrosTest, AppendStatusIfError) {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status,
                         FuncWithReturnIfError(false, "Message One!     \n"));
  APPEND_STATUS_IF_ERROR(
      status, FuncWithReturnIfError(true, "Message ignored!     \n"));
  APPEND_STATUS_IF_ERROR(status, FuncWithMakeError("Message Two     "));
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_EQ(StratumErrorSpace(), status.error_space());
  EXPECT_THAT(status.error_message(),
              HasSubstr("'cond' is false. Message One! Message Two. "));
}

}  // namespace stratum
