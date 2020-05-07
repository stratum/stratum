// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

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
  EXPECT_FALSE(bcm_target_info_.IsPipelineStageFixed(
      P4Annotation::INGRESS_ACL));
  EXPECT_FALSE(bcm_target_info_.IsPipelineStageFixed(P4Annotation::EGRESS_ACL));
  EXPECT_FALSE(bcm_target_info_.IsPipelineStageFixed(
      P4Annotation::DEFAULT_STAGE));
}

}  // namespace p4c_backends
}  // namespace stratum
