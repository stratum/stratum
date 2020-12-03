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

## 4. Demonstrability

Tests should serve as a demonstration of how the API works and should be
consumed by users. Do not rely on private + friend or test-only methods. Bad
usage in unit tests suggest a bad API.

## 5. Resilience

Do not write tests that fail in surprising ways:

- Flaky: Tests that can be re-run with the same build in the same state and flip
    from passing to failing (or timing out).

- Brittle: Tests that can fail for changes unrelated to the code under test. Do
    not run the code twice to generate a golden output to compare against in a
    test.

- Ordering: Tests that fail if they are not run all together or in a particular
    order. Avoid changing global state.

- Non-hermeticity: Tests that fail if the same tests is run at the same time
    twice. Do not depend on external resources (network, storage).

- Deep dependence: Mock tests that fail if the implementation of class under
    test is refactored. Do not over-fit the mock expectations.


### References and further reading

[CppCon 2015: T. Winters & H. Wright “All Your Tests are Terrible..."](https://youtu.be/u5senBJUkPc),
[[Slides](https://github.com/CppCon/CppCon2015/tree/master/Presentations/All%20Your%20Tests%20Are%20Terrible)]

[CppCon 2015: Titus Winters "Lessons in Sustainability...”](https://youtu.be/zW-i9eVGU_k),
[[Slides](https://github.com/CppCon/CppCon2015/blob/master/Presentations/Lessons%20in%20Sustainability/Lessons%20in%20Sustainability%20-%20Titus%20Winters%20-%20CppCon%202015.pdf)]
