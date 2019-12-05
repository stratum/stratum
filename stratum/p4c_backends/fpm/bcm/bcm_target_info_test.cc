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

// This file contains unit tests for the BcmTargetInfo class.

#include "stratum/p4c_backends/fpm/bcm/bcm_target_info.h"

#include "gtest/gtest.h"

namespace stratum {
namespace p4c_backends {

// The test fixture supports BcmTargetInfo unit tests.
class BcmTargetInfoTest : public testing::Test {
 public:
  BcmTargetInfo bcm_target_info_;
};

TEST_F(BcmTargetInfoTest, TestFixedPipelineStages) {
  EXPECT_TRUE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::L3_LPM));
  EXPECT_TRUE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::L2));
  EXPECT_TRUE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::DECAP));
  EXPECT_TRUE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::ENCAP));
}

TEST_F(BcmTargetInfoTest, TestNonFixedPipelineStages) {
  EXPECT_FALSE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::VLAN_ACL));
  EXPECT_FALSE(
      bcm_target_info_.IsPipelineStageFixed(P4Annotation::INGRESS_ACL));
  EXPECT_FALSE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::EGRESS_ACL));
  EXPECT_FALSE(
      bcm_target_info_.IsPipelineStageFixed(P4Annotation::DEFAULT_STAGE));
}

}  // namespace p4c_backends
}  // namespace stratum
