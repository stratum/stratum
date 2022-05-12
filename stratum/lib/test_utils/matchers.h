// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_LIB_TEST_UTILS_MATCHERS_H_
#define STRATUM_LIB_TEST_UTILS_MATCHERS_H_

#include <ostream>  // NOLINT
#include <string>
#include <type_traits>

// FIXME(boc) already included
#include "gmock/gmock-matchers.h"  // NOLINT
#include "gmock/gmock.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"

namespace stratum {
namespace test_utils {
namespace matchers_internal {

template <typename T>
class ProtoMatcher {
 public:
  ProtoMatcher(const T& expected, bool as_set, bool partial)
      : expected_(expected), as_set_(as_set), partial_(partial) {}

  bool MatchAndExplain(const T& m,
                       ::testing::MatchResultListener* listener) const {
    ::google::protobuf::util::MessageDifferencer differencer;
    if (as_set_) {
      differencer.set_repeated_field_comparison(
          ::google::protobuf::util::MessageDifferencer::AS_SET);
    }
    if (partial_) {
      differencer.set_scope(
          ::google::protobuf::util::MessageDifferencer::PARTIAL);
    }
    std::string result;
    differencer.ReportDifferencesToString(&result);
    if (differencer.Compare(expected_, m)) return true;
    *listener << "\nActual:\n" << m.DebugString();
    *listener << "\nDifference:\n" << result;
    return false;
  }

  void DescribeTo(::std::ostream* os) const {
    *os << "Equals proto:\n" << expected_.DebugString();
  }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "Does not equal proto:\n" << expected_.DebugString();
  }

 protected:
  const T expected_;  // Copy of the expected protobuf.
  bool as_set_;       // Treat repeated messages as a set (unordered).
  bool partial_;      // Perform a partial match on the expected protobuf.
};

}  // namespace matchers_internal

// Returns true if the protobuf matches the provided protobuf.
template <typename T>
inline ::testing::PolymorphicMatcher<matchers_internal::ProtoMatcher<T>>
EqualsProto(const T& proto) {
  return ::testing::MakePolymorphicMatcher(
      matchers_internal::ProtoMatcher<T>(proto, false, false));
}

// Returns true if the protobuf matches the provided protobuf regardless of
// order in any repeated fields.
template <typename T>
inline ::testing::PolymorphicMatcher<matchers_internal::ProtoMatcher<T>>
UnorderedEqualsProto(const T& proto) {
  return ::testing::MakePolymorphicMatcher(
      matchers_internal::ProtoMatcher<T>(proto, true, false));
}

// Returns true if the protobuf matches the provided protobuf.
template <typename T>
inline ::testing::PolymorphicMatcher<matchers_internal::ProtoMatcher<T>>
PartiallyEqualsProto(const T& proto) {
  return ::testing::MakePolymorphicMatcher(
      matchers_internal::ProtoMatcher<T>(proto, false, true));
}

// Returns true if the protobuf matches the provided protobuf regardless of
// order in any repeated fields.
template <typename T>
inline ::testing::PolymorphicMatcher<matchers_internal::ProtoMatcher<T>>
PartiallyUnorderedEqualsProto(const T& proto) {
  return ::testing::MakePolymorphicMatcher(
      matchers_internal::ProtoMatcher<T>(proto, true, true));
}

}  // namespace test_utils
}  // namespace stratum

#endif  // STRATUM_LIB_TEST_UTILS_MATCHERS_H_
