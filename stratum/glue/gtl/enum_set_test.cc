/*
 * Copyright 2018 Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stratum/glue/gtl/enum_set.h"

#include "gtest/gtest.h"

namespace stratum {
namespace gtl {
namespace {

enum Color {
  GREEN = 1,
  YELLOW = 2,
  RED = 3,
};

TEST(enum_set, size) {
  enum_set<Color> set;
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(0, set.size());
  set.insert(YELLOW);
  EXPECT_FALSE(set.empty());
  EXPECT_EQ(1, set.size());
}

}  // namespace
}  // namespace gtl
}  // namespace stratum
