// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Unit tests for p4_utils.

#include "stratum/hal/lib/barefoot/utils.h"

#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

using ::testing::HasSubstr;

namespace stratum {
namespace hal {
namespace barefoot {

TEST(IsDontCareMatchTest, RejectAllExactMatch) {
  {
    ::p4::v1::FieldMatch::Exact m;
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Exact m;
    m.set_value("\x00", 1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Exact m;
    m.set_value("\x00", 0);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Exact m;
    m.set_value("\xff", 1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
}
