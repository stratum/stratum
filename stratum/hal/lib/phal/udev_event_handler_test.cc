// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/udev_event_handler.h"

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/system_fake.h"
#include "stratum/hal/lib/phal/udev_event_handler_mock.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {
namespace hal {
namespace phal {
namespace {
// Parameters for ConcurrentUdevEventHandlerTest.
static const int kNumTestThreads = 20;
static const int kRequiredThreadedResults = 100;
}  // namespace

using ::testing::_;
using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

class UdevEventHandlerTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_OK(handler_.InitializeUdev()); }

  ::util::Status RunMonitorLoop() {
    RETURN_IF_ERROR(handler_.PollUdevMonitors());
    RETURN_IF_ERROR(handler_.SendCallbacks());
    return ::util::OkStatus();
  }

 protected:
  SystemFake system_fake_;
  // We don't use the normal MakeUdevEventHandler so that we can run
  // the udev monitor thread manually, rather than running it in a
  // separate thread.
  UdevEventHandler handler_{&system_fake_};
};

TEST_F(UdevEventHandlerTest, UdevEventCallbackRegistersAndUnregisters) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
}

TEST_F(UdevEventHandlerTest, NoCallbackToDeletedCallback) {
  {
    UdevEventCallbackMock callback("foo", "bar");
    EXPECT_OK(handler_.RegisterEventCallback(&callback));
  }
  system_fake_.SendUdevUpdate("foo", "bar", 1, "add", true);
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, UdevEventCallbackCannotRegisterTwice) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_FALSE(handler_.RegisterEventCallback(&callback).ok());
}

TEST_F(UdevEventHandlerTest, UdevEventCallbackCannotUnregisterTwice) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_FALSE(handler_.UnregisterEventCallback(&callback).ok());
}

TEST_F(UdevEventHandlerTest, CannotUseCallbackFromDifferentUdev) {
  auto other_handler_status =
      UdevEventHandler::MakeUdevEventHandler(&system_fake_);
  ASSERT_TRUE(other_handler_status.ok());
  auto other_handler = other_handler_status.ConsumeValueOrDie();
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_FALSE(other_handler->RegisterEventCallback(&callback).ok());
  EXPECT_FALSE(other_handler->UnregisterEventCallback(&callback).ok());
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
}

TEST_F(UdevEventHandlerTest, CallbacksCannotReceiveSameEvent) {
  UdevEventCallbackMock callback1("foo", "bar");
  UdevEventCallbackMock callback2("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback1));
  EXPECT_FALSE(handler_.RegisterEventCallback(&callback2).ok());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesLastAction) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", false);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, LastActionDefaultsToRemove) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("remove"))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesLastActionOnEachRegistration) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .Times(2)
      .WillRepeatedly(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", false);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesActionOnce) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", true);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesNewAction) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("remove"))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", true);
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesMultipleActions) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 1, "add", false);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
  EXPECT_CALL(callback, HandleUdevEvent("remove"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 2, "remove", true);
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, MultipleCallbacksReceiveRelevantActions) {
  UdevEventCallbackMock callback1("foo", "bar");
  UdevEventCallbackMock callback2("foo", "barbar");
  UdevEventCallbackMock callback3("foofoo", "bar");
  EXPECT_CALL(callback1, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(callback2, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(callback2, HandleUdevEvent("remove"))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(callback3, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 1, "add", false);
  system_fake_.SendUdevUpdate("foo", "barbar", 2, "add", false);
  system_fake_.SendUdevUpdate("foofoo", "bar", 3, "add", false);
  EXPECT_OK(handler_.RegisterEventCallback(&callback1));
  EXPECT_OK(handler_.RegisterEventCallback(&callback2));
  EXPECT_OK(handler_.RegisterEventCallback(&callback3));
  EXPECT_OK(RunMonitorLoop());
  system_fake_.SendUdevUpdate("foo", "barbar", 4, "remove", true);
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, NoCallbackForOutOfOrderOrDuplicateAction) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("add"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", true);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
  system_fake_.SendUdevUpdate("foo", "bar", 2, "old action", true);
  system_fake_.SendUdevUpdate("foo", "bar", 10, "add", true);  // duplicate
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, CallbackReceivesMostRecentAction) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_CALL(callback, HandleUdevEvent("remove"))
      .WillOnce(Return(::util::OkStatus()));
  system_fake_.SendUdevUpdate("foo", "bar", 1, "add", true);
  EXPECT_OK(RunMonitorLoop());
  system_fake_.SendUdevUpdate("foo", "bar", 2, "enable", true);
  EXPECT_OK(RunMonitorLoop());
  system_fake_.SendUdevUpdate("foo", "bar", 3, "remove", true);
  EXPECT_OK(RunMonitorLoop());
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(RunMonitorLoop());
}

TEST_F(UdevEventHandlerTest, HandlerRejectsBadUdevEvents) {
  UdevEventCallbackMock callback("foo", "bar");
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  // We reject an empty action.
  system_fake_.SendUdevUpdate("foo", "bar", 1, "", true);
  EXPECT_FALSE(RunMonitorLoop().ok());
  // We reject an empty dev_path.
  system_fake_.SendUdevUpdate("foo", "", 1, "add", true);
  EXPECT_FALSE(RunMonitorLoop().ok());
}

TEST_F(UdevEventHandlerTest, CallbackCanDeleteAnotherCallback) {
  UdevEventCallbackMock callback1("foo", "bar");
  std::unique_ptr<UdevEventCallbackMock> callback2(
      new UdevEventCallbackMock("foo", "sub-bar"));
  EXPECT_OK(handler_.RegisterEventCallback(&callback1));
  EXPECT_OK(handler_.RegisterEventCallback(callback2.get()));
  // Our first callback will delete our second callback!
  EXPECT_CALL(callback1, HandleUdevEvent("remove"))
      .WillOnce(DoAll(Assign(&callback2, nullptr), Return(::util::OkStatus())));
  system_fake_.SendUdevUpdate("foo", "bar", 1, "remove", true);
  EXPECT_OK(RunMonitorLoop());
  system_fake_.SendUdevUpdate("foo", "sub-bar", 1, "ignore remove", true);
  EXPECT_OK(RunMonitorLoop());
}

class ConcurrentUdevEventHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto handler = UdevEventHandler::MakeUdevEventHandler(&system_fake_);
    ASSERT_TRUE(handler.ok());
    handler_ = handler.ConsumeValueOrDie();
  }

  // Creates a new udev callback, sends some events that should trigger this
  // callback, and waits to receive them. This function is intended to be called
  // by many threads simultaneously.
  ::util::Status TriggerSomeCallbacks() {
    std::string dev_path;
    {
      absl::MutexLock lock(&test_counter_lock_);
      dev_path = "device" + std::to_string(udev_test_counter_++);
      udev_thread_counter_++;
      test_counter_cond_var_.Signal();
    }
    UdevEventCallbackMock callback("foo", dev_path);
    absl::Mutex event_lock;
    absl::CondVar event_cond_var;
    std::string most_recent_event;
    // Setup our callback to write to most_recent_event;
    EXPECT_CALL(callback, HandleUdevEvent(_))
        .WillRepeatedly(Invoke([&](std::string action) -> ::util::Status {
          absl::MutexLock lock(&event_lock);
          most_recent_event = action;
          event_cond_var.Signal();
          return ::util::OkStatus();
        }));
    // Send a couple udev events that should trigger our callback, and wait to
    // make sure the callback is sent.
    int seqnum;
    while (true) {
      {
        absl::MutexLock lock(&test_counter_lock_);
        if (udev_thread_counter_ == 0) break;
        seqnum = udev_test_counter_++;
      }
      RETURN_IF_ERROR(handler_->RegisterEventCallback(&callback));
      system_fake_.SendUdevUpdate("foo", dev_path, seqnum, dev_path + "add",
                                  true);
      {
        absl::MutexLock lock(&event_lock);
        while (most_recent_event != dev_path + "add")
          event_cond_var.Wait(&event_lock);
      }
      {
        absl::MutexLock lock(&test_counter_lock_);
        seqnum = udev_test_counter_++;
      }
      system_fake_.SendUdevUpdate("foo", dev_path, seqnum, dev_path + "remove",
                                  true);
      {
        absl::MutexLock lock(&event_lock);
        while (most_recent_event != dev_path + "remove")
          event_cond_var.Wait(&event_lock);
      }
      RETURN_IF_ERROR(handler_->UnregisterEventCallback(&callback));
      {
        // Tell the main thread that we've received callbacks successfully.
        absl::MutexLock lock(&test_counter_lock_);
        udev_test_results_++;
        test_counter_cond_var_.Signal();
      }
    }
    return ::util::OkStatus();
  }

 protected:
  SystemFake system_fake_;
  std::unique_ptr<UdevEventHandler> handler_;
  absl::Mutex test_counter_lock_;
  absl::CondVar test_counter_cond_var_;
  // Generates unique, increasing numbers. Used for both thread identifiers and
  // udev sequence numbers.
  int udev_test_counter_ GUARDED_BY(test_counter_lock_) = 1;
  // Counts the number of successful uses of UdevEventHandler by test threads.
  int udev_test_results_ GUARDED_BY(test_counter_lock_) = 0;
  // Counts the number of test threads currently running.
  // Stops all test threads when set back to 0.
  int udev_thread_counter_ GUARDED_BY(test_counter_lock_) = 0;
};

TEST_F(ConcurrentUdevEventHandlerTest, ManyConcurrentCallbacksExecute) {
  std::vector<pthread_t> test_threads;
  for (int i = 0; i < kNumTestThreads; i++) {
    pthread_t test_thread;
    ASSERT_EQ(
        0, pthread_create(&test_thread, nullptr,
                          +[](void* t) -> void* {
                            if (!static_cast<ConcurrentUdevEventHandlerTest*>(t)
                                     ->TriggerSomeCallbacks()
                                     .ok())
                              return t;  // Error, non-zero return.
                            return nullptr;
                          },
                          this));
    test_threads.push_back(test_thread);
  }
  {
    absl::MutexLock lock(&test_counter_lock_);
    // Wait for all of our test threads to start.
    while (udev_thread_counter_ < kNumTestThreads)
      test_counter_cond_var_.Wait(&test_counter_lock_);
    // Now all of the test threads have started. Wait for results.
    udev_test_results_ = 0;
    while (udev_test_results_ < kRequiredThreadedResults)
      test_counter_cond_var_.Wait(&test_counter_lock_);
    // We've seen enough successful callbacks. Stop our test threads.
    udev_thread_counter_ = 0;
  }
  for (pthread_t thread : test_threads) {
    void* status;
    pthread_join(thread, &status);
    EXPECT_EQ(status, nullptr);
  }
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
