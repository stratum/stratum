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

// This file contains TestTargetInfo unit tests.

#include "stratum/p4c_backends/test/test_target_info.h"

#include "gtest/gtest.h"
#include "stratum/public/proto/p4_annotation.pb.h"

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
