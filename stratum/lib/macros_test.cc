// Copyright 2018 Google LLC
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


#include "stratum/lib/macros.h"

#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::HasSubstr;

namespace stratum {

class CommonMacrosTest : public ::testing::Test {
 protected:
  ::util::Status FuncWithCheckReturnIfFalse(bool cond, const std::string& msg) {
    CHECK_RETURN_IF_FALSE(cond) << msg;
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
  APPEND_STATUS_IF_ERROR(
      status, FuncWithReturnIfError(false, "Message One!     \n"));
  APPEND_STATUS_IF_ERROR(
      status, FuncWithReturnIfError(true, "Message ignored!     \n"));
  APPEND_STATUS_IF_ERROR(
      status, FuncWithMakeError("Message Two     "));
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_EQ(StratumErrorSpace(), status.error_space());
  EXPECT_THAT(
      status.error_message(),
      HasSubstr("'cond' is false. Message One! Message Two. "));
}

}  // namespace stratum
