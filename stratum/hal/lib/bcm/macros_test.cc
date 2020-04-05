// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/bcm/macros.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::HasSubstr;

namespace stratum {
namespace hal {
namespace bcm {

class BcmMacrosTest : public ::testing::Test {
 protected:
  int FakeBcmFunc(int error_code) {
    return error_code;
  }

  ::util::Status FuncWithReturnIfBcmError(int error_code) {
    RETURN_IF_BCM_ERROR(FakeBcmFunc(error_code));
    return ::util::OkStatus();
  }
};

TEST_F(BcmMacrosTest, ReturnIfBcmError) {
  ::util::Status status = FuncWithReturnIfBcmError(BCM_E_PARAM);
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_EQ(StratumErrorSpace(), status.error_space());
  EXPECT_THAT(status.error_message(),
              HasSubstr("FakeBcmFunc(error_code)' failed with error message: "
                        "Invalid parameter. "));

  status = FuncWithReturnIfBcmError(BCM_E_NONE);
  EXPECT_OK(status);
  EXPECT_EQ("", status.error_message());
}

TEST_F(BcmMacrosTest, AppendStatusIfBcmErrorWithKnownError) {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(BCM_E_PARAM));
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(BCM_E_EXISTS));
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(BCM_E_NONE));
  APPEND_STATUS_IF_ERROR(status, FuncWithReturnIfBcmError(BCM_E_INTERNAL));
  EXPECT_EQ(ERR_INVALID_PARAM, status.error_code());
  EXPECT_EQ(StratumErrorSpace(), status.error_space());
  EXPECT_THAT(status.error_message(),
              HasSubstr("'FakeBcmFunc(BCM_E_PARAM)' failed with error message: "
                        "Invalid parameter. 'FakeBcmFunc(BCM_E_EXISTS)' failed "
                        "with error message: Entry exists. 'FakeBcmFunc("
                        "error_code)' failed with error message: Internal "
                        "error. "));
}

TEST_F(BcmMacrosTest, AppendStatusIfBcmErrorWithUnknownError) {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(-1000));  // Unknown code
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(BCM_E_EXISTS));
  APPEND_STATUS_IF_BCM_ERROR(status, FakeBcmFunc(BCM_E_NONE));
  EXPECT_EQ(ERR_UNKNOWN, status.error_code());
  EXPECT_EQ(StratumErrorSpace(), status.error_space());
  EXPECT_THAT(status.error_message(),
              HasSubstr("'FakeBcmFunc(-1000)' failed with error message: "
                        "Unknown error. 'FakeBcmFunc(BCM_E_EXISTS)' failed "
                        "with error message: Entry exists. "));
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
