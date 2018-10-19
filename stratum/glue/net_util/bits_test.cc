// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// Copyright (C) 2002 and onwards Google, Inc.
// Author: Paul Haahr
//
// This tests common/bits.{cc,h}

#include "stratum/glue/net_util/bits.h"

#include <string.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

#include "stratum/glue/logging.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "absl/numeric/int128.h"

namespace stratum {

int32 num_iterations = 10000;  // Number of test iterations to run.
int32 max_bytes = 100;  // Maximum number of bytes to use in tests.

static const int kMaxBytes = 128;
static const int kNumReverseBitsRandomTests = 10;

class BitsTest : public testing::Test {
 public:
  BitsTest() {}

 protected:
  template<typename T>
  static void CheckUnsignedType() {
    typedef typename Bits::UnsignedType<T>::Type UnsignedT;
    EXPECT_EQ(sizeof(T), sizeof(UnsignedT));
    EXPECT_FALSE(std::numeric_limits<UnsignedT>::is_signed);
  }

  // Generate a random number of type T with the same range as that of T.
  template<typename T>
  T RandomBits() {
    std::uniform_int_distribution<T> distribution;
    return distribution(random_);
  }

  template<typename T>
  T RandomUniform(T min, T max) {
    std::uniform_int_distribution<T> distribution(min, max - 1);
    return distribution(random_);
  }

  bool RandomOneIn(int max) {
    return RandomUniform<int>(0, max) == 0;
  }

  // generate a uniformly distributed float between 0 and 1
  float RandomFloat() {
    std::uniform_real_distribution<float> distribution(0.0, 1.0);
    return distribution(random_);
  }

  // Wrapper for Bits::SetBits with a slightly different interface for
  // testing.  Instead of modifying a scalar, it returns a new value
  // with some bits replaced.
  template<typename T>
  static T SetBits(T dest,
                   const typename Bits::UnsignedType<T>::Type src,
                   const int offset,
                   const int nbits) {
    Bits::SetBits(src, offset, nbits, &dest);
    return dest;
  }

  template<typename DestType, typename SrcType>
  void RandomCopyBitsTestTwoTypes();

  template<typename DestType>
  void RandomCopyBitsTestDestType();

  std::default_random_engine random_;
};

// Randomly test Bits::CopyBits of two scalar types.
template<typename DestType, typename SrcType>
void BitsTest::RandomCopyBitsTestTwoTypes() {
  const int kNumIterations = 2000;
  const int dest_bits = sizeof(DestType) * 8;
  const int src_bits = sizeof(SrcType) * 8;

  for (int i = 0; i < kNumIterations; ++i) {
    DestType dest = RandomBits<DestType>();
    DestType dest2 = dest;
    const int dest_offset = RandomBits<int>() % dest_bits;
    const SrcType src = RandomBits<SrcType>();
    const int src_offset = RandomBits<int>() % src_bits;
    const int nbits_max = std::min(dest_bits - dest_offset,
                                      src_bits - src_offset);
    const int nbits = RandomBits<int>() % (nbits_max + 1);

    Bits::CopyBits(&dest, dest_offset, src, src_offset, nbits);
    EXPECT_EQ(Bits::GetBits(src, src_offset, nbits),
              Bits::GetBits(dest, dest_offset, nbits));

    // Reference implementation: Copying bits one at a time.
    typedef typename Bits::UnsignedType<SrcType>::Type SrcUnsignedType;
    typedef typename Bits::UnsignedType<DestType>::Type DestUnsignedType;
    const SrcUnsignedType unsigned_src = static_cast<SrcUnsignedType>(src);
    for (int j = 0; j < nbits; ++j) {
      const SrcUnsignedType src_bit =
          static_cast<SrcUnsignedType>(1) << (src_offset + j);
      const DestUnsignedType dest_bit =
          static_cast<DestUnsignedType>(1) << (dest_offset + j);
      if ((unsigned_src & src_bit) != 0) {
        dest2 |= dest_bit;
      } else {
        dest2 &= ~dest_bit;
      }
    }

    EXPECT_EQ(dest, dest2);
  }
}

// Helper template to test all 8 scalar source types.
template<typename DestType>
void BitsTest::RandomCopyBitsTestDestType() {
  RandomCopyBitsTestTwoTypes<DestType, int8>();
  RandomCopyBitsTestTwoTypes<DestType, uint8>();
  RandomCopyBitsTestTwoTypes<DestType, int16>();
  RandomCopyBitsTestTwoTypes<DestType, uint16>();
  RandomCopyBitsTestTwoTypes<DestType, int32>();
  RandomCopyBitsTestTwoTypes<DestType, uint32>();
  RandomCopyBitsTestTwoTypes<DestType, int64>();
  RandomCopyBitsTestTwoTypes<DestType, uint64>();
}

TEST_F(BitsTest, BitCountingEdgeCases) {
  std::cout << "TestBitCountingEdgeCases" << std::endl;
  EXPECT_EQ(0, Bits::CountOnes(0));
  EXPECT_EQ(1, Bits::CountOnes(1));
  EXPECT_EQ(32, Bits::CountOnes(static_cast<uint32>(~0U)));
  EXPECT_EQ(1, Bits::CountOnes(0x8000000));

  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(1, Bits::CountOnes(1U << i));
    EXPECT_EQ(31, Bits::CountOnes(static_cast<uint32>(~0U) ^ (1U << i)));
  }

  EXPECT_EQ(0, Bits::CountOnes64(0LL));
  EXPECT_EQ(1, Bits::CountOnes64(1LL));
  EXPECT_EQ(64, Bits::CountOnes64(static_cast<uint64>(~0ULL)));
  EXPECT_EQ(1, Bits::CountOnes64(0x8000000LL));

  for (int i = 0; i < 64; i++) {
    EXPECT_EQ(1, Bits::CountOnes64(1LLU << i));
    EXPECT_EQ(63, Bits::CountOnes64(static_cast<uint64> (~(1LLU << i))));
  }

  EXPECT_EQ(0, Bits::CountOnes128(absl::uint128(0)));
  EXPECT_EQ(1, Bits::CountOnes128(absl::uint128(1)));
  EXPECT_EQ(128, Bits::CountOnes128(~absl::uint128(0)));

  for (int i = 0; i < 128; i++) {
    EXPECT_EQ(1, Bits::CountOnes128(absl::uint128(1) << i));
    EXPECT_EQ(127,
              Bits::CountOnes128(~absl::uint128(0) ^ (absl::uint128(1) << i)));
  }

  EXPECT_EQ(0, Bits::Count("", 0));
  for (int i = 0; i <= kint8max; i++) {
    uint8 b[1];
    b[0] = i;
    EXPECT_EQ(Bits::Count(b, 1), Bits::CountOnes(i));
  }
}

#if 0
// This test will only work on argo machines.  To run it,
// you'll have to link static, the scp and ssh into a prod
// argo machine.
TEST_F(BitsTest, BitCountingEdgeCasesWithPopcount) {
#if defined(__x86_64__)
  if (TestCPUFeature(POPCNT)) {
    std::cout << "TestBitCountingEdgeCasesWithPopcount" << std::endl;
    EXPECT_EQ(0, Bits::CountOnes(0));
    EXPECT_EQ(1, Bits::CountOnes(1));
    EXPECT_EQ(32, Bits::CountOnes(static_cast<uint32>(~0U)));
    EXPECT_EQ(1, Bits::CountOnes(0x8000000));

    for (int i = 0; i < 32; i++) {
      EXPECT_EQ(1, Bits::CountOnes(1U << i));
      EXPECT_EQ(31, Bits::CountOnes(static_cast<uint32>(~0U) ^ (1U << i)));
    }

    EXPECT_EQ(0, Bits::CountOnes64withPopcount(0LL));
    EXPECT_EQ(1, Bits::CountOnes64withPopcount(1LL));
    EXPECT_EQ(64, Bits::CountOnes64withPopcount(static_cast<uint64>(~0ULL)));
    EXPECT_EQ(1, Bits::CountOnes64withPopcount(0x8000000LL));

    for (int i = 0; i < 64; i++) {
      EXPECT_EQ(1, Bits::CountOnes64withPopcount(1LLU << i));
      EXPECT_EQ(63, Bits::CountOnes64withPopcount(
          static_cast<uint64>(~(1LLU << i))));
    }

    EXPECT_EQ(0, Bits::Count("", 0));
    for (int i = 0; i <= kint8max; i++) {
      uint8 b[1];
      b[0] = i;
      EXPECT_EQ(Bits::Count(b, 1), Bits::CountOnes(i));
    }
  } else {
    std::cout << "TestBitCountingEdgeCasesWithPopcount: NO POPCNT SUPPORT"
              << std::endl;
  }
#endif
}
#endif

TEST_F(BitsTest, BitCountingRandom) {
  std::cout << "TestBitCountingRandom" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    float p = RandomFloat();
    int nbits = 0;
    uint32 n = 0;
    for (int i = 0; i < 32; i++) {
      if (RandomFloat() < p) {
        n |= (1U << i);
        nbits++;
      }
    }
    EXPECT_EQ(nbits, Bits::CountOnes(n));
  }
}

TEST_F(BitsTest, BitCountingRandom64) {
  std::cout << "TestBitCountingRandom64" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    float p = RandomFloat();
    int nbits = 0;
    uint64 n = 0;
    for (int i = 0; i < 64; i++) {
      if (RandomFloat() < p) {
        n |= (1LL << i);
        nbits++;
      }
    }
    EXPECT_EQ(nbits, Bits::CountOnes64(n));
  }
}

TEST_F(BitsTest, BitCountingRandom128) {
  std::cout << "TestBitCountingRandom128" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    float p = RandomFloat();
    int nbits = 0;
    absl::uint128 n = 0;
    for (int i = 0; i < 128; i++) {
      if (RandomFloat() < p) {
        n |= (absl::uint128(1) << i);
        nbits++;
      }
    }
    EXPECT_EQ(nbits, Bits::CountOnes128(n));
  }
}

TEST_F(BitsTest, BitCountingRandomArray) {
  std::cout << "TestBitCountingRandomArray" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    float p = RandomFloat();
    int num_bytes = RandomUniform<int>(0, max_bytes);
    uint8 b[kMaxBytes];
    memset(b, 0, kMaxBytes);
    int num_bits = num_bytes * 8;
    int nbits = 0;
    for (int j = 0; j < num_bits; j++) {
      if (RandomFloat() < p) {
        b[j / 8] |= 1U << (j % 8);
        nbits++;
      }
    }
    EXPECT_EQ(nbits, Bits::Count(b, num_bytes));
  }
}

TEST_F(BitsTest, BitCountLeadingZeros) {
  std::cout << "TestBitCountLeadingZeros" << std::endl;
  EXPECT_EQ(32, Bits::CountLeadingZeros32(static_cast<uint32>(0)));
  EXPECT_EQ(64, Bits::CountLeadingZeros64(static_cast<uint64>(0)));
  EXPECT_EQ(128, Bits::CountLeadingZeros128(absl::uint128(0)));
  EXPECT_EQ(0, Bits::CountLeadingZeros32(~static_cast<uint32>(0)));
  EXPECT_EQ(0, Bits::CountLeadingZeros64(~static_cast<uint64>(0)));
  EXPECT_EQ(0, Bits::CountLeadingZeros128(~absl::uint128(0)));

  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(31 - i, Bits::CountLeadingZeros32(static_cast<uint32>(1) << i));
  }

  for (int i = 0; i < 64; i++) {
    EXPECT_EQ(63 - i, Bits::CountLeadingZeros64(static_cast<uint64>(1) << i));
  }

  for (int i = 0; i < 128; i++) {
    EXPECT_EQ(127 - i, Bits::CountLeadingZeros128(absl::uint128(1) << i));
  }
}

TEST_F(BitsTest, BitCountLeadingZerosRandom) {
  std::cout << "TestBitCountLeadingZerosRandom" << std::endl;

  for (int i = 0; i < num_iterations; i++) {
    int count = RandomUniform<int>(0, 32);
    uint32 n = (RandomBits<uint32>() | (static_cast<uint32>(1) << 31)) >> count;
    EXPECT_EQ(count, Bits::CountLeadingZeros32(n));
  }

  for (int i = 0; i < num_iterations; i++) {
    int count = RandomUniform<int>(0, 64);
    uint64 n = (RandomBits<uint64>() | (static_cast<uint64>(1) << 63)) >> count;
    EXPECT_EQ(count, Bits::CountLeadingZeros64(n));
  }

  for (int i = 0; i < num_iterations; i++) {
    int count = RandomUniform<int>(0, 128);
    absl::uint128 random =
        absl::MakeUint128(RandomBits<uint64>(), RandomBits<uint64>());
    absl::uint128 n = (random | (absl::uint128(1) << 127)) >> count;
    EXPECT_EQ(count, Bits::CountLeadingZeros128(n));
  }
}

TEST_F(BitsTest, BitDifferenceRandom) {
  std::cout << "TestBitDifferenceRandom" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    float p = RandomFloat();
    int num_bytes = RandomUniform<int>(0, max_bytes);
    uint8 b1[kMaxBytes];
    uint8 b2[kMaxBytes];
    memset(b1, 0, kMaxBytes);
    memset(b2, 0, kMaxBytes);
    for (int j = 0; j < num_bytes; j++) {
      b1[j] = RandomBits<uint8>();
      b2[j] = b1[j];
    }
    int num_bits = num_bytes * 8;
    int nbits = 0;
    for (int j = 0; j < num_bits; j++) {
      if (RandomFloat() < p) {
        b2[j / 8] ^= 1U << (j % 8);
        nbits++;
      }
    }
    EXPECT_EQ(nbits, Bits::Difference(b1, b2, num_bytes));
    EXPECT_EQ(nbits, Bits::CappedDifference(b1, b2, num_bytes, nbits * 3));
    int capped = Bits::CappedDifference(b1, b2, num_bytes, nbits / 2);
    EXPECT_GE(nbits, capped);
    EXPECT_LE(nbits / 2, capped);
  }
}

static bool SlowBytesContainByte(uint32 x, uint8 b) {
  return (x & 0xff) == b || ((x >> 8) & 0xff) == b ||
      ((x >> 16) & 0xff) == b || ((x >> 24) & 0xff) == b;
}

static bool SlowBytesContainByte(uint64 x, uint8 b) {
  uint32 u = x;
  uint32 v = x >> 32;
  return SlowBytesContainByte(u, b) || SlowBytesContainByte(v, b);
}

static bool SlowBytesContainByteLessThan(uint32 x, uint8 b) {
  return (x & 0xff) < b || ((x >> 8) & 0xff) < b ||
      ((x >> 16) & 0xff) < b || ((x >> 24) & 0xff) < b;
}

static bool SlowBytesContainByteLessThan(uint64 x, uint8 b) {
  uint32 u = x;
  uint32 v = x >> 32;
  return SlowBytesContainByteLessThan(u, b) ||
      SlowBytesContainByteLessThan(v, b);
}

TEST_F(BitsTest, BytesContainByte) {
  std::cout << "TestBytesContainByte" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    uint32 u32 = RandomBits<uint32>();
    uint64 u64 = RandomBits<uint64>();
    int64  s64 = u64;
    uint8    b = RandomBits<uint8>();

    EXPECT_EQ(Bits::BytesContainByte<uint32>(u32, b),
              SlowBytesContainByte(u32, b));
    EXPECT_EQ(Bits::BytesContainByte<uint64>(u64, b),
              SlowBytesContainByte(u64, b));
    EXPECT_EQ(Bits::BytesContainByte<uint64>(u64, b),
              Bits::BytesContainByte<int64>(s64, b));

    EXPECT_EQ(Bits::BytesContainByteLessThan<uint32>(u32, b),
              SlowBytesContainByteLessThan(u32, b));
    EXPECT_EQ(Bits::BytesContainByteLessThan<uint64>(u64, b),
              SlowBytesContainByteLessThan(u64, b));
    EXPECT_EQ(Bits::BytesContainByteLessThan<uint64>(u64, b),
              Bits::BytesContainByteLessThan<int64>(s64, b));
  }
}

static bool ByteInRange(uint8 x, uint8 lo, uint8 hi) {
  return lo <= x && x <= hi;
}

// True if all bytes in x are in [lo, hi].
static bool SlowBytesAllInRange(uint32 x, uint8 lo, uint8 hi) {
  return ByteInRange(x, lo, hi) && ByteInRange(x >> 8, lo, hi) &&
      ByteInRange(x >> 16, lo, hi) && ByteInRange(x >> 24, lo, hi);
}

// True if all bytes in x are in [lo, hi].
static bool SlowBytesAllInRange(uint64 x, uint8 lo, uint8 hi) {
  uint32 u = x;
  uint32 v = x >> 32;
  return SlowBytesAllInRange(u, lo, hi) && SlowBytesAllInRange(v, lo, hi);
}

TEST_F(BitsTest, BytesAllInRange) {
  std::cout << "TestBytesAllInRange" << std::endl;
  for (int i = 0; i < num_iterations; i++) {
    uint32 u32 = RandomBits<uint32>();
    uint64 u64 = RandomBits<uint64>();
    int64  s64 = u64;
    uint8   lo = RandomBits<uint32>() >> 13;
    uint8   hi = RandomBits<uint32>() >> 13;
    if (i > 5 && lo > hi) {
      std::swap(lo, hi);
    }

    EXPECT_EQ(Bits::BytesAllInRange<uint32>(u32, lo, hi),
              SlowBytesAllInRange(u32, lo, hi));
    EXPECT_EQ(Bits::BytesAllInRange<uint64>(u64, lo, hi),
              SlowBytesAllInRange(u64, lo, hi));
    EXPECT_EQ(Bits::BytesAllInRange<uint64>(u64, lo, hi),
              Bits::BytesAllInRange<int64>(s64, lo, hi));
  }
}

#if 0
// Benchmarks not available in depot3
template<class T>
static int BytesContainByteLessThanAggregatePercentage(const vector<T>& v,
                                                       uint8 b) {
  int yes = 0;
  for (int i = 0; i < v.size(); i++) {
    yes += Bits::BytesContainByteLessThan<T>(v[i], b);
  }
  return yes * 100 / v.size();
}

template<class T, bool F(T, uint8)>
static void BM_BytesContainByteLessThan(int iters, int min_percent_false) {
  static const int kNumRandomNumbers = 4096;
  static const uint8 k0 = 20;
  static const uint8 k1 = k0 + 128;
  StopBenchmarkTiming();
  const int max_percent_true = 100 - min_percent_false;
  int percent_to_change = 0;
  vector<T> v;
  vector<T> w;
  MTRandom random(test_random_seed);
  for (int i = 0; i < kNumRandomNumbers; i++) {
    v.push_back(random.Rand64());
    w.push_back(random.Rand64());
  }
  do {
    percent_to_change = BytesContainByteLessThanAggregatePercentage<T>(v, k0) -
        max_percent_true;
    for (int i = 0; i < kNumRandomNumbers * percent_to_change / 100; i++) {
      v[RandomUniform<size_t>(0, kNumRandomNumbers)] |= RandomBits<T>();
    }
  } while (percent_to_change > 0);
  do {
    percent_to_change = BytesContainByteLessThanAggregatePercentage<T>(w, k1) -
        max_percent_true;
    for (int i = 0; i < kNumRandomNumbers * percent_to_change / 100; i++) {
      w[RandomUniform<size_t>(kNumRandomNumbers)] |= RandomBits<T>();
    }
  } while (percent_to_change > 0);
  StartBenchmarkTiming();

  int result = 0;
  int64 processed = sizeof(T) * 2 * iters;
  while (iters > 0) {
    int stop_index = std::min<int>(iters, kNumRandomNumbers);
    for (int i = 0; i < stop_index; i++) {
      result += F(v[i], k0) ? 3 : -1;
      result += F(w[i], k1) ? 3 : -1;
    }
    iters -= stop_index;
  }
  CHECK_GT(processed, result & 1);  // inhibit compiler optimization
  SetBenchmarkBytesProcessed(processed);
}

// The expected use cases should have Bits::ContainsByteLessThan<T>(x) returning
// false most of the time; otherwise, why not use a brute force search?
#define PERCENT_FALSE0 90
#define PERCENT_FALSE1 98

void BM_Uint32ContainsByteLessThan(int iters, int min_percent_false) {
  BM_BytesContainByteLessThan<uint32,
      Bits::BytesContainByteLessThan>(iters, min_percent_false);
}
BENCHMARK_WITH_ARG(BM_Uint32ContainsByteLessThan, PERCENT_FALSE0);
BENCHMARK_WITH_ARG(BM_Uint32ContainsByteLessThan, PERCENT_FALSE1);

void BM_Uint64ContainsByteLessThan(int iters, int min_percent_false) {
  BM_BytesContainByteLessThan<uint64,
      Bits::BytesContainByteLessThan>(iters, min_percent_false);
}
BENCHMARK_WITH_ARG(BM_Uint64ContainsByteLessThan, PERCENT_FALSE0);
BENCHMARK_WITH_ARG(BM_Uint64ContainsByteLessThan, PERCENT_FALSE1);

#endif

TEST_F(BitsTest, Log2EdgeCases) {
  std::cout << "TestLog2EdgeCases" << std::endl;

  EXPECT_EQ(-1, Bits::Log2Floor(0));
  EXPECT_EQ(-1, Bits::Log2Floor64(0));
  EXPECT_EQ(-1, Bits::Log2Floor128(absl::uint128(0)));
  EXPECT_EQ(-1, Bits::Log2Ceiling(0));
  EXPECT_EQ(-1, Bits::Log2Ceiling64(0));
  EXPECT_EQ(-1, Bits::Log2Ceiling128(absl::uint128(0)));

  for (int i = 0; i < 32; i++) {
    uint32 n = 1U << i;
    EXPECT_EQ(i, Bits::Log2Floor(n));
    EXPECT_EQ(i, Bits::Log2FloorNonZero(n));
    EXPECT_EQ(i, Bits::Log2Ceiling(n));
    if (n > 2) {
      EXPECT_EQ(i - 1, Bits::Log2Floor(n - 1));
      EXPECT_EQ(i,     Bits::Log2Floor(n + 1));
      EXPECT_EQ(i - 1, Bits::Log2FloorNonZero(n - 1));
      EXPECT_EQ(i,     Bits::Log2FloorNonZero(n + 1));
      EXPECT_EQ(i,     Bits::Log2Ceiling(n - 1));
      EXPECT_EQ(i + 1, Bits::Log2Ceiling(n + 1));
    }
  }

  for (int i = 0; i < 64; i++) {
    uint64 n = 1ULL << i;
    EXPECT_EQ(i, Bits::Log2Floor64(n));
    EXPECT_EQ(i, Bits::Log2FloorNonZero64(n));
    EXPECT_EQ(i, Bits::Log2Ceiling64(n));
    if (n > 2) {
      EXPECT_EQ(i - 1, Bits::Log2Floor64(n - 1));
      EXPECT_EQ(i,     Bits::Log2Floor64(n + 1));
      EXPECT_EQ(i - 1, Bits::Log2FloorNonZero64(n - 1));
      EXPECT_EQ(i,     Bits::Log2FloorNonZero64(n + 1));
      EXPECT_EQ(i,     Bits::Log2Ceiling64(n - 1));
      EXPECT_EQ(i + 1, Bits::Log2Ceiling64(n + 1));
    }
  }

  for (int i = 0; i < 128; i++) {
    absl::uint128 n = absl::uint128(1) << i;
    EXPECT_EQ(i, Bits::Log2Floor128(n));
    EXPECT_EQ(i, Bits::Log2Ceiling128(n));
    if (n > 2) {
      EXPECT_EQ(i - 1, Bits::Log2Floor128(n - 1));
      EXPECT_EQ(i,     Bits::Log2Floor128(n + 1));
      EXPECT_EQ(i,     Bits::Log2Ceiling128(n - 1));
      EXPECT_EQ(i + 1, Bits::Log2Ceiling128(n + 1));
    }
  }
}

TEST_F(BitsTest, Log2Random) {
  std::cout << "TestLog2Random" << std::endl;

  for (int i = 0; i < num_iterations; i++) {
    int maxbit = -1;
    uint32 n = 0;
    while (!RandomOneIn(32)) {
      int bit = RandomUniform<int>(0, 32);
      n |= (1U << bit);
      maxbit = std::max(bit, maxbit);
    }
    EXPECT_EQ(maxbit, Bits::Log2Floor(n));
    if (n != 0) {
      EXPECT_EQ(maxbit, Bits::Log2FloorNonZero(n));
    }
  }
}

TEST_F(BitsTest, Log2Random64) {
  std::cout << "TestLog2Random64" << std::endl;

  for (int i = 0; i < num_iterations; i++) {
    int maxbit = -1;
    uint64 n = 0;
    while (!RandomOneIn(64)) {
      int bit = RandomUniform<int>(0, 64);
      n |= (1ULL << bit);
      maxbit = std::max(bit, maxbit);
    }
    EXPECT_EQ(maxbit, Bits::Log2Floor64(n));
    if (n != 0) {
      EXPECT_EQ(maxbit, Bits::Log2FloorNonZero64(n));
    }
  }
}

TEST_F(BitsTest, Log2Random128) {
  std::cout << "TestLog2Random128" << std::endl;

  for (int i = 0; i < num_iterations; i++) {
    int maxbit = -1;
    absl::uint128 n = absl::uint128(0);
    while (!RandomOneIn(128)) {
      int bit = RandomUniform<int>(0, 128);
      n |= (absl::uint128(1) << bit);
      maxbit = std::max(bit, maxbit);
    }
    EXPECT_EQ(maxbit, Bits::Log2Floor128(n));
  }
}

TEST_F(BitsTest, GetBits) {
  const int8 s8_src = 0x12;
  EXPECT_EQ(static_cast<uint8>(0x2), Bits::GetBits(s8_src, 0, 4));
  EXPECT_EQ(static_cast<uint8>(0x1), Bits::GetBits(s8_src, 4, 4));

  const uint8 u8_src = 0x12;
  EXPECT_EQ(static_cast<uint8>(0x2), Bits::GetBits(u8_src, 0, 4));
  EXPECT_EQ(static_cast<uint8>(0x1), Bits::GetBits(u8_src, 4, 4));

  const int16 s16_src = 0x1234;
  EXPECT_EQ(static_cast<uint16>(0x34), Bits::GetBits(s16_src, 0, 8));
  EXPECT_EQ(static_cast<uint16>(0x23), Bits::GetBits(s16_src, 4, 8));
  EXPECT_EQ(static_cast<uint16>(0x12), Bits::GetBits(s16_src, 8, 8));

  const uint16 u16_src = 0x1234;
  EXPECT_EQ(static_cast<uint16>(0x34), Bits::GetBits(u16_src, 0, 8));
  EXPECT_EQ(static_cast<uint16>(0x23), Bits::GetBits(u16_src, 4, 8));
  EXPECT_EQ(static_cast<uint16>(0x12), Bits::GetBits(u16_src, 8, 8));

  const int32 s32_src = 0x12345678;
  EXPECT_EQ(static_cast<uint32>(0x5678), Bits::GetBits(s32_src, 0, 16));
  EXPECT_EQ(static_cast<uint32>(0x3456), Bits::GetBits(s32_src, 8, 16));
  EXPECT_EQ(static_cast<uint32>(0x1234), Bits::GetBits(s32_src, 16, 16));

  const uint32 u32_src = 0x12345678;
  EXPECT_EQ(static_cast<uint32>(0x5678), Bits::GetBits(u32_src, 0, 16));
  EXPECT_EQ(static_cast<uint32>(0x3456), Bits::GetBits(u32_src, 8, 16));
  EXPECT_EQ(static_cast<uint32>(0x1234), Bits::GetBits(u32_src, 16, 16));

  const int64 s64_src = 0x123456789abcdef0LL;
  EXPECT_EQ(static_cast<uint64>(0x9abcdef0), Bits::GetBits(s64_src, 0, 32));
  EXPECT_EQ(static_cast<uint64>(0x56789abc), Bits::GetBits(s64_src, 16, 32));
  EXPECT_EQ(static_cast<uint64>(0x12345678), Bits::GetBits(s64_src, 32, 32));

  const int64 u64_src = 0x123456789abcdef0ULL;
  EXPECT_EQ(static_cast<uint64>(0x9abcdef0), Bits::GetBits(u64_src, 0, 32));
  EXPECT_EQ(static_cast<uint64>(0x56789abc), Bits::GetBits(u64_src, 16, 32));
  EXPECT_EQ(static_cast<uint64>(0x12345678), Bits::GetBits(u64_src, 32, 32));
}

TEST_F(BitsTest, SetBits) {
  const int8 s8_dest = 0x12;
  EXPECT_EQ(0, SetBits(s8_dest, 0, 0, 8));
  EXPECT_EQ(-1, SetBits(s8_dest, 0xff, 0, 8));
  EXPECT_EQ(0x32, SetBits(s8_dest, 0xf3, 4, 4));

  const uint8 u8_dest = 0x12;
  EXPECT_EQ(0, SetBits(u8_dest, 0, 0, 8));
  EXPECT_EQ(0xff, SetBits(u8_dest, 0xff, 0, 8));
  // Should only write the lower 4 bits of value.
  EXPECT_EQ(0x32, SetBits(u8_dest, 0xf3, 4, 4));

  const int16 s16_dest = 0x1234;
  EXPECT_EQ(0, SetBits(s16_dest, 0, 0, 16));
  EXPECT_EQ(-1, SetBits(s16_dest, 0xffff, 0, 16));
  EXPECT_EQ(0x1254, SetBits(s16_dest, 0xf5, 4, 4));

  const uint16 u16_dest = 0x1234;
  EXPECT_EQ(0, SetBits(u16_dest, 0, 0, 16));
  EXPECT_EQ(0xffff, SetBits(u16_dest, 0xffff, 0, 16));
  EXPECT_EQ(0x1254, SetBits(u16_dest, 0xf5, 4, 4));

  const int32 s32_dest = 0x12345678;
  EXPECT_EQ(0, SetBits(s32_dest, 0, 0, 32));
  EXPECT_EQ(-1, SetBits(s32_dest, 0xffffffff, 0, 32));
  EXPECT_EQ(0x12345698, SetBits(s32_dest, 0xf9, 4, 4));

  const uint32 u32_dest = 0x12345678;
  EXPECT_EQ(0UL, SetBits(u32_dest, 0, 0, 32));
  EXPECT_EQ(0xffffffffUL, SetBits(u32_dest, 0xffffffff, 0, 32));
  EXPECT_EQ(0x12345698UL, SetBits(u32_dest, 0xf9, 4, 4));

  const int64 s64_dest = 0x123456789abcdef0LL;
  EXPECT_EQ(0x0000000000000000LL, SetBits(s64_dest, 0ULL, 0, 64));
  EXPECT_EQ(-1LL, SetBits(s64_dest, 0xffffffffffffffffULL, 0, 64));
  EXPECT_EQ(0x123456789abcde10LL, SetBits(s64_dest, 0xf1, 4, 4));

  const uint64 u64_dest = 0x123456789abcdef0ULL;
  EXPECT_EQ(0ULL, SetBits(u64_dest, 0x00000000, 0, 64));
  EXPECT_EQ(0xffffffffffffffffULL,
            SetBits(u64_dest, 0xffffffffffffffffULL, 0, 64));
  EXPECT_EQ(0x123456789abcde10ULL, SetBits(u64_dest, 0xf1, 4, 4));
}

TEST_F(BitsTest, CopyBits) {
  int8 s8_dest = 0x12;
  Bits::CopyBits(&s8_dest, 0, 0, 0, 8);
  EXPECT_EQ(0, s8_dest);
  s8_dest = 0x12;
  Bits::CopyBits(&s8_dest, 0, -1, 0, 8);
  EXPECT_EQ(-1, s8_dest);
  s8_dest = 0x12;
  Bits::CopyBits(&s8_dest, 4, 0xf3ff, 8, 4);
  EXPECT_EQ(0x32, s8_dest);

  int16 s16_dest = 0x1234;
  Bits::CopyBits(&s16_dest, 0, 0, 0, 16);
  EXPECT_EQ(0, s16_dest);
  s16_dest = 0x1234;
  Bits::CopyBits(&s16_dest, 0, -1, 0, 16);
  EXPECT_EQ(-1, s16_dest);
  s16_dest = 0x1234;
  Bits::CopyBits(&s16_dest, 8, 0xf5fff, 12, 4);
  EXPECT_EQ(0x1534, s16_dest);

  int32 s32_dest = 0x12345678;
  Bits::CopyBits(&s32_dest, 0, 0, 0, 32);
  EXPECT_EQ(0, s32_dest);
  s32_dest = 0x12345678;
  Bits::CopyBits(&s32_dest, 0, -1, 0, 32);
  EXPECT_EQ(-1, s32_dest);
  s32_dest = 0x12345678;
  Bits::CopyBits(&s32_dest, 12, 0xf9ffff, 16, 4);
  EXPECT_EQ(0x12349678, s32_dest);

  int64 s64_dest = 0x123456789abcdef0LL;
  Bits::CopyBits(&s64_dest, 0, 0LL, 0, 64);
  EXPECT_EQ(0, s64_dest);
  s64_dest = 0x123456789abcdef0LL;
  Bits::CopyBits(&s64_dest, 0, -1LL, 0, 64);
  EXPECT_EQ(-1, s64_dest);
  s64_dest = 0x123456789abcdef0LL;
  Bits::CopyBits(&s64_dest, 16, 0xf1fffff, 20, 4);
  EXPECT_EQ(0x123456789ab1def0, s64_dest);
}

TEST_F(BitsTest, RandomCopyBitsTest) {
  RandomCopyBitsTestDestType<int8>();
  RandomCopyBitsTestDestType<uint8>();
  RandomCopyBitsTestDestType<int16>();
  RandomCopyBitsTestDestType<uint16>();
  RandomCopyBitsTestDestType<int32>();
  RandomCopyBitsTestDestType<uint32>();
  RandomCopyBitsTestDestType<int64>();
  RandomCopyBitsTestDestType<uint64>();
}

TEST(Bits, Port32) {
  for (int shift = 0; shift < 32; shift++) {
    for (int delta = -1; delta <= +1; delta++) {
      const uint32 v = (static_cast<uint32>(1) << shift) + delta;
      EXPECT_EQ(Bits::Log2Floor_Portable(v), Bits::Log2Floor(v)) << v;
      if (v != 0) {
        EXPECT_EQ(Bits::Log2FloorNonZero_Portable(v),
                  Bits::Log2FloorNonZero(v)) << v;
        EXPECT_EQ(Bits::FindLSBSetNonZero_Portable(v),
                  Bits::FindLSBSetNonZero(v)) << v;
      }
    }
  }
  static const uint32 M32 = kuint32max;
  EXPECT_EQ(Bits::Log2Floor_Portable(M32), Bits::Log2Floor(M32)) << M32;
  EXPECT_EQ(Bits::Log2FloorNonZero_Portable(M32),
            Bits::Log2FloorNonZero(M32)) << M32;
  EXPECT_EQ(Bits::FindLSBSetNonZero_Portable(M32),
            Bits::FindLSBSetNonZero(M32)) << M32;
}

TEST(Bits, Port64) {
  for (int shift = 0; shift < 64; shift++) {
    for (int delta = -1; delta <= +1; delta++) {
      const uint64 v = (static_cast<uint64>(1) << shift) + delta;
      EXPECT_EQ(Bits::Log2Floor64_Portable(v), Bits::Log2Floor64(v)) << v;
      if (v != 0) {
        EXPECT_EQ(Bits::Log2FloorNonZero64_Portable(v),
                  Bits::Log2FloorNonZero64(v)) << v;
        EXPECT_EQ(Bits::FindLSBSetNonZero64_Portable(v),
                  Bits::FindLSBSetNonZero64(v)) << v;
      }
    }
  }
  static const uint64 M64 = kuint64max;
  EXPECT_EQ(Bits::Log2Floor64_Portable(M64), Bits::Log2Floor64(M64)) << M64;
  EXPECT_EQ(Bits::Log2FloorNonZero64_Portable(M64),
            Bits::Log2FloorNonZero64(M64)) << M64;
  EXPECT_EQ(Bits::FindLSBSetNonZero64_Portable(M64),
            Bits::FindLSBSetNonZero64(M64)) << M64;
}

TEST(CountOnes, InByte) {
  for (int i = 0; i < 256; i++) {
    unsigned char c = static_cast<unsigned char>(i);
    int expected = 0;
    for (int pos = 0; pos < 8; pos++) {
      expected += (c & (1 << pos)) ? 1 : 0;
    }
    EXPECT_EQ(expected, Bits::CountOnesInByte(c))
      << std::hex << static_cast<int>(c);
  }
}

TEST(FindLSBSetNonZero, OneAllOrSomeBitsSet) {
  uint32 testone = 0x00000001;
  uint32 testall = 0xFFFFFFFF;
  uint32 testsome = 0x87654321;
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(i, Bits::FindLSBSetNonZero(testone));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero(testall));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero(testsome));
    testone <<= 1;
    testall <<= 1;
    testsome <<= 1;
  }
}

TEST(FindLSBSetNonZero64, OneAllOrSomeBitsSet) {
  uint64 testone = 0x0000000000000001ULL;
  uint64 testall = 0xFFFFFFFFFFFFFFFFULL;
  uint64 testsome = 0x0FEDCBA987654321ULL;
  for (int i = 0; i < 64; ++i) {
    EXPECT_EQ(i, Bits::FindLSBSetNonZero64(testone));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero64(testall));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero64(testsome));
    testone <<= 1;
    testall <<= 1;
    testsome <<= 1;
  }
}

TEST(FindLSBSetNonZero128, OneAllOrSomeBitsSet) {
  absl::uint128 testone = absl::uint128(1);
  absl::uint128 testall = ~absl::uint128(0);
  absl::uint128 testsome =
      absl::MakeUint128(0x0FEDCBA987654321ULL, 0x0FEDCBA987654321ULL);
  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ(i, Bits::FindLSBSetNonZero128(testone));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero128(testall));
    EXPECT_EQ(i, Bits::FindLSBSetNonZero128(testsome));
    testone <<= 1;
    testall <<= 1;
    testsome <<= 1;
  }
}

TEST(FindMSBSetNonZero, OneAllOrSomeBitsSet) {
  uint32 testone = 0x80000000;
  uint32 testall = 0xFFFFFFFF;
  uint32 testsome = 0x87654321;
  for (int i = 31; i >= 0; --i) {
    EXPECT_EQ(i, Bits::FindMSBSetNonZero(testone));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero(testall));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero(testsome));
    testone >>= 1;
    testall >>= 1;
    testsome >>= 1;
  }
}

TEST(FindMSBSetNonZero64, OneAllOrSomeBitsSet) {
  uint64 testone = 0x8000000000000000ULL;
  uint64 testall = 0xFFFFFFFFFFFFFFFFULL;
  uint64 testsome = 0xFEDCBA9876543210ULL;
  for (int i = 63; i >= 0; --i) {
    EXPECT_EQ(i, Bits::FindMSBSetNonZero64(testone));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero64(testall));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero64(testsome));
    testone >>= 1;
    testall >>= 1;
    testsome >>= 1;
  }
}

TEST(FindMSBSetNonZero128, OneAllOrSomeBitsSet) {
  absl::uint128 testone = absl::MakeUint128(0x8000000000000000ULL, 0);
  absl::uint128 testall = ~absl::uint128(0);
  absl::uint128 testsome =
      absl::MakeUint128(0xFEDCBA9876543210ULL, 0xFEDCBA9876543210ULL);
  for (int i = 127; i >= 0; --i) {
    EXPECT_EQ(i, Bits::FindMSBSetNonZero128(testone));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero128(testall));
    EXPECT_EQ(i, Bits::FindMSBSetNonZero128(testsome));
    testone >>= 1;
    testall >>= 1;
    testsome >>= 1;
  }
}

#if 0
// Benchmarks not available in depot3
template<typename T, int func(T)>
void BM_FindXSBSetNonZero(int iters, int set_bit) {
  // This is volatile so that a compiler cannot optimize repeated calls to a
  // pure function with a loop-invariant argument by only calling the function
  // once and multiply result with loop count.
  volatile T word = static_cast<T>(1) << set_bit;
  const int64 total_iters = iters;
  int64 sum = 0;
  while (iters-- > 0) {
    sum += func(word);
  }
  CHECK_EQ(sum, set_bit * total_iters);
}

void BM_FindLSBSetNonZero(int iters, int set_bit) {
  BM_FindXSBSetNonZero<uint32, Bits::FindLSBSetNonZero>(iters, set_bit);
}

void BM_FindLSBSetNonZero64(int iters, int set_bit) {
  BM_FindXSBSetNonZero<uint64, Bits::FindLSBSetNonZero64>(iters, set_bit);
}

void BM_FindMSBSetNonZero(int iters, int set_bit) {
  BM_FindXSBSetNonZero<uint32, Bits::FindMSBSetNonZero>(iters, set_bit);
}

void BM_FindMSBSetNonZero64(int iters, int set_bit) {
  BM_FindXSBSetNonZero<uint64, Bits::FindMSBSetNonZero64>(iters, set_bit);
}

BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero, 0);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero, 4);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero, 8);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero, 16);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero, 31);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 0);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 4);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 8);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 16);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 32);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 48);
BENCHMARK_WITH_ARG(BM_FindLSBSetNonZero64, 63);

BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero, 0);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero, 4);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero, 8);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero, 16);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero, 31);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 0);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 4);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 8);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 16);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 32);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 48);
BENCHMARK_WITH_ARG(BM_FindMSBSetNonZero64, 63);
#endif

// Function that does what ReverseBits*() do, but doing a bit-by-bit walk.
// The ReverseBits*() functions are much more efficient.
template <class T>
static T ExpectedReverseBits(T n) {
  T r = 0;
  for (size_t i = 0; i < sizeof(T) << 3 ; ++i) {
    r = (r << 1) | (n & 1);
    n >>= 1;
  }
  return r;
}

TEST_F(BitsTest, ReverseBitsInByte) {
  EXPECT_EQ(0, Bits::ReverseBits8(0));
  EXPECT_EQ(0xff, Bits::ReverseBits8(0xff));
  EXPECT_EQ(0x80, Bits::ReverseBits8(0x01));
  EXPECT_EQ(0x01, Bits::ReverseBits8(0x80));

  for (int i = 0; i < kNumReverseBitsRandomTests; ++i) {
    const uint8 n = RandomBits<uint8>();
    const uint8 r = Bits::ReverseBits8(n);
    EXPECT_EQ(n, Bits::ReverseBits8(r)) << n;
    EXPECT_EQ(ExpectedReverseBits<uint8>(n), r) << n;
    EXPECT_EQ(Bits::CountOnesInByte(n), Bits::CountOnesInByte(r)) << n;
  }
}

TEST_F(BitsTest, ReverseBitsIn32BitWord) {
  EXPECT_EQ(0UL, Bits::ReverseBits32(0));
  EXPECT_EQ(0xffffffffUL, Bits::ReverseBits32(0xffffffff));
  EXPECT_EQ(0x80000000UL, Bits::ReverseBits32(0x00000001));
  EXPECT_EQ(0x00000001UL, Bits::ReverseBits32(0x80000000));
  EXPECT_EQ(0x55555555UL, Bits::ReverseBits32(0xaaaaaaaa));
  EXPECT_EQ(0xaaaaaaaaUL, Bits::ReverseBits32(0x55555555));
  EXPECT_EQ(0xcafebabeUL, Bits::ReverseBits32(0x7d5d7f53));
  EXPECT_EQ(0x7d5d7f53UL, Bits::ReverseBits32(0xcafebabe));

  for (int i = 0; i < kNumReverseBitsRandomTests; ++i) {
    const uint32 n = RandomBits<uint32>();
    const uint32 r = Bits::ReverseBits32(n);
    EXPECT_EQ(n, Bits::ReverseBits32(r)) << n;
    EXPECT_EQ(ExpectedReverseBits<uint32>(n), r) << n;
    EXPECT_EQ(Bits::CountOnes(n), Bits::CountOnes(r)) << n;
  }
}

TEST_F(BitsTest, ReverseBitsIn64BitWord) {
  EXPECT_EQ(0ull, Bits::ReverseBits64(0ull));
  EXPECT_EQ(0xffffffffffffffffull, Bits::ReverseBits64(0xffffffffffffffffull));
  EXPECT_EQ(0x8000000000000000ull, Bits::ReverseBits64(0x0000000000000001ull));
  EXPECT_EQ(0x0000000000000001ull, Bits::ReverseBits64(0x8000000000000000ull));
  EXPECT_EQ(0x5555555555555555ull, Bits::ReverseBits64(0xaaaaaaaaaaaaaaaaull));
  EXPECT_EQ(0xaaaaaaaaaaaaaaaaull, Bits::ReverseBits64(0x5555555555555555ull));

  for (int i = 0; i < kNumReverseBitsRandomTests; ++i) {
    const uint64 n = RandomBits<uint64>();
    const uint64 r = Bits::ReverseBits64(n);
    EXPECT_EQ(n, Bits::ReverseBits64(r)) << n;
    EXPECT_EQ(ExpectedReverseBits<uint64>(n), r) << n;
    EXPECT_EQ(Bits::CountOnes64(n), Bits::CountOnes64(Bits::ReverseBits64(n)))
        << n;
  }
}

TEST_F(BitsTest, ReverseBitsIn128BitWord) {
  EXPECT_EQ(absl::uint128(0), Bits::ReverseBits128(absl::uint128(0)));
  EXPECT_EQ(~absl::uint128(0), Bits::ReverseBits128(~absl::uint128(0)));
  EXPECT_EQ(absl::MakeUint128(0x8000000000000000ull, 0),
            Bits::ReverseBits128(absl::uint128(1)));
  EXPECT_EQ(absl::uint128(1),
            Bits::ReverseBits128(absl::MakeUint128(0x8000000000000000ull, 0)));
  EXPECT_EQ(absl::MakeUint128(0x5555555555555555ull, 0x5555555555555555ull),
            Bits::ReverseBits128(absl::MakeUint128(0xaaaaaaaaaaaaaaaaull,
                                                   0xaaaaaaaaaaaaaaaaull)));
  EXPECT_EQ(absl::MakeUint128(0xaaaaaaaaaaaaaaaaull, 0xaaaaaaaaaaaaaaaaull),
            Bits::ReverseBits128(absl::MakeUint128(0x5555555555555555ull,
                                                   0x5555555555555555ull)));

  for (int i = 0; i < kNumReverseBitsRandomTests; ++i) {
    const absl::uint128 n =
        absl::MakeUint128(RandomBits<uint64>(), RandomBits<uint64>());
    const absl::uint128 r = Bits::ReverseBits128(n);
    EXPECT_EQ(n, Bits::ReverseBits128(r)) << n;
    EXPECT_EQ(ExpectedReverseBits<absl::uint128>(n), r) << n;
    EXPECT_EQ(Bits::CountOnes128(n),
              Bits::CountOnes128(r)) << n;
  }
}

#if 0
// This must be an unsigned power of 2 so that % kNumRandomNumbersForBenchmark
// can be computed cheaply.  Otherwise, we skew our results by doing signed
// division in benchmarks loops.
static constexpr uint32 kNumRandomNumbersForBenchmark = 16;

template <class T>
static void RandomNumbersForBenchmark(vector<T>* nums) {
  StopBenchmarkTiming();
  MTRandom random(test_random_seed);
  nums->clear();
  nums->resize(kNumRandomNumbersForBenchmark, 0);
  for (int i = 0; i < kNumRandomNumbersForBenchmark; ++i) {
    (*nums)[i] = static_cast<T>(random.Rand64() >> (64 - (sizeof(T) << 3)));
  }
  StartBenchmarkTiming();
}

template <class T>
static void RandomNumbersForLeadingZerosBenchmark(vector<T>* nums) {
  StopBenchmarkTiming();
  MTRandom random(test_random_seed);
  nums->clear();
  nums->resize(kNumRandomNumbersForBenchmark, 0);
  const uint64 top_bit = static_cast<uint64>(1) << 63;
  for (int i = 0; i < kNumRandomNumbersForBenchmark; ++i) {
    int count = RandomUniform<int>(0, sizeof(T) * 8 + 1);
    (*nums)[i] = (count == sizeof(T) * 8
                  ? 0
                  : static_cast<T>(random.Rand64() | top_bit) >> count);
  }
  StartBenchmarkTiming();
}

void BM_ReverseBits8(int iters) {
  vector<uint8> nums;
  RandomNumbersForBenchmark<uint8>(&nums);
  uint8 x = 0;
  while (iters-- > 0) {
    x += Bits::ReverseBits8(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}


void BM_ReverseBits32(int iters) {
  vector<uint32> nums;
  RandomNumbersForBenchmark<uint32>(&nums);
  uint32 x = 0;
  while (iters-- > 0) {
    x += Bits::ReverseBits32(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

void BM_ReverseBits64(int iters) {
  vector<uint64> nums;
  RandomNumbersForBenchmark<uint64>(&nums);
  uint64 x = 0;
  while (iters-- > 0) {
    x += Bits::ReverseBits64(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

BENCHMARK(BM_ReverseBits8);
BENCHMARK(BM_ReverseBits32);
BENCHMARK(BM_ReverseBits64);

void BM_CountOnes8(int iters) {
  vector<uint8> nums;
  RandomNumbersForBenchmark<uint8>(&nums);
  uint8 x = 0;
  while (iters-- > 0) {
    x += Bits::CountOnesInByte(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

void BM_CountOnes32(int iters) {
  vector<uint32> nums;
  RandomNumbersForBenchmark<uint32>(&nums);
  uint32 x = 0;
  while (iters-- > 0) {
    x += Bits::CountOnes(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

void BM_CountOnes64(int iters) {
  vector<uint64> nums;
  RandomNumbersForBenchmark<uint64>(&nums);
  uint32 x = 0;
  while (iters-- > 0) {
    x += Bits::CountOnes64(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

void BM_CountLeadingZeros32(int iters) {
  vector<uint32> nums;
  RandomNumbersForLeadingZerosBenchmark<uint32>(&nums);
  uint32 x = 0;
  while (iters-- > 0) {
    x += Bits::CountLeadingZeros32(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

void BM_CountLeadingZeros64(int iters) {
  vector<uint64> nums;
  RandomNumbersForLeadingZerosBenchmark<uint64>(&nums);
  uint64 x = 0;
  while (iters-- > 0) {
    x += Bits::CountLeadingZeros64(nums[iters % kNumRandomNumbersForBenchmark]);
  }
  StopBenchmarkTiming();
  // ensure the compiler doesn't optimize out the loop to a noop
  VLOG(1) << x;
}

BENCHMARK(BM_CountOnes8);
BENCHMARK(BM_CountOnes32);
BENCHMARK(BM_CountOnes64);
BENCHMARK(BM_CountLeadingZeros32);
BENCHMARK(BM_CountLeadingZeros64);

int main(int argc, char **argv) {
  LogToStderr();
  InitGoogle("", &argc, &argv, true);
  RunSpecifiedBenchmarks();

  CHECK_LE(max_bytes, kMaxBytes);
  return RUN_ALL_TESTS();
}
#endif

}  // namespace stratum
