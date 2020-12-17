<!--
Copyright 2020-present Open Networking Foundation

SPDX-License-Identifier: Apache-2.0
-->
# Testing Rules

This document describes the rules in regards to testing the Stratum projects
follows. It serves both as a guide for code authors and as a reference for code
reviewers. Each rule is accompanied by counter examples on what to avoid.

All contributors must adhere to these rules.

## 0. Write tests

All new code must be accompanied by a test suite. It should live besides the
code and must be run as part of the build system and CI. Code without tests will
get deleted in the future:
"If you liked it, you should have put a test on it." - Beyonce Rule

## 1. Correctness

Do not test known bugs.
Test real scenarios with the class under test, not mocks.
A test must be correct by inspection, as there are no tests for itself.

```c++
// Don't test known bugs.

int square(int x) {
  // TODO(goofus): Implement
  return 0;
}

TEST(SquareTest, MathTests) {
  EXPECT_EQ(0, square(2));
  EXPECT_EQ(0, square(3));
  EXPECT_EQ(0, square(7));
}
```

```c++
// Don't write tests not executing real scenarios.

class MockWorld : public World {
  // For simplicity, we assume the world is flat
  bool IsFlat() override { return true; }
};

TEST(Flat, WorldTests) {
  MockWorld world;
  EXPECT_TRUE(world.Populate());
  EXPECT_TRUE(world.IsFlat());
}
```

## 2. Readability

Test should be obvious to the future reader. Avoid boilerplate and distractions,
but provide enough context. A test should be like a novel: setup (test data),
action (function call), conclusion (result).

```c++
// Avoid too much boilerplate or distractions in tests.

TEST(BigSystemTest, CallIsUnimplemented) {
  // Meaningless setup.
  TestStorageSystem storage;
  auto test_data = GetTestFileMap();
  storage.MapFilesystem(test_data);
  BigSystem system;
  ASSERT_OK(system.Initialize(5));
  ThreadPool pool(10);
  pool.StartThreads();
  storage.SetThreads(pool);
  system.SetStorage(storage);
  ASSERT_TRUE(system.IsRunning());

  // Actual test.
  EXPECT_TRUE(IsUnimplemented(system.Status()));
}
```

```c++
// Keep enough context for the reader.

TEST(BigSystemTest, ReadMagicBytes) {
  BigSystem system = InitializeTestSystemAndTestData();
  EXPECT_EQ(42, system.PrivateKey());
}
```

## 3. Completeness

Tests should cover common inputs, corner cases and outlandish cases.
Only test the behavior of the API under test, not unrelated code.

```c++
// Do not write tests for APIs that are not yours.

TEST(FilterTest, WithVector) {
  vector<int> v;  // Make sure that vector is working.
  v.push_back(1);
  EXPECT_EQ(1, v.size());
  v.clear();
  EXPECT_EQ(0, v.size());
  EXPECT_TRUE(v.empty());

  // Now test our filter.
  v = Filter({1, 2, 3, 4, 5}, [](int x) { return x % 2 == 0; });
  EXPECT_THAT(v, ElementsAre(2, 4));
}
```

## 4. Demonstrability

Tests should serve as a demonstration of how the API works and should be
consumed by users. Do not rely on private + friend or test-only methods. Bad
usage in unit tests suggest a bad API.

```c++
// Do not write tests that depend on private APIs.

class Foo {
  friend FooTest;

 public:
  bool Setup();

 private:
  bool ShortcutSetupForTesting();
};

TEST(FooTest, Setup) {
  EXPECT_TRUE(ShortcutSetupForTesting());
}
```

## 5. Resilience

Do not write tests that fail in surprising ways:

- Flaky: Tests that can be re-run with the same build in the same state and flip
    from passing to failing (or timing out).

```c++
TEST(UpdaterTest, RunsFast) {
  Updater updater;
  updater.UpdateAsync();
  SleepFor(Seconds(.5));  // Half a second should be *plenty*.
  EXPECT_TRUE(updater.Updated());
}
 ```

- Brittle: Tests that can fail for changes unrelated to the code under test. Do
    not run the code twice to generate a golden output to compare against in a
    test.

```c++
TEST(Tags, ContentsAreCorrect) {
  TagSet tags = {5, 8, 10};

  // TODO(goofus): Figure out why these are ordered funny.
  EXPECT_THAT(tags, ElementsAre(8, 5, 10));
}
```

```c++
TEST(MyTest, LogWasCalled) {
  StartLogCapture();
  EXPECT_TRUE(Frobber::Start());
  EXPECT_THAT(Logs(), Contains("file.cc:421: Opened file frobber.config"));
}
```

- Ordering: Tests that fail if they are not run all together or in a particular
    order. Avoid changing global state.

```c++
static int i = 0;
TEST(Foo, First) {
  ASSERT_EQ(0, i);
  ++i;
}
TEST(Foo, Second) {
  ASSERT_EQ(1, i);
  ++i;
}
```

- Non-hermeticity: Tests that fail if the same tests is run at the same time
    twice. Do not depend on external resources (network, storage).

```c++
TEST(Foo, StorageTest) {
  StorageServer* server = GetStorageServerHandle();
  auto my_val = rand();
  server->Store("testkey", my_val);
  EXPECT_EQ(my_val, server->Load("testkey"));
}
```

- Deep dependence: Mock tests that fail if the implementation of class under
    test is refactored. Do not over-fit the mock expectations.

```c++
class File {
 public:
  ...
  virtual bool Stat(Stat* stat);
  virtual bool StatWithOptions(Stat* stat, StatOptions options) {
    return Stat(stat);  // Ignore options by default
  }
};
TEST(MyTest, FSUsage) {
  ... EXPECT_CALL(file, Stat(_)).Times(1);
  Frobber::Start();
}
```

## References and further reading

This document heavily based on talks from Titus Winters and Hyrum Wright. Check
these out for more information:

[CppCon 2015: T. Winters & H. Wright “All Your Tests are Terrible..."](https://youtu.be/u5senBJUkPc),
[[Slides](https://github.com/CppCon/CppCon2015/blob/master/Presentations/All%20Your%20Tests%20Are%20Terrible/All%20Your%20Tests%20Are%20Terrible%20-%20Titus%20Winters%20and%20Hyrum%20Wright%20-%20CppCon%202015.pdf)]

[CppCon 2015: Titus Winters "Lessons in Sustainability...”](https://youtu.be/zW-i9eVGU_k),
[[Slides](https://github.com/CppCon/CppCon2015/blob/master/Presentations/Lessons%20in%20Sustainability/Lessons%20in%20Sustainability%20-%20Titus%20Winters%20-%20CppCon%202015.pdf)]
