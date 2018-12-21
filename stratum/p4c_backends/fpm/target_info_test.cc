// This file contains unit tests for the TargetInfo class.

#include "platforms/networking/hercules/p4c_backend/switch/target_info.h"

#include "platforms/networking/hercules/p4c_backend/switch/target_info_mock.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace google {
namespace hercules {
namespace p4c_backend {

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

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
