// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/lib/signal_waiter.h"

#include <pthread.h>
#include <signal.h>

#include <string>

#include "absl/time/clock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace hal {

class SignalWaiterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    int sem_value = GetSemaphoreValue();
    ASSERT_EQ(0, sem_value);
    LOG(INFO) << "Starting test with semaphore value: " << sem_value;
  }

  void TearDown() override {
    int sem_value = GetSemaphoreValue();
    EXPECT_LE(1, sem_value);
    LOG(INFO) << "Ended test with semaphore value: " << sem_value;
    Reset();
  }

  void Reset() {
    CHECK_ERR(sem_destroy(&SignalWaiter::instance_.sem_));
    CHECK_ERR(sem_init(&SignalWaiter::instance_.sem_, 0, 0));
  }

  int GetSemaphoreValue() {
    int sem_value;
    CHECK_ERR(sem_getvalue(&SignalWaiter::instance_.sem_, &sem_value));
    return sem_value;
  }

  void ConstructWaiter(const std::vector<int> signals) {
    SignalWaiter waiter(signals);
  }
};

TEST_F(SignalWaiterTest, SignalBeforeWait) {
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  SignalWaiter::WaitForSignal();
}

TEST_F(SignalWaiterTest, SignalBeforeDoubleWait) {
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  SignalWaiter::WaitForSignal();
  SignalWaiter::WaitForSignal();
}

TEST_F(SignalWaiterTest, DoubleSignalBeforeWait) {
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  SignalWaiter::WaitForSignal();
  EXPECT_EQ(2, GetSemaphoreValue());
}

TEST_F(SignalWaiterTest, DoubleSignalBeforeDoubleWait) {
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  SignalWaiter::WaitForSignal();
  SignalWaiter::WaitForSignal();
  EXPECT_EQ(2, GetSemaphoreValue());
}

TEST_F(SignalWaiterTest, InvalidSignal) {
  ::util::Status status = SignalWaiter::HandleSignal(SIGUSR1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(0, GetSemaphoreValue());
  // Send a registered signal because the test framework expects one.
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
}

namespace {

constexpr absl::Duration kShutdownThreadSleep = ::absl::Seconds(3);
const int kWaitReturn = 0x3333;
const int kSignalReturn = 0x4444;

void* TestWaitThread(void*) {
  // Disable SA_RESTART on SIGUSR2
  struct sigaction sa;
  CHECK_ERR(sigaction(SIGUSR2, NULL, &sa));
  sa.sa_flags = sa.sa_flags & ~SA_RESTART;
  CHECK_ERR(sigaction(SIGUSR2, &sa, NULL));

  LOG(INFO) << "Waiting...";
  SignalWaiter::WaitForSignal();
  return new int{kWaitReturn};
}

void* TestSignalThread(void* arg) {
  const int* signal = static_cast<int*>(arg);
  ::absl::SleepFor(kShutdownThreadSleep);  // some sleep to emulate a task.
  SignalWaiter::HandleSignal(*signal);
  return new int{kSignalReturn};
}

void JoinThread(const pthread_t& tid, int expected) {
  void* void_val;
  ASSERT_EQ(0, pthread_join(tid, &void_val));
  int* int_val = static_cast<int*>(void_val);
  EXPECT_EQ(expected, *int_val);
  delete int_val;
}

}  // namespace

TEST_F(SignalWaiterTest, WaitInThread) {
  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestWaitThread, nullptr));
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  JoinThread(tid, kWaitReturn);
}

TEST_F(SignalWaiterTest, DoubleWaitInThread) {
  pthread_t tid1;
  pthread_t tid2;
  ASSERT_EQ(0, pthread_create(&tid1, nullptr, &TestWaitThread, nullptr));
  ASSERT_EQ(0, pthread_create(&tid2, nullptr, &TestWaitThread, nullptr));
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));

  JoinThread(tid1, kWaitReturn);
  JoinThread(tid2, kWaitReturn);
}

TEST_F(SignalWaiterTest, SignalInThread) {
  int signal = SIGINT;
  pthread_t tid;
  const absl::Time start = absl::Now();

  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestSignalThread, &signal));
  SignalWaiter::WaitForSignal();

  const absl::Time end = absl::Now();
  // Make sure we waited at least 90% of the time
  EXPECT_LE(kShutdownThreadSleep * .9, end - start);

  JoinThread(tid, kSignalReturn);
}

TEST_F(SignalWaiterTest, SignalAndWaitInThreads) {
  int signal = SIGINT;
  pthread_t signal_tid;
  pthread_t wait_tid;
  ASSERT_EQ(0,
            pthread_create(&signal_tid, nullptr, &TestSignalThread, &signal));
  ASSERT_EQ(0, pthread_create(&wait_tid, nullptr, &TestWaitThread, nullptr));

  JoinThread(signal_tid, kSignalReturn);
  JoinThread(wait_tid, kWaitReturn);
}

TEST_F(SignalWaiterTest, SignalUsingPthreadKillUsr2) {
  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestWaitThread, nullptr));
  ::absl::SleepFor(kShutdownThreadSleep);  // give the thread a chance to start.

  ASSERT_EQ(0, pthread_kill(tid, SIGUSR2));

  JoinThread(tid, kWaitReturn);
}

TEST_F(SignalWaiterTest, SignalUsingPthreadKillInt) {
  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestWaitThread, nullptr));
  ::absl::SleepFor(kShutdownThreadSleep);  // give the thread a chance to start.

  ASSERT_EQ(0, pthread_kill(tid, SIGINT));

  JoinThread(tid, kWaitReturn);
}

TEST_F(SignalWaiterTest, SignalUsingKillTerm) {
  pthread_t tid;
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestWaitThread, nullptr));

  ASSERT_EQ(0, kill(getpid(), SIGTERM));

  JoinThread(tid, kWaitReturn);
}

namespace {

void TestCallback(int value) {
  LOG(INFO) << "Got signal " << strsignal(value) << " (" << value << ").";
}

}  // namespace

TEST_F(SignalWaiterTest, SignalUsingKillUsr1) {
  pthread_t tid;

  signal(SIGUSR1, TestCallback);
  ASSERT_EQ(0, pthread_create(&tid, nullptr, &TestWaitThread, nullptr));
  ASSERT_EQ(0, kill(getpid(), SIGUSR1));

  // Make sure the waiter has not stopped
  ::absl::SleepFor(kShutdownThreadSleep);
  ASSERT_NE(0, pthread_tryjoin_np(tid, nullptr));
  EXPECT_EQ(0, GetSemaphoreValue());

  // Send a registered signal because the test framework expects one.
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
  JoinThread(tid, kWaitReturn);
}

TEST_F(SignalWaiterTest, Constructor) {
  // Grab the handler
  struct sigaction sa1;
  CHECK_ERR(sigaction(SIGUSR1, NULL, &sa1));

  ConstructWaiter({SIGUSR1});

  // Make sure the old handler gets put back
  struct sigaction sa2;
  CHECK_ERR(sigaction(SIGUSR1, NULL, &sa2));
  EXPECT_EQ(sa1.sa_handler, sa2.sa_handler);

  // Send a registered signal because the test framework expects one.
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
}

TEST_F(SignalWaiterTest, ConstructorError) {
  ::std::string error_msg("Failed to register signal: ");
  error_msg += strsignal(SIGKILL);
  ASSERT_DEATH(ConstructWaiter({SIGKILL}), error_msg);

  // Send a registered signal because the test framework expects one.
  EXPECT_OK(SignalWaiter::HandleSignal(SIGINT));
}

}  // namespace hal
}  // namespace stratum
