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

#include "stratum/glue/gtl/node_hash_set.h"

#include "gtest/gtest.h"

namespace stratum {
namespace gtl {
namespace {

TEST(node_hash_set, size) {
  node_hash_set<int> set;
  EXPECT_TRUE(set.empty());
  EXPECT_EQ(0, set.size());
  set.insert(7);
  EXPECT_FALSE(set.empty());
  EXPECT_EQ(1, set.size());
}

}  // <empty>
}  // namespace gtl
}  // namespace stratum
