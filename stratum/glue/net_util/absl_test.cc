// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Tests for absl behavior

#include <stdint.h>

#include "gtest/gtest.h"
#include "absl/strings/numbers.h"

using absl::numbers_internal::safe_strto32_base;

TEST(absl, safe_strto32_base) {
    const std::string string1 = " 1";
    const std::string string2 = "2";
    const std::string string3 = "3 ";

    int32_t value;
    EXPECT_TRUE(safe_strto32_base(string1, &value, 10));
    EXPECT_TRUE(safe_strto32_base(string1, &value, 10));
    /*
     FIXME Abseil public vs. Google-internal may differ in behavior on this test
     This behavioral difference seems to be allowing StringToIPRange to accept
     strings will trailing spaces, which the unittest indicates should be invalid.
    */
    // expected based on other tests
    // EXPECT_FALSE(safe_strto32_base(string1, &value, 10));
    // actual behavior
    EXPECT_TRUE(safe_strto32_base(string1, &value, 10));
}
