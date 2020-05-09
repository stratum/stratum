// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/glue/status/status_test_util.h"

#include "stratum/glue/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"

TEST(StatusTestUtil, ExpectOkSuccess) { EXPECT_OK(::util::Status::OK); }

TEST(StatusTestUtil, AssertOkSuccess) { ASSERT_OK(::util::Status::OK); }

TEST(StatusTestUtil, ExpectOkFailure) {
  ::util::Status error_status(::util::error::UNKNOWN, "error_status_message");
  EXPECT_NONFATAL_FAILURE(EXPECT_OK(error_status), "error_status_message");
}

TEST(StatusTestUtil, AssertOkFailure) {
  EXPECT_FATAL_FAILURE(
      ASSERT_OK(::util::Status(::util::error::UNKNOWN, "error_status_message")),
      "error_status_message");
}
