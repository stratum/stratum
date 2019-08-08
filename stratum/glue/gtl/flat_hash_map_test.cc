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

#include "stratum/glue/gtl/flat_hash_map.h"

#include "gtest/gtest.h"

namespace stratum {
namespace gtl {
namespace {

TEST(flat_hash_map, size) {
  flat_hash_map<int, int> map;
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(0, map.size());
  map[1] = 1;
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(1, map.size());
}

}  // namespace
}  // namespace gtl
}  // namespace stratum
