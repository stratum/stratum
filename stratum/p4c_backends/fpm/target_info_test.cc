// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains unit tests for the TargetInfo class.

#include "stratum/p4c_backends/fpm/target_info.h"

#include "stratum/p4c_backends/fpm/target_info_mock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

  static void TearDownTestCase() {
    delete target_info_mock_;
  }

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
