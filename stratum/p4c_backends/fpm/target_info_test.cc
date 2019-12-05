// Copyright 2019 Google LLC
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

// This file contains unit tests for the TargetInfo class.

#include "stratum/p4c_backends/fpm/target_info.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/p4c_backends/fpm/target_info_mock.h"

namespace stratum {
namespace p4c_backends {

using ::testing::Return;

// The test fixture injects a mock instance into the singleton TargetInfo.
class TargetInfoTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    target_info_mock_ = new TargetInfoMock;
    TargetInfo::InjectSingleton(target_info_mock_);
  }

  static void TearDownTestCase() { delete target_info_mock_; }

  static TargetInfoMock* target_info_mock_;
};

TargetInfoMock* TargetInfoTest::target_info_mock_ = nullptr;

TEST_F(TargetInfoTest, TestGetSingleton) {
  TargetInfo* singleton = TargetInfo::GetSingleton();
  EXPECT_EQ(target_info_mock_, singleton);
}

TEST_F(TargetInfoTest, TestPipelineStageFixed) {
  const P4Annotation::PipelineStage kTestStage = P4Annotation::L3_LPM;
  EXPECT_CALL(*target_info_mock_, IsPipelineStageFixed(kTestStage))
      .WillOnce(Return(true));
  EXPECT_TRUE(TargetInfo::GetSingleton()->IsPipelineStageFixed(kTestStage));
}

}  // namespace p4c_backends
}  // namespace stratum
