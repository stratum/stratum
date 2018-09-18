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


#include "stratum/hal/lib/common/error_buffer.h"

#include <pthread.h>
#include <memory>

#include "base/commandlineflags.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/public/lib/error.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

using ::testing::HasSubstr;

DECLARE_int32(max_num_errors_to_track);

namespace stratum {
namespace hal {

class ErrorBufferTest : public ::testing::Test {
 public:
  // Spawns a thread to emulate a case where another thread writes to
  // ErrorBuffer.
  void StartTestThread() {
    ASSERT_FALSE(pthread_create(&test_thread_id_, nullptr,
                                &ErrorBufferTest::TestThreadFunc, this));
  }

  // Main thread function.
  static void* TestThreadFunc(void* arg) {
    static_cast<ErrorBufferTest*>(arg)->TestThreadRun();
    return nullptr;
  }

  // The test thread run function. We write some errors to the ErrorBuffer
  // which is already being accessed by the main thread.
  void TestThreadRun() {
    for (int i = 0; i < FLAGS_max_num_errors_to_track / 2; ++i) {
      error_buffer_->AddError(
          ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg2),
          "Some additional info: ", GTL_LOC);
    }
  }

  // Wait for the thread to finish its job and exit.
  void WaitForTestThreadToDie() {
    ASSERT_FALSE(pthread_join(test_thread_id_, nullptr));
  }

 protected:
  void SetUp() override { error_buffer_ = absl::make_unique<ErrorBuffer>(); }

  static constexpr char kErrorMsg1[] = "Some error 1";
  static constexpr char kErrorMsg2[] = "Some error 2";
  static constexpr char kErrorMsg3[] = "Some error 3";

  std::unique_ptr<ErrorBuffer> error_buffer_;
  pthread_t test_thread_id_;
};

constexpr char ErrorBufferTest::kErrorMsg1[];
constexpr char ErrorBufferTest::kErrorMsg2[];
constexpr char ErrorBufferTest::kErrorMsg3[];

TEST_F(ErrorBufferTest, SingleThreadedCase) {
  // Add the errors.
  error_buffer_->AddError(
      ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg1), GTL_LOC);
  error_buffer_->AddError(
      ::util::Status(StratumErrorSpace(), ERR_UNKNOWN, kErrorMsg2),
      "Some additional info: ", GTL_LOC);
  error_buffer_->AddError(
      ::util::Status(StratumErrorSpace(), ERR_TABLE_FULL, kErrorMsg3),
      GTL_LOC);
  ASSERT_TRUE(error_buffer_->ErrorExists());

  // Get the errors back and verify.
  const auto& errors = error_buffer_->GetErrors();
  ASSERT_EQ(3U, errors.size());

  EXPECT_FALSE(errors[0].ok());
  EXPECT_EQ(ERR_INTERNAL, errors[0].error_code());
  EXPECT_THAT(errors[0].error_message(), HasSubstr(kErrorMsg1));

  EXPECT_FALSE(errors[1].ok());
  EXPECT_EQ(ERR_UNKNOWN, errors[1].error_code());
  EXPECT_THAT(errors[1].error_message(), HasSubstr(kErrorMsg2));
  EXPECT_THAT(errors[1].error_message(), HasSubstr("Some additional info: "));

  EXPECT_FALSE(errors[2].ok());
  EXPECT_EQ(ERR_TABLE_FULL, errors[2].error_code());
  EXPECT_THAT(errors[2].error_message(), HasSubstr(kErrorMsg3));

  // Clear the errors and verify.
  error_buffer_->ClearErrors();
  ASSERT_FALSE(error_buffer_->ErrorExists());
}

TEST_F(ErrorBufferTest, MultiThreadedCase) {
  // Bump up the flag to use a large number of errors.
  FLAGS_max_num_errors_to_track = 5000;

  // Spawn the thread and immediately try to write to ErrorBuffer. Need to
  // make sure all the errors are in the buffer after done. The thread is
  // going to add FLAGS_max_num_errors_to_track/2 errors and here we are
  // adding FLAGS_max_num_errors_to_track/2 more errors.
  StartTestThread();
  for (int i = 0; i < FLAGS_max_num_errors_to_track / 2; ++i) {
    error_buffer_->AddError(
        ::util::Status(StratumErrorSpace(), ERR_INTERNAL, kErrorMsg1),
        GTL_LOC);
  }
  WaitForTestThreadToDie();
  ASSERT_TRUE(error_buffer_->ErrorExists());

  // The order here is not important. We sort the list to make sure the result
  // is always the same. Then verify.
  auto errors = error_buffer_->GetErrors();
  std::sort(errors.begin(), errors.end(),
            [](const ::util::Status& x, const ::util::Status& y) {
              return x.error_code() < y.error_code();
            });
  ASSERT_EQ(FLAGS_max_num_errors_to_track, errors.size());

  for (int i = 0; i < FLAGS_max_num_errors_to_track / 2; ++i) {
    EXPECT_FALSE(errors[i].ok());
    EXPECT_EQ(ERR_UNKNOWN, errors[i].error_code());
    EXPECT_THAT(errors[i].error_message(), HasSubstr(kErrorMsg2));
    EXPECT_THAT(errors[i].error_message(), HasSubstr("Some additional info: "));
  }
  for (int i = FLAGS_max_num_errors_to_track / 2;
       i < FLAGS_max_num_errors_to_track; ++i) {
    EXPECT_FALSE(errors[i].ok());
    EXPECT_EQ(ERR_INTERNAL, errors[i].error_code());
    EXPECT_THAT(errors[i].error_message(), HasSubstr(kErrorMsg1));
  }
}

}  // namespace hal
}  // namespace stratum
