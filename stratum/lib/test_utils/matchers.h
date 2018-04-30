/*
 * Copyright 2018 Google LLC
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

#ifndef STRATUM_LIB_TEST_UTILS_MATCHERS_H_
#define STRATUM_LIB_TEST_UTILS_MATCHERS_H_

#include <ostream>  // NOLINT
#include <string>
#include <type_traits>

#include "google/protobuf/util/message_differencer.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

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

// Monomorphic implementation of a matcher for a StatusOr.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  using ValueType = typename std::remove_reference<decltype(
      std::declval<StatusOrType>().ValueOrDie())>::type;

  template <typename InnerMatcher>
  explicit IsOkAndHoldsMatcherImpl(InnerMatcher&& inner_matcher)
      : inner_matcher_(::testing::SafeMatcherCast<const ValueType&>(
            std::forward<InnerMatcher>(inner_matcher))) {}

  void DescribeTo(std::ostream* os) const {
    *os << "is OK and has a value that ";
    inner_matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't OK or has a value that ";
    inner_matcher_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(StatusOrType actual_value,
                       ::testing::MatchResultListener* listener) const {
    if (!actual_value.ok()) {
      *listener << "which has status " << actual_value.status();
      return false;
    }

    ::testing::StringMatchResultListener inner_listener;
    const bool matches = inner_matcher_.MatchAndExplain(
        actual_value.ValueOrDie(), &inner_listener);
    const string inner_explanation = inner_listener.str();
    if (!inner_explanation.empty()) {
      *listener << "which contains value "
                << ::testing::PrintToString(actual_value.ValueOrDie()) << ", "
                << inner_explanation;
    }
    return matches;
  }

 private:
  const ::testing::Matcher<const ValueType&> inner_matcher_;
};

// Implements IsOkAndHolds() as a polymorphic matcher.
template <typename InnerMatcher>
class IsOkAndHoldsMatcher {
 public:
  explicit IsOkAndHoldsMatcher(InnerMatcher inner_matcher)
      : inner_matcher_(std::move(inner_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic one of the given type.
  // StatusOrType can be either StatusOr<T> or a reference to StatusOr<T>.
  template <typename StatusOrType>
  operator ::testing::Matcher<StatusOrType>() const {
    return ::testing::MakeMatcher(
        new IsOkAndHoldsMatcherImpl<StatusOrType>(inner_matcher_));
  }

 private:
  const InnerMatcher inner_matcher_;
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

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
matchers_internal::IsOkAndHoldsMatcher<typename std::decay<InnerMatcher>::type>
IsOkAndHolds(InnerMatcher&& inner_matcher) {
  return matchers_internal::IsOkAndHoldsMatcher<
      typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

}  // namespace test_utils
}  // namespace stratum

#endif  // STRATUM_LIB_TEST_UTILS_MATCHERS_H_
