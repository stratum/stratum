// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

#include "stratum/lib/timer_daemon.h"

#include "absl/synchronization/mutex.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace hal {

class TimerDaemonTest : public ::testing::Test {
 protected:
  TimerDaemonTest() {}

  ~TimerDaemonTest() override {}

  void SetUp() override {
    absl::WriterMutexLock l(&access_lock_);
    // The deadlock detector incorrectly assumes that the following scenario
    // leads to a deadlock:
    // 1) test startup:
    // a) lock TimerDaemonTest::access_lock_ and reset counter
    // b) lock TimerDaemon::access_lock_ and mark daemon as started
    // 2) actual test:
    // a) lock TimerDaemon::access_lock_ and schedule a timer
    // b) lock TimerDaemon::access_lock_ and execute action that:
    // c) locks TimerDaemonTest::access_lock_ and updates counter
    // Different order of locking mutexes 1a/1b and 2b/2c leads the detector to
    // claim potential deadlock, which is a false positive.
    // Cleaning up history created in the SetUp() to remeady this.
    access_lock_.ForgetDeadlockInfo();
    ASSERT_OK(TimerDaemon::Start());
    count_ = 0;
  }

  void TearDown() override { ASSERT_OK(TimerDaemon::Stop()); }

  TimerDaemon::DescriptorPtr GetTimerDescriptorPtr(const absl::Time when) {
    TimerDaemon::DescriptorPtr desc = std::make_shared<TimerDaemon::Descriptor>(
        /* repeat = */ false, []() { return ::util::OkStatus(); });
    desc->due_time_ = when;
    return desc;
  }

  TimerDaemon::DescriptorWeakPtr GetTimerDescriptorWeakPtr(
      const TimerDaemon::DescriptorPtr& desc) {
    return TimerDaemon::DescriptorWeakPtr(desc);
  }

  // A counter used to check if timers are executed in correct order. Each timer
  // checks if the 'count_' has expected value and then increments it.
  // This simple mechanism allows for checking if all timers are handled as
  // expected.
  int count_ GUARDED_BY(access_lock_);
  // A Mutex used to guard access to the 'count_'.
  mutable absl::Mutex access_lock_;

  // A comparator used internally by the TimerSevice to compare timestamps.
  // This class is private, so, it is instantiated here to allow access by
  // relevant tests.
  TimerDaemon::TimerDescriptorComparator cmp_;
};

TEST_F(TimerDaemonTest, CompareSmallerBigger) {
  absl::Time now = absl::Now();
  auto desc1 = GetTimerDescriptorPtr(now);
  auto desc2 = GetTimerDescriptorPtr(now + absl::Milliseconds(10));

  auto weak1 = GetTimerDescriptorWeakPtr(desc1);
  auto weak2 = GetTimerDescriptorWeakPtr(desc2);

  // 0 > 10
  EXPECT_FALSE(cmp_(weak1, weak2));

  desc2.reset();
  // 0 > expired 10
  EXPECT_FALSE(cmp_(weak1, weak2));

  desc1.reset();
  // expired 0 > expired 10
  EXPECT_FALSE(cmp_(weak1, weak2));

  desc2 = GetTimerDescriptorPtr(now + absl::Milliseconds(10));
  weak2 = GetTimerDescriptorWeakPtr(desc2);
  // expired 0 > 10
  EXPECT_TRUE(cmp_(weak1, weak2));
}

TEST_F(TimerDaemonTest, CompareBiggerSmaller) {
  absl::Time now = absl::Now();
  auto desc1 = GetTimerDescriptorPtr(now + absl::Milliseconds(10));
  auto desc2 = GetTimerDescriptorPtr(now);

  auto weak1 = GetTimerDescriptorWeakPtr(desc1);
  auto weak2 = GetTimerDescriptorWeakPtr(desc2);

  // 10 > 0
  EXPECT_TRUE(cmp_(weak1, weak2));

  desc2.reset();
  // 10 > expired 0
  EXPECT_FALSE(cmp_(weak1, weak2));

  desc1.reset();
  // expired 10 > expired 0
  EXPECT_FALSE(cmp_(weak1, weak2));

  desc2 = GetTimerDescriptorPtr(now);
  weak2 = GetTimerDescriptorWeakPtr(desc2);
  // expired 10 > 0
  EXPECT_TRUE(cmp_(weak1, weak2));
}

TEST_F(TimerDaemonTest, CreateOneShot) {
  // This test verifies that TimerDaemon does create one-shot timer.
  TimerDaemon::DescriptorPtr desc;
  ASSERT_OK(TimerDaemon::RequestOneShotTimer(
      1000,
      [&]() {
        absl::WriterMutexLock l(&access_lock_);
        EXPECT_EQ(count_++, 0);
        return ::util::OkStatus();
      },
      &desc));
  usleep(1100000);
}

TEST_F(TimerDaemonTest, CreateTwoOneShots) {
  // This test verifies that TimerDaemon does create two one-shot timers.
  TimerDaemon::DescriptorPtr desc1, desc2;
  ASSERT_OK(TimerDaemon::RequestOneShotTimer(
      1000,
      [&]() {
        absl::WriterMutexLock l(&access_lock_);
        EXPECT_EQ(count_++, 1);
        return ::util::OkStatus();
      },
      &desc1));
  ASSERT_OK(TimerDaemon::RequestOneShotTimer(
      100,
      [&]() {
        absl::WriterMutexLock l(&access_lock_);
        EXPECT_EQ(count_++, 0);
        return ::util::OkStatus();
      },
      &desc2));
  usleep(1500000);
}

TEST_F(TimerDaemonTest, CreatePeriodic) {
  // This test verifies that TimerDaemon does create periodic timer.
}

}  // namespace hal
}  // namespace stratum
