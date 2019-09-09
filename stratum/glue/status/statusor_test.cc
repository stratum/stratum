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


// Unit tests for StatusOr

#include "stratum/glue/status/statusor.h"

#include <errno.h>
#include <memory>
#include <vector>
#include <algorithm>

#include "stratum/glue/logging.h"
#include "stratum/glue/status/posix_error_space.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace util {
namespace {

using ::util::Status;
using util::PosixErrorToStatus;

class Base1 {
 public:
  virtual ~Base1() {}
  int pad;
};

class Base2 {
 public:
  virtual ~Base2() {}
  int yetotherpad;
};

class Derived : public Base1, public Base2 {
 public:
  virtual ~Derived() {}
  int evenmorepad;
};

class CopyNoAssign {
 public:
  explicit CopyNoAssign(int value) : foo(value) {}
  CopyNoAssign(const CopyNoAssign& other) : foo(other.foo) {}
  int foo;

 private:
  const CopyNoAssign& operator=(const CopyNoAssign&);
};

StatusOr<std::unique_ptr<int>> ReturnUniquePtr() {
  // Uses implicit constructor from T&&
  return std::unique_ptr<int>(new int(0));
}

TEST(StatusOr, TestMoveOnlyInitialization) {
  StatusOr<std::unique_ptr<int>> thing(ReturnUniquePtr());
  ASSERT_TRUE(thing.ok());
  EXPECT_EQ(0, *thing.ValueOrDie());
  int* previous = thing.ValueOrDie().get();

  thing = ReturnUniquePtr();
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(0, *thing.ValueOrDie());
  EXPECT_NE(previous, thing.ValueOrDie().get());
}

TEST(StatusOr, TestMoveOnlyValueExtraction) {
  StatusOr<std::unique_ptr<int>> thing(ReturnUniquePtr());
  ASSERT_TRUE(thing.ok());
  std::unique_ptr<int> ptr = thing.ConsumeValueOrDie();
  EXPECT_EQ(0, *ptr);
}

TEST(StatusOr, TestMoveOnlyConversion) {
  StatusOr<std::unique_ptr<const int>> const_thing(ReturnUniquePtr());
  EXPECT_TRUE(const_thing.ok());
  EXPECT_EQ(0, *const_thing.ValueOrDie());

  // Test rvalue converting assignment
  const int* const_previous = const_thing.ValueOrDie().get();
  const_thing = ReturnUniquePtr();
  EXPECT_TRUE(const_thing.ok());
  EXPECT_EQ(0, *const_thing.ValueOrDie());
  EXPECT_NE(const_previous, const_thing.ValueOrDie().get());
}

TEST(StatusOr, TestMoveOnlyVector) {
  // Sanity check that StatusOr<MoveOnly> works in vector.
  std::vector<StatusOr<std::unique_ptr<int>>> vec;
  vec.push_back(ReturnUniquePtr());
  vec.resize(2);
  auto another_vec = std::move(vec);
  EXPECT_TRUE(another_vec[0].ok());
  EXPECT_EQ(0, *another_vec[0].ValueOrDie());
  EXPECT_EQ(Status::UNKNOWN, another_vec[1].status());
}

TEST(StatusOr, TestDefaultCtor) {
  StatusOr<int> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_TRUE(thing.status().Matches(Status::UNKNOWN));
}

TEST(StatusOrDeathTest, TestDefaultCtorValue) {
  StatusOr<int> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::unknown");
}

TEST(StatusOr, TestStatusCtor) {
  StatusOr<int> thing(Status::CANCELLED);
  EXPECT_FALSE(thing.ok());
  EXPECT_TRUE(thing.status().Matches(Status::CANCELLED));
}

TEST(StatusOrDeathTest, TestStatusCtorStatusOk) {
  EXPECT_DEBUG_DEATH({
    // This will DCHECK
    StatusOr<int> thing(Status::OK);
    // In optimized mode, we are actually going to get EINVAL for
    // status here, rather than crashing, so check that.
    EXPECT_FALSE(thing.ok());
    EXPECT_TRUE(thing.status().Matches(PosixErrorToStatus(EINVAL, "")));
  }, "Status::OK is not a valid constructor argument");
}

TEST(StatusOr, TestValueCtor) {
  const int kI = 4;
  StatusOr<int> thing(kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(kI, thing.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusOk) {
  const int kI = 4;
  StatusOr<int> original(kI);
  StatusOr<int> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
  EXPECT_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusNotOk) {
  StatusOr<int> original(Status::CANCELLED);
  StatusOr<int> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
}

TEST(StatusOr, TestCopyCtorNonAssignable) {
  const int kI = 4;
  CopyNoAssign value(kI);
  StatusOr<CopyNoAssign> original(value);
  StatusOr<CopyNoAssign> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
  EXPECT_EQ(original.ValueOrDie().foo, copy.ValueOrDie().foo);
}

TEST(StatusOr, TestCopyCtorStatusOKConverting) {
  const int kI = 4;
  StatusOr<int> original(kI);
  StatusOr<double> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
  EXPECT_DOUBLE_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestCopyCtorStatusNotOkConverting) {
  StatusOr<int> original(Status::CANCELLED);
  StatusOr<double> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
}

TEST(StatusOr, TestAssignmentStatusOk) {
  const int kI = 4;
  StatusOr<int> source(kI);
  StatusOr<int> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
  EXPECT_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestAssignmentStatusNotOk) {
  StatusOr<int> source(Status::CANCELLED);
  StatusOr<int> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
}

TEST(StatusOr, TestAssignmentStatusOKConverting) {
  const int kI = 4;
  StatusOr<int> source(kI);
  StatusOr<double> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
  EXPECT_DOUBLE_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestAssignmentStatusNotOkConverting) {
  StatusOr<int> source(Status::CANCELLED);
  StatusOr<double> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
}

TEST(StatusOr, TestStatus) {
  StatusOr<int> good(4);
  EXPECT_TRUE(good.ok());
  StatusOr<int> bad(Status::CANCELLED);
  EXPECT_FALSE(bad.ok());
  EXPECT_TRUE(bad.status().Matches(Status::CANCELLED));
}

TEST(StatusOr, TestValue) {
  const int kI = 4;
  StatusOr<int> thing(kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(kI, thing.ValueOrDie());
}

TEST(StatusOr, TestValueConst) {
  const int kI = 4;
  const StatusOr<int> thing(kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(kI, thing.ValueOrDie());
}

TEST(StatusOrDeathTest, TestValueNotOk) {
  StatusOr<int> thing(Status::CANCELLED);
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::cancelled");
}

TEST(StatusOrDeathTest, TestValueNotOkConst) {
  const StatusOr<int> thing(Status::UNKNOWN);
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::unknown");
}

TEST(StatusOr, TestPointerDefaultCtor) {
  StatusOr<int*> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_TRUE(thing.status().Matches(Status::UNKNOWN));
}

TEST(StatusOrDeathTest, TestPointerDefaultCtorValue) {
  StatusOr<int*> thing;
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::unknown");
}

TEST(StatusOr, TestPointerStatusCtor) {
  StatusOr<int*> thing(Status::CANCELLED);
  EXPECT_FALSE(thing.ok());
  EXPECT_TRUE(thing.status().Matches(Status::CANCELLED));
}

TEST(StatusOrDeathTest, TestPointerStatusCtorStatusOk) {
  EXPECT_DEBUG_DEATH({
    StatusOr<int*> thing(Status::OK);
    // In optimized mode, we are actually going to get EINVAL for
    // status here, rather than crashing, so check that.
    EXPECT_FALSE(thing.ok());
    EXPECT_TRUE(thing.status().Matches(PosixErrorToStatus(EINVAL, "")));
  }, "Status::OK is not a valid constructor argument");
}

TEST(StatusOr, TestPointerValueCtor) {
  const int kI = 4;
  StatusOr<const int*> thing(&kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOrDeathTest, TestPointerValueCtorNullValue) {
  EXPECT_DEBUG_DEATH({
    StatusOr<int*> thing(NULL);
    // In optimized mode, we are actually going to get EINVAL for
    // status here, rather than DCHECKing, so verify that
    EXPECT_FALSE(thing.ok());
    EXPECT_TRUE(thing.status().Matches(PosixErrorToStatus(EINVAL, "")));
  }, "NULL is not a valid constructor argument to StatusOr");
}

TEST(StatusOr, TestPointerCopyCtorStatusOk) {
  const int kI = 0;
  StatusOr<const int*> original(&kI);
  StatusOr<const int*> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
  EXPECT_EQ(original.ValueOrDie(), copy.ValueOrDie());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOk) {
  StatusOr<int*> original(Status::CANCELLED);
  StatusOr<int*> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
}

TEST(StatusOr, TestPointerCopyCtorStatusOKConverting) {
  Derived derived;
  StatusOr<Derived*> original(&derived);
  StatusOr<Base2*> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
  EXPECT_EQ(static_cast<const Base2*>(original.ValueOrDie()),
            copy.ValueOrDie());
}

TEST(StatusOr, TestPointerCopyCtorStatusNotOkConverting) {
  StatusOr<Derived*> original(Status::CANCELLED);
  StatusOr<Base2*> copy(original);
  EXPECT_TRUE(copy.status().Matches(original.status()));
}

TEST(StatusOr, TestPointerAssignmentStatusOk) {
  const int kI = 0;
  StatusOr<const int*> source(&kI);
  StatusOr<const int*> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
  EXPECT_EQ(source.ValueOrDie(), target.ValueOrDie());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOk) {
  StatusOr<int*> source(Status::CANCELLED);
  StatusOr<int*> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
}

TEST(StatusOr, TestPointerAssignmentStatusOKConverting) {
  Derived derived;
  StatusOr<Derived*> source(&derived);
  StatusOr<Base2*> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
  EXPECT_EQ(static_cast<const Base2*>(source.ValueOrDie()),
            target.ValueOrDie());
}

TEST(StatusOr, TestPointerAssignmentStatusNotOkConverting) {
  StatusOr<Derived*> source(Status::CANCELLED);
  StatusOr<Base2*> target;
  target = source;
  EXPECT_TRUE(target.status().Matches(source.status()));
}

TEST(StatusOr, TestPointerStatus) {
  const int kI = 0;
  StatusOr<const int*> good(&kI);
  EXPECT_TRUE(good.ok());
  StatusOr<const int*> bad(Status::CANCELLED);
  EXPECT_TRUE(bad.status().Matches(Status::CANCELLED));
}

TEST(StatusOr, TestPointerValue) {
  const int kI = 0;
  StatusOr<const int*> thing(&kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOr, TestPointerValueConst) {
  const int kI = 0;
  const StatusOr<const int*> thing(&kI);
  EXPECT_TRUE(thing.ok());
  EXPECT_EQ(&kI, thing.ValueOrDie());
}

TEST(StatusOrDeathTest, TestPointerValueNotOk) {
  StatusOr<int*> thing(Status::CANCELLED);
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::cancelled");
}

TEST(StatusOrDeathTest, TestPointerValueNotOkConst) {
  const StatusOr<int*> thing(Status::CANCELLED);
  EXPECT_FALSE(thing.ok());
  EXPECT_DEATH(thing.ValueOrDie(), "generic::cancelled");
}

#ifdef BENCHMARK
// A factory to help us benchmark the various factory styles. All of
// the factory methods are marked as non-inlineable so as to more
// accurately simulate calling a factory for which you do not have
// visibility of implementation. Similarly, the value_ variable is
// marked volatile to prevent the compiler from getting too clever
// about detecting that the same value is used in all loop iterations.
template <typename T>
class BenchmarkFactory {
 public:
  // Construct a new factory. Allocate an object which will always
  // be the result of the factory methods.
  BenchmarkFactory() : value_(new T) {}

  // Destroy this factory, including the result value.
  ~BenchmarkFactory() { delete value_; }

  // A trivial factory that just returns the value. There is no status
  // object that could be returned to encapsulate an error
  T* TrivialFactory() ABSL_ATTRIBUTE_NOINLINE { return value_; }

  // A more sophisticated factory, which returns a status to indicate
  // the result of the operation. The factory result is populated into
  // the user provided pointer result.
  Status ArgumentFactory(T** result) ABSL_ATTRIBUTE_NOINLINE {
    *result = value_;
    return Status::OK;
  }

  Status ArgumentFactoryFail(T** result) ABSL_ATTRIBUTE_NOINLINE {
    *result = NULL;
    return Status::CANCELLED;
  }

  Status ArgumentFactoryFailPosix(T** result) ABSL_ATTRIBUTE_NOINLINE {
    *result = NULL;
    return PosixErrorToStatus(EINVAL, "");
  }

  Status ArgumentFactoryFailPosixString(T** result) ABSL_ATTRIBUTE_NOINLINE {
    *result = NULL;
    return PosixErrorToStatus(
        EINVAL, "a big string of message junk that will never be read");
  }

  // A factory that returns a StatusOr<T*>. If the factory operation
  // is OK, then the StatusOr<T*> will hold a T*. Otherwise, it will
  // hold a status explaining the error.
  StatusOr<T*> StatusOrFactory() ABSL_ATTRIBUTE_NOINLINE {
    return static_cast<T*>(value_);
  }

  StatusOr<T*> StatusOrFactoryFail() ABSL_ATTRIBUTE_NOINLINE {
    return Status::CANCELLED;
  }

  StatusOr<T*> StatusOrFactoryFailPosix() ABSL_ATTRIBUTE_NOINLINE {
    return PosixErrorToStatus(EINVAL, "");
  }

  StatusOr<T*> StatusOrFactoryFailPosixString() ABSL_ATTRIBUTE_NOINLINE {
    return PosixErrorToStatus(
        EINVAL, "a big string of message junk that will never be read");
  }

 private:
  T* volatile value_;
  BenchmarkFactory(const BenchmarkFactory&) = delete;
  BenchmarkFactory& operator=(const BenchmarkFactory&) = delete;
};

// A simple type we use with the factory.
class BenchmarkType {
 public:
  BenchmarkType() {}
  virtual ~BenchmarkType() {}
  virtual void DoWork() ABSL_ATTRIBUTE_NOINLINE {}

 private:
  BenchmarkType(const BenchmarkType&) = delete;
  BenchmarkType& operator=(const BenchmarkType&) = delete;
};

// Calibrate the amount of time spent just calling DoWork, since each of our
// tests will do this, we can subtract this out of benchmark results.
static void BM_CalibrateWorkLoop(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  BenchmarkType* result = factory.TrivialFactory();
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    if (result != NULL) result->DoWork();
  }
}
BENCHMARK(BM_CalibrateWorkLoop);

// Measure the time taken to call into the factory, return the value,
// determine that it is OK, and invoke a trivial function.
static void BM_TrivialFactory(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    BenchmarkType* result = factory.TrivialFactory();
    if (result != NULL) result->DoWork();
  }
}
BENCHMARK(BM_TrivialFactory);

// Measure the time taken to call into the factory, providing an
// out-param for the result, evaluating the status result and the
// result pointer, and invoking the trivial function.
static void BM_ArgumentFactory(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    BenchmarkType* result = NULL;
    Status status = factory.ArgumentFactory(&result);
    if (status.ok() && result != NULL) {
      result->DoWork();
    }
  }
}
BENCHMARK(BM_ArgumentFactory);

// Measure the time to use the StatusOr<T*> factory, evaluate the result,
// and invoke the trivial function.
static void BM_StatusOrFactory(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    StatusOr<BenchmarkType*> result = factory.StatusOrFactory();
    if (result.ok()) {
      result.ValueOrDie()->DoWork();
    }
  }
}
BENCHMARK(BM_StatusOrFactory);

// Measure the time taken to call into the factory, providing an
// out-param for the result, evaluating the status result and the
// result pointer, and invoking the trivial function.
static void BM_ArgumentFactoryFail(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    BenchmarkType* result = NULL;
    Status status = factory.ArgumentFactoryFail(&result);
    if (status.ok() && result != NULL) {
      result->DoWork();
    }
  }
}
BENCHMARK(BM_ArgumentFactoryFail);

// Measure the time to use the StatusOr<T*> factory, evaluate the result,
// and invoke the trivial function.
static void BM_StatusOrFactoryFail(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    StatusOr<BenchmarkType*> result = factory.StatusOrFactoryFail();
    if (result.ok()) {
      result.ValueOrDie()->DoWork();
    }
  }
}
BENCHMARK(BM_StatusOrFactoryFail);

// Measure the time taken to call into the factory, providing an
// out-param for the result, evaluating the status result and the
// result pointer, and invoking the trivial function.
static void BM_ArgumentFactoryFailPosix(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    BenchmarkType* result = NULL;
    Status status = factory.ArgumentFactoryFailPosix(&result);
    if (status.ok() && result != NULL) {
      result->DoWork();
    }
  }
}
BENCHMARK(BM_ArgumentFactoryFailPosix);

// Measure the time to use the StatusOr<T*> factory, evaluate the result,
// and invoke the trivial function.
static void BM_StatusOrFactoryFailPosix(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    StatusOr<BenchmarkType*> result = factory.StatusOrFactoryFailPosix();
    if (result.ok()) {
      result.ValueOrDie()->DoWork();
    }
  }
}
BENCHMARK(BM_StatusOrFactoryFailPosix);

// Measure the time taken to call into the factory, providing an
// out-param for the result, evaluating the status result and the
// result pointer, and invoking the trivial function.
static void BM_ArgumentFactoryFailPosixString(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    BenchmarkType* result = NULL;
    Status status = factory.ArgumentFactoryFailPosixString(&result);
    if (status.ok() && result != NULL) {
      result->DoWork();
    }
  }
}
BENCHMARK(BM_ArgumentFactoryFailPosixString);

// Measure the time to use the StatusOr<T*> factory, evaluate the result,
// and invoke the trivial function.
static void BM_StatusOrFactoryFailPosixString(int iters) {
  StopBenchmarkTiming();
  BenchmarkFactory<BenchmarkType> factory;
  StartBenchmarkTiming();
  for (int i = 0; i != iters; ++i) {
    StatusOr<BenchmarkType*> result = factory.StatusOrFactoryFailPosixString();
    if (result.ok()) {
      result.ValueOrDie()->DoWork();
    }
  }
}
BENCHMARK(BM_StatusOrFactoryFailPosixString);
#endif  // BENCHMARK

}  // namespace
}  // namespace util
