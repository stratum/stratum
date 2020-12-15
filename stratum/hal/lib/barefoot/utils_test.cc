// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

TEST(IsDontCareMatchTest, ClassifyLpmMatch) {
  {
    ::p4::v1::FieldMatch::LPM m;
    m.set_prefix_len(1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::LPM m;
    m.set_prefix_len(0);
    EXPECT_TRUE(IsDontCareMatch(m));
  }
}

TEST(IsDontCareMatchTest, ClassifyTernaryMatch) {
  {
    ::p4::v1::FieldMatch::Ternary m;
    m.set_mask("\xff", 1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Ternary m;
    m.set_mask("\x00", 1);
    EXPECT_TRUE(IsDontCareMatch(m));
  }
}

TEST(IsDontCareMatchTest, ClassifyRangeMatch) {
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\xff", 1);
    EXPECT_TRUE(IsDontCareMatch(m, 8)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\xff", 1);
    EXPECT_FALSE(IsDontCareMatch(m, 16)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\x00", 1);
    EXPECT_FALSE(IsDontCareMatch(m, 8)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\xff", 1);
    m.set_high("\xff", 1);
    EXPECT_FALSE(IsDontCareMatch(m, 8)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\x0f", 1);
    EXPECT_TRUE(IsDontCareMatch(m, 4)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x0f", 1);
    m.set_high("\x0f", 1);
    EXPECT_FALSE(IsDontCareMatch(m, 4)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("", 0);
    m.set_high("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 10);
    EXPECT_FALSE(IsDontCareMatch(m, 80)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("", 0);
    EXPECT_FALSE(IsDontCareMatch(m, 80)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("", 0);
    m.set_high("", 0);
    EXPECT_FALSE(IsDontCareMatch(m, 80)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 10);
    EXPECT_TRUE(IsDontCareMatch(m, 80)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);
    m.set_high("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 10);
    EXPECT_TRUE(IsDontCareMatch(m, 80)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 10);
    EXPECT_FALSE(IsDontCareMatch(m, 81)) << m.DebugString();
  }
}

TEST(IsDontCareMatchTest, RejectAllOptionalMatch) {
  {
    ::p4::v1::FieldMatch::Optional m;
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Optional m;
    m.set_value("\x00", 1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Optional m;
    m.set_value("\x00", 0);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
  {
    ::p4::v1::FieldMatch::Optional m;
    m.set_value("\xff", 1);
    EXPECT_FALSE(IsDontCareMatch(m));
  }
}

TEST(ConvertPriorityTest, ToAndFromP4Runtime) {
  const int32 kP4rtPriority = 1;
  auto bfrt_priority = ConvertPriorityFromP4rtToBfrt(kP4rtPriority);
  EXPECT_OK(bfrt_priority);
  EXPECT_EQ(bfrt_priority.ValueOrDie(), 0xfffffe);
  auto p4rt_priority_from_bfrt =
      ConvertPriorityFromBfrtToP4rt(bfrt_priority.ValueOrDie());
  EXPECT_OK(p4rt_priority_from_bfrt);
  EXPECT_EQ(kP4rtPriority, p4rt_priority_from_bfrt.ValueOrDie());
}

TEST(ConvertPriorityTest, ToAndFromBfrt) {
  const uint64 kBfrtPriority = 1;
  auto p4rt_priority = ConvertPriorityFromBfrtToP4rt(kBfrtPriority);
  EXPECT_OK(p4rt_priority);
  EXPECT_EQ(p4rt_priority.ValueOrDie(), 0xfffffe);
  auto bfrt_priority_from_p4rt =
      ConvertPriorityFromBfrtToP4rt(p4rt_priority.ValueOrDie());
  EXPECT_OK(bfrt_priority_from_p4rt);
  EXPECT_EQ(kBfrtPriority, bfrt_priority_from_p4rt.ValueOrDie());
}

TEST(ConvertPriorityTest, InvalidP4rtPriority) {
  {
    auto result = ConvertPriorityFromP4rtToBfrt(0x1000000);
    EXPECT_THAT(result.status().error_code(),
                stratum::ErrorCode::ERR_INVALID_PARAM);
  }
  {
    auto result = ConvertPriorityFromP4rtToBfrt(-1);
    EXPECT_THAT(result.status().error_code(),
                stratum::ErrorCode::ERR_INVALID_PARAM);
  }
}

TEST(ConvertPriorityTest, InvalidBfrtPriority) {
  auto result = ConvertPriorityFromBfrtToP4rt(0x1000000);
  EXPECT_THAT(result.status().error_code(),
              stratum::ErrorCode::ERR_INVALID_PARAM);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
