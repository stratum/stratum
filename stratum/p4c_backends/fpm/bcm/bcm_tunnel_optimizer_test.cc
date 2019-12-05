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

// This file contains unit tests for the BcmTunnelOptimizer class.

#include "stratum/p4c_backends/fpm/bcm/bcm_tunnel_optimizer.h"

#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"
#include "gtest/gtest.h"
#include "stratum/hal/lib/p4/p4_table_map.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/p4_table_defs.pb.h"

namespace stratum {
namespace p4c_backends {

// The test fixture supports BcmTunnelOptimizer unit tests.
class BcmTunnelOptimizerTest : public testing::Test {
 public:
  BcmTunnelOptimizerTest()
      : test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {}

  // Initializes common encap/decap properties in descriptor.
  void SetUpCommonTunnelProperties(bool is_gre, bool ecn_copy, bool dscp_copy,
                                   bool ttl_copy,
                                   hal::P4ActionDescriptor* descriptor) {
    auto tunnel_properties = descriptor->mutable_tunnel_properties();
    tunnel_properties->set_is_gre_tunnel(is_gre);
    tunnel_properties->mutable_ecn_value()->set_copy(ecn_copy);
    tunnel_properties->mutable_dscp_value()->set_copy(dscp_copy);
    tunnel_properties->mutable_ttl_value()->set_copy(ttl_copy);
  }

  BcmTunnelOptimizer bcm_tunnel_optimizer_;
  hal::P4ActionDescriptor optimized_action_;

  // This test uses its own p4c context since it doesn't have the context
  // that IRTestHelperJson commonly provides to many backend unit tests.
  AutoCompileContext test_p4c_context_;
};

// Verifies that Optimize accepts the minimum set of valid tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestOptimizeValidTunnel) {
  hal::P4ActionDescriptor input_action;
  input_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  EXPECT_TRUE(bcm_tunnel_optimizer_.Optimize(input_action, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
}

// Verifies that Optimize removes encap header-to-header copy assignments.
TEST_F(BcmTunnelOptimizerTest, TestOptimizeEncapHeader) {
  hal::P4ActionDescriptor input_action;
  input_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  input_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  auto header_copy = input_action.add_assignments();
  header_copy->mutable_assigned_value()->set_source_header_name(
      "hdr.ipv4_base");
  header_copy->set_destination_field_name("hdr.inner.ipv4");
  auto field_assign = input_action.add_assignments();
  field_assign->mutable_assigned_value()->set_parameter_name("param1");
  field_assign->set_destination_field_name("field1");

  EXPECT_TRUE(bcm_tunnel_optimizer_.Optimize(input_action, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(optimized_action_.has_tunnel_properties());
  EXPECT_TRUE(ProtoEqual(input_action.tunnel_properties(),
                         optimized_action_.tunnel_properties()));
  ASSERT_EQ(1, optimized_action_.assignments_size());
  EXPECT_TRUE(ProtoEqual(*field_assign, optimized_action_.assignments(0)));
}

// Verifies that Optimize removes decap header-to-header copy assignments.
TEST_F(BcmTunnelOptimizerTest, TestOptimizeDecapHeader) {
  hal::P4ActionDescriptor input_action;
  input_action.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV6);
  input_action.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV4);
  auto header_copy = input_action.add_assignments();
  header_copy->mutable_assigned_value()->set_source_header_name(
      "hdr.inner.ipv4");
  header_copy->set_destination_field_name("hdr.ipv4_base");
  auto field_assign = input_action.add_assignments();
  field_assign->mutable_assigned_value()->set_parameter_name("param1");
  field_assign->set_destination_field_name("field1");

  EXPECT_TRUE(bcm_tunnel_optimizer_.Optimize(input_action, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(optimized_action_.has_tunnel_properties());
  EXPECT_TRUE(ProtoEqual(input_action.tunnel_properties(),
                         optimized_action_.tunnel_properties()));
  ASSERT_EQ(1, optimized_action_.assignments_size());
  EXPECT_TRUE(ProtoEqual(*field_assign, optimized_action_.assignments(0)));
}

// Verifies that MergeAndOptimize accepts the minimum set of valid
// tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeValidTunnel) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  EXPECT_TRUE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
}

// Verifies that MergeAndOptimize properly merges encap properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeEncapHeaders) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  EXPECT_TRUE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  const auto& tunnel_properties = optimized_action_.tunnel_properties();
  ASSERT_EQ(2, tunnel_properties.encap().encap_inner_headers_size());
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.encap().encap_inner_headers(0));
  EXPECT_EQ(P4_HEADER_IPV6, tunnel_properties.encap().encap_inner_headers(1));
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.encap().encap_outer_header());
}

// Verifies that MergeAndOptimize properly merges decap properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeDecapHeaders) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV6);
  EXPECT_TRUE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  const auto& tunnel_properties = optimized_action_.tunnel_properties();
  ASSERT_EQ(2, tunnel_properties.decap().decap_inner_headers_size());
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.decap().decap_inner_headers(0));
  EXPECT_EQ(P4_HEADER_IPV6, tunnel_properties.decap().decap_inner_headers(1));
}

// Verifies that MergeAndOptimize properly merges the same encap inner
// header from two actions.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeSameEncapInnerHeaders) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  EXPECT_TRUE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  const auto& tunnel_properties = optimized_action_.tunnel_properties();
  ASSERT_EQ(1, tunnel_properties.encap().encap_inner_headers_size());
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.encap().encap_inner_headers(0));
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.encap().encap_outer_header());
}

// Verifies that MergeAndOptimize properly merges the same decap inner
// header from two actions.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeSameDecapInnerHeaders) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV4);
  EXPECT_TRUE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_EQ(0, ::errorCount());
  const auto& tunnel_properties = optimized_action_.tunnel_properties();
  ASSERT_EQ(1, tunnel_properties.decap().decap_inner_headers_size());
  EXPECT_EQ(P4_HEADER_IPV4, tunnel_properties.decap().decap_inner_headers(0));
}

// Verifies that Optimize fails with no tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestOptimizeNoTunnelError) {
  hal::P4ActionDescriptor input_action;
  EXPECT_FALSE(
      bcm_tunnel_optimizer_.Optimize(input_action, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the first input action has no
// tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeInvalidTunnelAction1) {
  hal::P4ActionDescriptor input_action1;
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the second input action has no
// tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeInvalidTunnelAction2) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies Optimize failure when tunnel properties specify neither
// encap nor decap.
TEST_F(BcmTunnelOptimizerTest, TestOptimizeNoEncapDecap) {
  hal::P4ActionDescriptor input_action;
  input_action.mutable_tunnel_properties()->set_is_gre_tunnel(true);
  EXPECT_FALSE(
      bcm_tunnel_optimizer_.Optimize(input_action, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when one input encaps and the other
// input decaps.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeEncapDecapConflict) {
  hal::P4ActionDescriptor input_action1;
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  input_action2.mutable_tunnel_properties()
      ->mutable_decap()
      ->add_decap_inner_headers(P4_HEADER_IPV4);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the two actions have conflicting
// tunnel properties GRE flags.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeConflictGRE) {
  hal::P4ActionDescriptor no_gre_action;
  no_gre_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  SetUpCommonTunnelProperties(
      /*is_gre=*/false, false, false, false, &no_gre_action);
  hal::P4ActionDescriptor gre_action;
  gre_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  SetUpCommonTunnelProperties(
      /*is_gre=*/true, false, false, false, &gre_action);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(no_gre_action, gre_action,
                                                      &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the two actions have conflicting
// ECN treatment in tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeConflictECN) {
  hal::P4ActionDescriptor no_ecn_action;
  no_ecn_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  SetUpCommonTunnelProperties(false, /*ecn_copy=*/false, false, false,
                              &no_ecn_action);
  hal::P4ActionDescriptor copy_ecn_action;
  copy_ecn_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  SetUpCommonTunnelProperties(false, /*ecn_copy=*/true, false, false,
                              &copy_ecn_action);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      no_ecn_action, copy_ecn_action, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the two actions have conflicting
// DSCP treatment in tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeConflictDSCP) {
  hal::P4ActionDescriptor no_dscp_action;
  no_dscp_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  SetUpCommonTunnelProperties(false, false, /*dscp_copy=*/false, false,
                              &no_dscp_action);
  hal::P4ActionDescriptor copy_dscp_action;
  copy_dscp_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  SetUpCommonTunnelProperties(false, false, /*dscp_copy=*/true, false,
                              &copy_dscp_action);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      no_dscp_action, copy_dscp_action, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the two actions have conflicting
// TTL treatment in tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeConflictTTL) {
  hal::P4ActionDescriptor no_ttl_action;
  no_ttl_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  SetUpCommonTunnelProperties(false, false, false, /*ttl_copy=*/false,
                              &no_ttl_action);
  hal::P4ActionDescriptor copy_ttl_action;
  copy_ttl_action.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  SetUpCommonTunnelProperties(false, false, false, /*ttl_copy=*/true,
                              &copy_ttl_action);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      no_ttl_action, copy_ttl_action, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

// Verifies that MergeAndOptimize fails when the two encap actions have
// conflicting outer headers in tunnel properties.
TEST_F(BcmTunnelOptimizerTest, TestMergeAndOptimizeConflictOuterHeader) {
  hal::P4ActionDescriptor input_action1;
  SetUpCommonTunnelProperties(
      /*is_gre=*/true, false, false, false, &input_action1);
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV4);
  input_action1.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV4);
  hal::P4ActionDescriptor input_action2;
  SetUpCommonTunnelProperties(
      /*is_gre=*/true, false, false, false, &input_action2);
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->add_encap_inner_headers(P4_HEADER_IPV6);
  input_action2.mutable_tunnel_properties()
      ->mutable_encap()
      ->set_encap_outer_header(P4_HEADER_IPV6);
  EXPECT_FALSE(bcm_tunnel_optimizer_.MergeAndOptimize(
      input_action1, input_action2, &optimized_action_));
  EXPECT_NE(0, ::errorCount());
}

}  // namespace p4c_backends
}  // namespace stratum
