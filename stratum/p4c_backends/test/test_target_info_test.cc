// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains TestTargetInfo unit tests.

#include "stratum/p4c_backends/test/test_target_info.h"

#include "stratum/public/proto/p4_annotation.pb.h"
#include "gtest/gtest.h"

namespace stratum {
namespace p4c_backends {

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

}  // namespace p4c_backends
}  // namespace stratum
