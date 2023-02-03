// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_
#define STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_

#include <ostream>  // NOLINT
#include <string>
#include <type_traits>
#include <utility>

#include "absl/meta/type_traits.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace test_utils {
namespace matchers_internal {

inline const ::util::Status& GetStatus(const ::util::Status& status) {
  return status;
}

template <typename T>
inline const ::util::Status& GetStatus(const ::util::StatusOr<T>& status) {
  return status.status();
}

////////////////////////////////////////////////////////////
// Implementation of IsOk().

// Monomorphic implementation of matcher IsOk() for a given type T.
// T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoIsOkMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  void DescribeTo(std::ostream* os) const override { *os << "is OK"; }
  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not OK";
  }
  bool MatchAndExplain(T actual_value,
                       ::testing::MatchResultListener*) const override {
    return GetStatus(actual_value).ok();
  }
};

// Implements IsOk() as a polymorphic matcher.
class IsOkMatcher {
 public:
  template <typename T>
  operator ::testing::Matcher<T>() const {  // NOLINT
    return ::testing::MakeMatcher(new MonoIsOkMatcherImpl<T>());
  }
};

////////////////////////////////////////////////////////////
// Implementation of IsOkAndHolds().

// Monomorphic implementation of a matcher for a StatusOr.
template <typename StatusOrType>
class IsOkAndHoldsMatcherImpl
    : public ::testing::MatcherInterface<StatusOrType> {
 public:
  using ValueType = typename std::remove_reference<
      decltype(std::declval<StatusOrType>().ValueOrDie())>::type;

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
    const std::string inner_explanation = inner_listener.str();
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

////////////////////////////////////////////////////////////
// Implementation of StatusIs().

// An internal matcher for matching a const util::ErrorSpace* against
// an expected const util::ErrorSpace*.  It describes the expected
// error space by the symbolic name.
MATCHER_P(ErrorSpaceEq, expected_error_space,
          (negation ? "isn't <" : "is <") + expected_error_space->SpaceName() +
              ">") {
  return arg == expected_error_space;
}

template <typename E>
struct ErrorSpaceAdlTag {};

template <typename E, typename = void>
struct EnumHasErrorSpace : std::false_type {};

template <typename E>
struct EnumHasErrorSpace<
    E, absl::void_t<decltype(GetErrorSpace(ErrorSpaceAdlTag<E>{})),
                    typename std::enable_if<std::is_enum<E>::value>::type>>
    : std::true_type {};

template <typename E>
typename std::enable_if<EnumHasErrorSpace<E>::value,
                        const ::util::ErrorSpace*>::type
GetErrorSpaceForStatusIs(E e) {
  if (static_cast<int>(e) == 0) return ::util::Status::canonical_space();
  return GetErrorSpaceForEnum(e);
}

template <typename E>
typename std::enable_if<!EnumHasErrorSpace<E>::value,
                        const ::util::ErrorSpace*>::type
GetErrorSpaceForStatusIs(E) {
  return ::util::Status::canonical_space();
}

// ToErrorSpaceMatcher(m) converts m to a Matcher<const util::ErrorSpace*>.
//
// If m is a pointer to util::ErrorSpace or a subclass, the first
// overload is picked; otherwise the second overload is picked.
inline ::testing::Matcher<const ::util::ErrorSpace*> ToErrorSpaceMatcher(
    const ::util::ErrorSpace* space) {
  // Ensure that the expected error space is described by its symbolic name.
  return ErrorSpaceEq(space);
}
inline ::testing::Matcher<const ::util::ErrorSpace*> ToErrorSpaceMatcher(
    const ::testing::Matcher<const ::util::ErrorSpace*>& space_matcher) {
  return space_matcher;
}

MATCHER_P2(ErrorCodeEq, expected_error_code, error_space,
           (negation ? "isn't " : "is ") +
               error_space->String(expected_error_code)) {
  if (arg != expected_error_code) {
    *result_listener << error_space->String(arg);
    return false;
  }
  return true;
}

// Converts m to a Matcher<int>.  If m is an ErrorSpace enum, the first overload
// will be enabled and picked; if it is some other enum, the second overload is
// enabled and picked; otherwise the third overload will be picked.
template <typename T>
typename std::enable_if<EnumHasErrorSpace<T>::value,
                        ::testing::Matcher<int>>::type
ToCodeMatcher(T m) {
  return ErrorCodeEq(m, GetErrorSpaceForEnum(m));
}
template <typename T>
typename std::enable_if<std::is_enum<T>::value && !EnumHasErrorSpace<T>::value,
                        ::testing::Matcher<int>>::type
ToCodeMatcher(T m) {
  return ::testing::Eq(m);
}
inline ::testing::Matcher<int> ToCodeMatcher(const ::testing::Matcher<int>& m) {
  return m;
}

// StatusIs() is a polymorphic matcher.  This class is the common
// implementation of it shared by all types T where StatusIs() can be
// used as a Matcher<T>.
class StatusIsMatcherCommonImpl {
 public:
  StatusIsMatcherCommonImpl(
      ::testing::Matcher<const ::util::ErrorSpace*> space_matcher,
      ::testing::Matcher<int> code_matcher,
      ::testing::Matcher<const std::string&> message_matcher)
      : space_matcher_(std::move(space_matcher)),
        code_matcher_(std::move(code_matcher)),
        message_matcher_(std::move(message_matcher)) {}

  void DescribeTo(std::ostream* os) const;

  void DescribeNegationTo(std::ostream* os) const;

  bool MatchAndExplain(const ::util::Status& status,
                       ::testing::MatchResultListener* result_listener) const;

 private:
  const ::testing::Matcher<const ::util::ErrorSpace*> space_matcher_;
  const ::testing::Matcher<int> code_matcher_;
  const ::testing::Matcher<const std::string&> message_matcher_;
};

// Monomorphic implementation of matcher StatusIs() for a given type
// T.  T can be Status, StatusOr<>, or a reference to either of them.
template <typename T>
class MonoStatusIsMatcherImpl : public ::testing::MatcherInterface<T> {
 public:
  explicit MonoStatusIsMatcherImpl(StatusIsMatcherCommonImpl common_impl)
      : common_impl_(std::move(common_impl)) {}

  void DescribeTo(std::ostream* os) const override {
    common_impl_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    common_impl_.DescribeNegationTo(os);
  }

  bool MatchAndExplain(
      T actual_value,
      ::testing::MatchResultListener* result_listener) const override {
    return common_impl_.MatchAndExplain(GetStatus(actual_value),
                                        result_listener);
  }

 private:
  StatusIsMatcherCommonImpl common_impl_;
};

// Implements StatusIs() as a polymorphic matcher.
class StatusIsMatcher {
 public:
  template <typename ErrorSpaceMatcher, typename StatusCodeMatcher>
  StatusIsMatcher(ErrorSpaceMatcher&& space_matcher,
                  StatusCodeMatcher&& code_matcher,
                  ::testing::Matcher<const std::string&> message_matcher)
      : common_impl_(
            ToErrorSpaceMatcher(std::forward<ErrorSpaceMatcher>(space_matcher)),
            ToCodeMatcher(std::forward<StatusCodeMatcher>(code_matcher)),
            std::move(message_matcher)) {}

  // Converts this polymorphic matcher to a monomorphic matcher of the
  // given type.  T can be StatusOr<>, Status, or a reference to
  // either of them.
  template <typename T>
  operator ::testing::Matcher<T>() const {
    return MakeMatcher(new MonoStatusIsMatcherImpl<T>(common_impl_));
  }

 private:
  const StatusIsMatcherCommonImpl common_impl_;
};

}  // namespace matchers_internal

// Returns a gMock matcher that matches a Status or StatusOr<> which is OK.
inline matchers_internal::IsOkMatcher IsOk() {
  return matchers_internal::IsOkMatcher();
}

// Returns a gMock matcher that matches a StatusOr<> whose status is
// OK and whose value matches the inner matcher.
template <typename InnerMatcher>
inline matchers_internal::IsOkAndHoldsMatcher<
    typename std::decay<InnerMatcher>::type>
IsOkAndHolds(InnerMatcher&& inner_matcher) {
  return matchers_internal::IsOkAndHoldsMatcher<
      typename std::decay<InnerMatcher>::type>(
      std::forward<InnerMatcher>(inner_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> whose
// error space matches space_matcher, whose status code matches
// code_matcher, and whose error message matches message_matcher.
template <typename ErrorSpaceMatcher, typename StatusCodeMatcher>
inline matchers_internal::StatusIsMatcher StatusIs(
    ErrorSpaceMatcher&& space_matcher, StatusCodeMatcher&& code_matcher,
    ::testing::Matcher<const std::string&> message_matcher) {
  return matchers_internal::StatusIsMatcher(
      std::forward<ErrorSpaceMatcher>(space_matcher),
      std::forward<StatusCodeMatcher>(code_matcher),
      std::move(message_matcher));
}

// The one and two-arg StatusIs methods may infer the expected ErrorSpace from
// the StatusCodeMatcher argument. If you call StatusIs(e) or StatusIs(e, msg)
// and the argument `e` is:
// - an enum type,
// - which is associated with a custom ErrorSpace `S`,
// - and is not "OK" (i.e. 0),
// then the matcher will match a Status or StatusOr<> whose error space is `S`.
//
// Otherwise, the expected error space is the canonical error space.

// Returns a gMock matcher that matches a Status or StatusOr<> whose error space
// is the inferred error space (see above), whose status code matches
// code_matcher, and whose error message matches message_matcher.
template <typename StatusCodeMatcher>
inline matchers_internal::StatusIsMatcher StatusIs(
    StatusCodeMatcher&& code_matcher,
    ::testing::Matcher<const std::string&> message_matcher) {
  return matchers_internal::StatusIsMatcher(
      matchers_internal::GetErrorSpaceForStatusIs(code_matcher),
      std::forward<StatusCodeMatcher>(code_matcher),
      std::move(message_matcher));
}

// Returns a gMock matcher that matches a Status or StatusOr<> whose error space
// is the inferred error space (see above), and whose status code matches
// code_matcher.
template <typename StatusCodeMatcher>
inline matchers_internal::StatusIsMatcher StatusIs(
    StatusCodeMatcher&& code_matcher) {
  return StatusIs(std::forward<StatusCodeMatcher>(code_matcher), ::testing::_);
}

// Macros for testing the results of functions that return ::util::Status.

#define EXPECT_OK(statement) \
  EXPECT_THAT(statement, ::stratum::test_utils::IsOk())
#define ASSERT_OK(statement) \
  ASSERT_THAT(statement, ::stratum::test_utils::IsOk())

// Macros for testing the results of functions that return ::util::StatusOr.
#define ASSERT_CONCAT1(prefix, postfix) prefix##postfix
#define ASSERT_CONCAT(prefix, postfix) ASSERT_CONCAT1(prefix, postfix)
#define ASSERT_UNIQUE(prefix) ASSERT_CONCAT(prefix, __LINE__)

#define ASSERT_OK_AND_ASSIGN(lhs, statement) \
  auto ASSERT_UNIQUE(result) = statement;    \
  ASSERT_OK(ASSERT_UNIQUE(result));          \
  lhs = ASSERT_UNIQUE(result).ConsumeValueOrDie();

// There are no EXPECT_NOT_OK/ASSERT_NOT_OK macros since they would not
// provide much value (when they fail, they would just print the OK status
// which conveys no more information than EXPECT_FALSE(status.ok());
// If you want to check for particular errors, better alternatives are:
// EXPECT_EQ(::util::Status(...expected error...), status.StripMessage());
// EXPECT_THAT(status.ToString(), HasSubstr("expected error"));
// Also, see testing/base/public/gmock.h.

}  // namespace test_utils
}  // namespace stratum

#endif  // STRATUM_GLUE_STATUS_STATUS_TEST_UTIL_H_
