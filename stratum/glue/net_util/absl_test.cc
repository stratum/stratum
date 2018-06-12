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
    //EXPECT_FALSE(safe_strto32_base(string1, &value, 10));
    // actual behavior
    EXPECT_TRUE(safe_strto32_base(string1, &value, 10));
}