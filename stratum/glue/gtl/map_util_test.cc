// Copyright 2015 The TensorFlow Authors. All Rights Reserved.
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/glue/gtl/map_util.h"

#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/container/node_hash_set.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"

namespace stratum {
namespace gtl {
namespace {

TEST(MapUtil, Find) {
  typedef std::map<std::string, std::string> Map;
  Map m;

  // Check that I can use a type that's implicitly convertible to the
  // key or value type, such as const char* -> string.
  EXPECT_EQ("", gtl::FindWithDefault(m, "foo", ""));
  m["foo"] = "bar";
  EXPECT_EQ("bar", gtl::FindWithDefault(m, "foo", ""));
  EXPECT_EQ("bar", *gtl::FindOrNull(m, "foo"));
  std::string str;
  EXPECT_GT(m.count("foo"), 0);
  EXPECT_EQ(m["foo"], "bar");
}

TEST(MapUtil, LookupOrInsert) {
  typedef std::map<std::string, std::string> Map;
  Map m;

  // Check that I can use a type that's implicitly convertible to the
  // key or value type, such as const char* -> string.
  EXPECT_EQ("xyz", gtl::LookupOrInsert(&m, "foo", "xyz"));
  EXPECT_EQ("xyz", gtl::LookupOrInsert(&m, "foo", "abc"));
}

TEST(MapUtil, InsertIfNotPresent) {
  // Set operations
  typedef std::set<int> Set;
  Set s;
  EXPECT_TRUE(gtl::InsertIfNotPresent(&s, 0));
  EXPECT_EQ(s.count(0), 1);
  EXPECT_FALSE(gtl::InsertIfNotPresent(&s, 0));
  EXPECT_EQ(s.count(0), 1);
}

TEST(MapUtil, FindOrDie) {
  std::map<std::string, int> map;
  EXPECT_DEATH(gtl::FindOrDie(map, "foo"), "");
  gtl::InsertOrDie(&map, "foo", 5);
  auto val = gtl::FindOrDie(map, "foo");
  EXPECT_EQ(val, 5);
  auto const cval = gtl::FindOrDie(map, "foo");
  EXPECT_EQ(cval, 5);
}

template <typename T>
class MapUtilSet : public ::testing::Test {
 public:
  T collection_;
};
using SetTypes =
    ::testing::Types<std::set<int>, std::unordered_set<int>,
                     ::absl::flat_hash_set<int>, ::absl::node_hash_set<int>>;
TYPED_TEST_SUITE(MapUtilSet, SetTypes);

TYPED_TEST(MapUtilSet, ContainsKey) {
  EXPECT_FALSE(gtl::ContainsKey(this->collection_, 0));
  ASSERT_TRUE(this->collection_.insert(0).second);
  EXPECT_TRUE(gtl::ContainsKey(this->collection_, 0));
}

TYPED_TEST(MapUtilSet, InsertOrDie) {
  gtl::InsertOrDie(&this->collection_, 0);
  gtl::ContainsKey(this->collection_, 0);
  EXPECT_DEATH(gtl::InsertOrDie(&this->collection_, 0), "");
}

template <typename T>
class MapUtilMap : public ::testing::Test {
 public:
  T collection_;
};
using MapTypes = ::testing::Types<std::map<int, std::string>,
                                  std::unordered_map<int, std::string>,
                                  ::absl::flat_hash_map<int, std::string>,
                                  ::absl::node_hash_map<int, std::string>>;
TYPED_TEST_SUITE(MapUtilMap, MapTypes);

TYPED_TEST(MapUtilMap, ContainsKey) {
  EXPECT_FALSE(gtl::ContainsKey(this->collection_, 0));
  ASSERT_TRUE(this->collection_.insert({0, "foo"}).second);
  EXPECT_TRUE(gtl::ContainsKey(this->collection_, 0));
}

TYPED_TEST(MapUtilMap, InsertOrDie) {
  gtl::InsertOrDie(&this->collection_, 0, "foo");
  gtl::ContainsKey(this->collection_, 0);
  EXPECT_DEATH(gtl::InsertOrDie(&this->collection_, 0, "foo"), "");
}

}  // namespace
}  // namespace gtl
}  // namespace stratum
