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

#include "stratum/hal/lib/common/decimal_types_utils.h"

#include "gtest/gtest.h"

namespace stratum {

TEST(TypedDecimalInitializerTest, InitializeGnmiDecimal_Success) {
  const DecimalDigits digits = 19;
  const DecimalPrecision precision = 0;

  GnmiDecimal value = TypedDecimalInitializer<GnmiDecimal>(
      digits, precision).Init();

  EXPECT_EQ(value.digits(), digits);
  EXPECT_EQ(value.precision(), precision);
}

TEST(TypedDecimalInitializerTest, InitializeGnmiAllocatedDecimal_Success) {
  const DecimalDigits digits = 201;
  const DecimalPrecision precision = 10;

  GnmiDecimal* value = TypedDecimalInitializer<GnmiDecimal>(
      digits, precision).InitAllocated();

  EXPECT_EQ(value->digits(), digits);
  EXPECT_EQ(value->precision(), precision);
}

TEST(TypedDecimalComparatorTest, EqualGnmiDifferentPrecision_False) {
  GnmiDecimal left = TypedDecimalInitializer<GnmiDecimal>(9011, 3).Init();
  GnmiDecimal right = TypedDecimalInitializer<GnmiDecimal>(9011, 2).Init();

  auto equal = TypedDecimalComparator::Equal<GnmiDecimal, GnmiDecimal>(
      left, right);
  EXPECT_FALSE(equal);
}

TEST(TypedDecimalComparatorTest, EqualGnmiDifferentDigits_False) {
  GnmiDecimal left = TypedDecimalInitializer<GnmiDecimal>(9010, 3).Init();
  GnmiDecimal right = TypedDecimalInitializer<GnmiDecimal>(901, 3).Init();

  auto equal = TypedDecimalComparator::Equal<GnmiDecimal, GnmiDecimal>(
      left, right);
  EXPECT_FALSE(equal);
}

TEST(TypedDecimalComparatorTest, EqualGnmiDifferentDigitsAndPrecision_False) {
  GnmiDecimal left = TypedDecimalInitializer<GnmiDecimal>(505, 2).Init();
  GnmiDecimal right = TypedDecimalInitializer<GnmiDecimal>(105, 3).Init();

  auto equal = TypedDecimalComparator::Equal<GnmiDecimal, GnmiDecimal>(
      left, right);
  EXPECT_FALSE(equal);
}

TEST(TypedDecimalComparatorTest, EqualGnmiEqualDigitsAndPrecision_True) {
  GnmiDecimal left = TypedDecimalInitializer<GnmiDecimal>(778, 1).Init();
  GnmiDecimal right = TypedDecimalInitializer<GnmiDecimal>(778, 1).Init();

  auto equal = TypedDecimalComparator::Equal<GnmiDecimal, GnmiDecimal>(
      left, right);
  EXPECT_TRUE(equal);
}

}  // namespace stratum