/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
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

#ifndef STRATUM_HAL_LIB_COMMON_DECIMAL_TYPES_UTILS_H_
#define STRATUM_HAL_LIB_COMMON_DECIMAL_TYPES_UTILS_H_

#include "gnmi/gnmi.pb.h"

namespace stratum {


// Compiled gNMI library protobuf decimal type.
using GnmiDecimal = ::gnmi::Decimal64;

// The decimal type is a pair of two components: digits and precision.
// Both having the protobuf types.
using DecimalDigits = ::google::protobuf::int64;
using DecimalPrecision = ::google::protobuf::uint32;

// Decimal types matcher.
//
// Now, this class family supports only gNMI protobuf decimal implementation.
//
// In case you want to extend the supported types set, please check the desired
// type copy and heap-allocation specifications first.
//
// This class family also has some comparison logic which can be different from
// other decimal types.
//
template <typename DecimalType>
class DecimalTypesMatcher {
  static constexpr bool supported_types_match
      = std::is_same<DecimalType, GnmiDecimal>();
  static_assert(supported_types_match, "The decimal type is not supported!");
};

// Protobuf decimal type initializer.
//
// Since the protobuf class isn't an aggregate type, we can't create it through
// the initializer list. It also doesn't have a dedicated constructor with two
// parameters for now. Hence this class helps to eliminate a lot of copy-pasted
// code for setting every necessary field of the DecimalType. E.g.:
//
//   DecimalType value;
//   value.set_digits(101);
//   value.set_precision(2);
//
//   DecimalType* valuePtr = new DecimalType;
//   valuePtr->set_digits(101);
//   valuePtr->set_precision(2);
//
// A more sophisticated way to do that has been introduced with this class.
// E.g.:
//
//   DecimalType value = TypedDecimalInitializer<DecimalType>(101, 2).Init();
//
//   DecimalType* valuePtr = TypedDecimalInitializer<DecimalType>(
//       101, 2).InitAllocated();
//
template <typename DecimalType>
class TypedDecimalInitializer final : public DecimalTypesMatcher<DecimalType> {
 public:
  // Create from the supported decimal type.
  TypedDecimalInitializer(const DecimalDigits& digits,
                          const DecimalPrecision& precision)
    : digits_{ digits }, precision_{ precision } {}

  // Default copy construction.
  TypedDecimalInitializer(const TypedDecimalInitializer&) = default;
  TypedDecimalInitializer(TypedDecimalInitializer&&) = default;

  // Default move construction.
  TypedDecimalInitializer& operator = (const TypedDecimalInitializer&)
      = default;
  TypedDecimalInitializer& operator = (TypedDecimalInitializer&&) = default;

  // Default destruction.
  ~TypedDecimalInitializer() = default;
  // Get a value copy of stack-allocated DecimalType value with given params.
  DecimalType Init() const {
    DecimalType result;
    SetFields(&result);
    return result;
  }
  // Get heap-allocated DecimalType value with given params.
  DecimalType* InitAllocated() const {
    DecimalType* result = new DecimalType;
    SetFields(result);
    return result;
  }

 private:
  void SetFields(DecimalType* out) const {
    assert(out != nullptr);
    out->set_digits(digits_);
    out->set_precision(precision_);
  }

 private:
  const DecimalDigits digits_;
  const DecimalPrecision precision_;
};

// A decimal value comparable by TypedDecimalComparator class.
//
// To compare a decimal value wrap it in this class instance. E.g.:
//
//   DecimalValue value;
//   TypedDecimalComparable comparable(value);
//   // Do the comparison.
//
template <typename DecimalType>
class TypedDecimalComparable final : public DecimalTypesMatcher<DecimalType> {
 public:
  // Create from the supported decimal type.
  // This class instantiation should work implicitly. See
  // \class TypedDecimalComparator usage examples.
  // NOLINTNEXTLINE(runtime/explicit)
  TypedDecimalComparable(const DecimalType& value)
    : digits_{ value.digits() }, precision_{ value.precision() } {}

  // Default copy construction.
  TypedDecimalComparable(const TypedDecimalComparable&) = default;
  TypedDecimalComparable(TypedDecimalComparable&&) = default;

  // Default move construction.
  TypedDecimalComparable& operator = (const TypedDecimalComparable&) = default;
  TypedDecimalComparable& operator = (TypedDecimalComparable&&) = default;

  // Default destruction.
  ~TypedDecimalComparable() = default;

  // Get digits.
  DecimalDigits digits() const {
    return digits_;
  }
  // Get precision.
  DecimalPrecision precision() const {
    return precision_;
  }

 private:
  const DecimalDigits digits_;
  const DecimalPrecision precision_;
};

// A decimal value comparator for comparing two values of TypedDecimalComparable
// type.
//
// To compare two decimal values wrap them in comparable wrapper and use this
// class methods to proceed comparison. E.g.:
//
//   DecimalValue left, right;
//   std::cout << std::boolalpha << TypedDecimalComparator::Equal(left, right);
//
class TypedDecimalComparator final {
 public:
  // Is the left equal to the right.
  //
  // Returns 'true' only if
  // * left and right digits match,
  // * left and right precision match.
  //
  // Returns 'false' otherwise.
  template <typename DecimalLeft, typename DecimalRight>
  static bool Equal(const TypedDecimalComparable<DecimalLeft>& left,
                    const TypedDecimalComparable<DecimalRight>& right) {
    return left.digits() == right.digits()
        && left.precision() == right.precision();
  }
};

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_DECIMAL_TYPES_UTILS_H_
