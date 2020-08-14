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
    m.set_low("\x00", 1);
    m.set_high("\xff\xff", 2);
    EXPECT_TRUE(IsDontCareMatch(m, 16)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
    m.set_high("\x0f\xff", 2);
    EXPECT_FALSE(IsDontCareMatch(m, 16)) << m.DebugString();
  }
  {
    ::p4::v1::FieldMatch::Range m;
    m.set_low("\x00", 1);
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
