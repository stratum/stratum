// This file contains TestTargetInfo unit tests.

#include "platforms/networking/hercules/p4c_backend/test/test_target_info.h"

#include "platforms/networking/hercules/public/proto/p4_annotation.host.pb.h"
#include "testing/base/public/gunit.h"

namespace google {
namespace hercules {
namespace p4c_backend {

TEST(TestTargetInfoTest, TestFixedStages) {
  TestTargetInfo::SetUpTestTargetInfo();
  TargetInfo* test_target = TargetInfo::GetSingleton();
  ASSERT_TRUE(test_target != nullptr);
  EXPECT_TRUE(test_target->IsPipelineStageFixed(P4Annotation::L2));
  EXPECT_TRUE(test_target->IsPipelineStageFixed(P4Annotation::L3_LPM));
  TestTargetInfo::TearDownTestTargetInfo();
}

TEST(TestTargetInfoTest, TestNonFixedStages) {
  TestTargetInfo::SetUpTestTargetInfo();
  TargetInfo* test_target = TargetInfo::GetSingleton();
  ASSERT_TRUE(test_target != nullptr);
  EXPECT_FALSE(test_target->IsPipelineStageFixed(P4Annotation::VLAN_ACL));
  EXPECT_FALSE(test_target->IsPipelineStageFixed(P4Annotation::INGRESS_ACL));
  EXPECT_FALSE(test_target->IsPipelineStageFixed(P4Annotation::EGRESS_ACL));
  TestTargetInfo::TearDownTestTargetInfo();
}

}  // namespace p4c_backend
}  // namespace hercules
}  // namespace google
