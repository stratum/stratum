// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Unit tests for p4_utils.

#include "stratum/hal/lib/tdi/utils.h"

#include "absl/strings/substitute.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {
namespace tdi {

using ::testing::HasSubstr;

TEST(DefaultRangeLowValueTest, HasFullBitwidth) {
  EXPECT_EQ(RangeDefaultLow(0).size(), 0);
  EXPECT_EQ(RangeDefaultLow(1).size(), 1);
  EXPECT_EQ(RangeDefaultLow(7).size(), 1);
  EXPECT_EQ(RangeDefaultLow(8).size(), 1);
  EXPECT_EQ(RangeDefaultLow(9).size(), 2);
  EXPECT_EQ(RangeDefaultLow(16).size(), 2);
}

TEST(RangeDefaultHighValueTest, HasFullBitwidth) {
  EXPECT_EQ(RangeDefaultHigh(0).size(), 0);
  EXPECT_EQ(RangeDefaultHigh(1).size(), 1);
  EXPECT_EQ(RangeDefaultHigh(7).size(), 1);
  EXPECT_EQ(RangeDefaultHigh(8).size(), 1);
  EXPECT_EQ(RangeDefaultHigh(9).size(), 2);
  EXPECT_EQ(RangeDefaultHigh(16).size(), 2);
}

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
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00\x40\x00", 3);
    m.set_high("\x03\xFF\xFF", 3);
    EXPECT_FALSE(IsDontCareMatch(m, 18)) << m.DebugString();
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
  auto tdi_priority = ConvertPriorityFromP4rtToTdi(kP4rtPriority);
  EXPECT_OK(tdi_priority);
  EXPECT_EQ(tdi_priority.ValueOrDie(), 0xfffffe);
  auto p4rt_priority_from_tdi =
      ConvertPriorityFromTdiToP4rt(tdi_priority.ValueOrDie());
  EXPECT_OK(p4rt_priority_from_tdi);
  EXPECT_EQ(kP4rtPriority, p4rt_priority_from_tdi.ValueOrDie());
}

TEST(ConvertPriorityTest, ToAndFromTdi) {
  const uint64 kTdiPriority = 1;
  auto p4rt_priority = ConvertPriorityFromTdiToP4rt(kTdiPriority);
  EXPECT_OK(p4rt_priority);
  EXPECT_EQ(p4rt_priority.ValueOrDie(), 0xfffffe);
  auto tdi_priority_from_p4rt =
      ConvertPriorityFromTdiToP4rt(p4rt_priority.ValueOrDie());
  EXPECT_OK(tdi_priority_from_p4rt);
  EXPECT_EQ(kTdiPriority, tdi_priority_from_p4rt.ValueOrDie());
}

TEST(ConvertPriorityTest, InvalidP4rtPriority) {
  {
    auto result = ConvertPriorityFromP4rtToTdi(0x1000000);
    EXPECT_THAT(result.status().error_code(),
                stratum::ErrorCode::ERR_INVALID_PARAM);
  }
  {
    auto result = ConvertPriorityFromP4rtToTdi(-1);
    EXPECT_THAT(result.status().error_code(),
                stratum::ErrorCode::ERR_INVALID_PARAM);
  }
}

TEST(ConvertPriorityTest, InvalidTdiPriority) {
  auto result = ConvertPriorityFromTdiToP4rt(0x1000000);
  EXPECT_THAT(result.status().error_code(),
              stratum::ErrorCode::ERR_INVALID_PARAM);
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
