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


#ifndef STRATUM_HAL_LIB_PHAL_TEST_UTIL_H_
#define STRATUM_HAL_LIB_PHAL_TEST_UTIL_H_

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/attribute_database_interface.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/managed_attribute.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace impl {
// Checks the given values for equality, handling floating point comparison
// correctly.
template <typename T>
inline bool CompareValues(T expected, T actual) {
  return expected == actual;
}

template<>
inline bool CompareValues<float>(float expected, float actual) {
  return ::testing::Value(actual, ::testing::FloatEq(expected));
}

template<>
inline bool CompareValues<double>(double expected, double actual) {
  return ::testing::Value(actual, ::testing::DoubleEq(expected));
}

// Checks that the given ManagedAttribute* contains a value in the given range
// [low,high). Call ContainsValueInRange(expected_value) to use this matcher.
template <typename T>
class ContainsValueInRangeMatcher
    : public ::testing::MatcherInterface<ManagedAttribute*> {
 public:
  ContainsValueInRangeMatcher(T low, T high) : low_(low), high_(high) {}

  void DescribeTo(::std::ostream* os) const override {
    *os << "contains a value in the range [" << low_ << "," << high_ << ")";
  }

  void DescribeNegationTo(::std::ostream* os) const override {
    *os << "contains a value outside the range [" << low_ << "," << high_
        << ")";
  }

  bool MatchAndExplain(
      ManagedAttribute* arg,
      ::testing::MatchResultListener* listener) const override {
    Attribute attribute = arg->GetValue();
    const T* actual_value = absl::get_if<T>(&attribute);
    if (!actual_value) {
      *listener << "does not contain the expected type.";
      return false;
    } else if (*actual_value < low_ || *actual_value >= high_) {
      *listener << "does not contain the a value in the expected range ["
                << low_ << "," << high_
                << "). Actual value is: " << actual_value << ")";
      return false;
    }
    return true;
  }

 private:
  T low_;
  T high_;
};
}  // namespace impl

using ::testing::PrintToString;

// Checks that the given ManagedAttribute* contains the given value. The
// optional template argument should usually be used, since this will always
// return false if the wrong type is passed in (integer conversions are not
// performed).
MATCHER_P(ContainsValue, value,
          "ManagedAttribute* " + string(negation ? "doesn't store" : "stores") +
              " the value: " + PrintToString(value)) {
  Attribute attribute = arg->GetValue();
  const value_type* actual_value = absl::get_if<value_type>(&attribute);
  return actual_value && impl::CompareValues(*actual_value, value);
}

// Updates the datasource for the given ManagedAttribute*, then checks that it
// contains the given value. This update ignores normal datasource caching
// behavior. The optional template argument should usually be used, since this
// will always return false if the wrong type is passed in (integer conversions
// are not performed).
MATCHER_P(ContainsValueAfterUpdate, value,
          "ManagedAttribute* updates successfully and " +
              string(negation ? "doesn't store" : "stores") +
              " the value: " + PrintToString(value)) {
  DataSource* datasource = arg->GetDataSource();
  if (!datasource->UpdateValuesUnsafelyWithoutCacheOrLock().ok()) return false;
  return ::testing::Value(arg, ContainsValue<value_type>(value));
}

// Checks that the given ::util::StatusOr<ManagedAttribute*> is okay and
// contains the given value. The optional template argument should usually be
// used, since this will always return false if the wrong type is passed in
// (integer conversions are not performed).
MATCHER_P(IsOkAndContainsValue, value,
          "::util::StatusOr<ManagedAttribute*> " +
              string(negation ? "isn't ok or doesn't store"
                              : "is ok and stores") +
              " the value: " + PrintToString(value)) {
  if (!arg.ok()) return false;
  ManagedAttribute* managed_attribute = arg.ValueOrDie();
  return ::testing::Value(managed_attribute, ContainsValue<value_type>(value));
}

// Checks that the given ManagedAttribute* contains a value in the given range
// [low,high). The optional template argument should usually be used, since
// this will always return false if the wrong type is passed in (integer
// conversions are not performed).
template <typename T>
::testing::Matcher<ManagedAttribute*> ContainsValueInRange(T low, T high) {
  return ::testing::MakeMatcher(
      new impl::ContainsValueInRangeMatcher<T>(low, high));
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum


#endif  // STRATUM_HAL_LIB_PHAL_TEST_UTIL_H_
