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

#include "stratum/hal/lib/phal/onlp/onlp_event_handler.h"

#include <functional>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler_mock.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

class OnlpEventHandlerTest : public ::testing::Test {
 public:
  ::util::Status PollOids() { return handler_.PollOids(); }
  ::util::Status RunPolling() { return handler_.InitializePollingThread(); }

 protected:
  StrictMock<OnlpWrapperMock> onlp_;
  OnlpEventHandler handler_{&onlp_};
};

namespace {
class CallbackMock : public OnlpEventCallback {
 public:
  explicit CallbackMock(OnlpOid oid) : OnlpEventCallback(oid) {}
  MOCK_METHOD1(HandleOidStatusChange, ::util::Status(const OidInfo&));
};

TEST_F(OnlpEventHandlerTest, OnlpEventCallbackRegistersAndUnregisters) {
  CallbackMock callback(1234);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
}

TEST_F(OnlpEventHandlerTest, CannotDoubleRegisterOrUnregister) {
  CallbackMock callback(1234);
  EXPECT_OK(handler_.RegisterEventCallback(&callback));
  EXPECT_THAT(handler_.RegisterEventCallback(&callback),
              StatusIs(_, _, HasSubstr("already registered.")));
  EXPECT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_THAT(handler_.UnregisterEventCallback(&callback),
              StatusIs(_, _, HasSubstr("not currently registered.")));
}

TEST_F(OnlpEventHandlerTest, RegisterSeveralCallbacks) {
  CallbackMock callback1(1234);
  CallbackMock callback1_conflict(1234);
  CallbackMock callback2(1337);
  EXPECT_OK(handler_.RegisterEventCallback(&callback1));
  EXPECT_OK(handler_.RegisterEventCallback(&callback2));
  EXPECT_THAT(handler_.RegisterEventCallback(&callback1_conflict),
              StatusIs(_, _, HasSubstr("two callbacks for the same OID.")));
}

TEST_F(OnlpEventHandlerTest, UnusedHandlerCanPollOids) {
  // Poll a bunch. There are no registered callbacks, so this should be a no-op.
  for (int i = 0; i < 10; i++) EXPECT_OK(PollOids());
}

TEST_F(OnlpEventHandlerTest, CallbackSendsInitialUpdate) {
  CallbackMock callback1(1234);
  CallbackMock callback2(1235);
  ASSERT_OK(handler_.RegisterEventCallback(&callback1));
  ASSERT_OK(handler_.RegisterEventCallback(&callback2));

  onlp_oid_hdr_t fake_oid;
  fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(onlp_, GetOidInfo(1235)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(callback1, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(callback2, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(PollOids());
}

TEST_F(OnlpEventHandlerTest, ExecutesAllCallbacksDespiteFailures) {
  CallbackMock callback1(1234);
  CallbackMock callback2(1235);
  ASSERT_OK(handler_.RegisterEventCallback(&callback1));
  ASSERT_OK(handler_.RegisterEventCallback(&callback2));

  onlp_oid_hdr_t fake_oid;
  fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(onlp_, GetOidInfo(1235)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(callback1, HandleOidStatusChange(_))
      .WillOnce(Return(::util::Status{MAKE_ERROR() << "callback1 failure"}));
  EXPECT_CALL(callback2, HandleOidStatusChange(_))
      .WillOnce(Return(::util::Status{MAKE_ERROR() << "callback2 failure"}));
  EXPECT_THAT(PollOids(), StatusIs(_, _,
                                   AllOf(HasSubstr("callback1 failure"),
                                         HasSubstr("callback2 failure"))));
}

TEST_F(OnlpEventHandlerTest, CallbackOnlySentAfterUpdate) {
  CallbackMock callback(1234);
  ASSERT_OK(handler_.RegisterEventCallback(&callback));

  onlp_oid_hdr_t fake_oid;
  fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(callback, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(PollOids());

  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  // No call to HandleOidStatusChange, since the oid status hasn't changed.
  EXPECT_OK(PollOids());

  fake_oid.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(callback, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(PollOids());
}

TEST_F(OnlpEventHandlerTest, UpdateCallbackSentAfterAnyUpdate) {
  CallbackMock callback1(1234);
  CallbackMock callback2(1235);
  ASSERT_OK(handler_.RegisterEventCallback(&callback1));
  ASSERT_OK(handler_.RegisterEventCallback(&callback2));
  MockFunction<void(::util::Status)> mock_update_callback;
  handler_.AddUpdateCallback(mock_update_callback.AsStdFunction());

  onlp_oid_hdr_t fake_oid;
  fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;

  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(onlp_, GetOidInfo(1235)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(callback1, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(callback2, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(mock_update_callback, Call(::util::OkStatus()));
  EXPECT_OK(PollOids());

  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  EXPECT_CALL(onlp_, GetOidInfo(1235)).WillOnce(Return(OidInfo(fake_oid)));
  // No call to HandleOidStatusChange, since the oid status hasn't changed.
  EXPECT_OK(PollOids());

  EXPECT_CALL(onlp_, GetOidInfo(1234)).WillOnce(Return(OidInfo(fake_oid)));
  fake_oid.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(onlp_, GetOidInfo(1235)).WillOnce(Return(OidInfo(fake_oid)));
  // Only one oid status has changed, but we still get an update callback.
  EXPECT_CALL(callback2, HandleOidStatusChange(_))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(mock_update_callback, Call(::util::OkStatus()));
  EXPECT_OK(PollOids());
}

TEST_F(OnlpEventHandlerTest, BringupAndTeardownPollingThread) {
  EXPECT_OK(RunPolling());
}

TEST_F(OnlpEventHandlerTest, PollingThreadSendsMultipleCallbacks) {
  ASSERT_OK(RunPolling());
  absl::Mutex test_thread_lock;
  absl::CondVar test_thread_condvar;
  onlp_oid_hdr_t fake_oid;
  int callback_counter = 0;
  EXPECT_CALL(onlp_, GetOidInfo(1234))
      .WillRepeatedly(Invoke([&](OnlpOid oid) -> ::util::StatusOr<OidInfo> {
        absl::MutexLock lock(&test_thread_lock);
        return OidInfo(fake_oid);
      }));

  CallbackMock callback(1234);
  EXPECT_CALL(callback, HandleOidStatusChange(_))
      .WillRepeatedly(Invoke([&](OidInfo info) -> ::util::Status {
        absl::MutexLock lock(&test_thread_lock);
        test_thread_condvar.SignalAll();
        callback_counter++;
        return ::util::OkStatus();
      }));
  {
    absl::MutexLock lock(&test_thread_lock);
    fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  }
  // We must release the lock when registering our callback to avoid deadlock.
  ASSERT_OK(handler_.RegisterEventCallback(&callback));
  {
    // Wait for an initial callback to occur.
    absl::MutexLock lock(&test_thread_lock);
    while (callback_counter == 0)
      ASSERT_FALSE(test_thread_condvar.WaitWithTimeout(&test_thread_lock,
                                                       absl::Seconds(10)));
    // We should get more callbacks as the oid status changes.
    fake_oid.status = ONLP_OID_STATUS_FLAG_PRESENT;
    while (callback_counter == 1)
      ASSERT_FALSE(test_thread_condvar.WaitWithTimeout(&test_thread_lock,
                                                       absl::Seconds(10)));
    fake_oid.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
    while (callback_counter == 2)
      ASSERT_FALSE(test_thread_condvar.WaitWithTimeout(&test_thread_lock,
                                                       absl::Seconds(10)));
  }
  ASSERT_OK(handler_.UnregisterEventCallback(&callback));
  EXPECT_EQ(callback_counter, 3);
}
}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
