// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains TunnelTypeMapper unit tests.

#include "stratum/p4c_backends/fpm/tunnel_type_mapper.h"

#include <string>
#include <tuple>

#include "stratum/hal/lib/p4/p4_pipeline_config.pb.h"
#include "stratum/lib/utils.h"
#include "stratum/p4c_backends/fpm/table_map_generator.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "gtest/gtest.h"
#include "external/com_github_p4lang_p4c/frontends/common/options.h"
#include "external/com_github_p4lang_p4c/lib/compile_context.h"

namespace stratum {
namespace p4c_backends {

// For parameterized tests, the first tuple member is the inner header type,
// the second tuple member is the outer header type, and the third parameter is
// true for GRE tests.  Thus, a tuple with:
//  <P4_HEADER_IPV4, P4_HEADER_IPV6, true>
// indicates a 4-in-6 test with a GRE wrapper.
class TunnelTypeMapperTest : public testing::TestWithParam<
    std::tuple<P4HeaderType, P4HeaderType, bool>> {
 protected:
  static constexpr const char* kTestAction = "test-tunnel-action";
  static constexpr const char* kTestAction2 = "test-tunnel-action-2";
  static constexpr const char* kTestDecapHeader1 = "test-decap-header-1";
  static constexpr const char* kTestDecapHeader2 = "test-decap-header-2";
  static constexpr const char* kTestEncapHeader = "test-encap-header";
  static constexpr const char* kTestEncapField = "test-encap-header.field";
  static constexpr const char* kTestOuterHeader = "test-outer-header";
  static constexpr const char* kTestOuterField = "test-outer-header.field";
  static constexpr const char* kTestNoTunnelHeader = "test-no-tunnel-header";
  static constexpr const char* kTestGreHeader = "test-gre-header";

  TunnelTypeMapperTest()
      : test_p4c_context_(new P4CContextWithOptions<CompilerOptions>) {
    expected_tunnel_properties_.mutable_ecn_value()->set_copy(true);
    expected_tunnel_properties_.mutable_dscp_value()->set_copy(true);
    expected_tunnel_properties_.mutable_ttl_value()->set_copy(true);
  }

  // Sets up a test P4 packet header for tunneling by creating a header
  // descriptor and a tunnel_actions entry in the action descriptor.
  void SetUpTestHeader(const std::string& header_name,
                       P4HeaderType header_type, int depth,
                       const std::string& action_name, P4HeaderOp header_op) {
    table_map_generator_.AddHeader(header_name);
    table_map_generator_.SetHeaderAttributes(header_name, header_type, depth);

    // This AddAction may be redundant if other test headers already use the
    // same action, but TableMapGenerator allows repeated AddAction calls.
    table_map_generator_.AddAction(action_name);
    hal::P4ActionDescriptor::P4TunnelAction tunnel_action;
    tunnel_action.set_header_op(header_op);
    tunnel_action.set_header_name(header_name);
    table_map_generator_.AddTunnelAction(action_name, tunnel_action);
  }

  // Adds an assignment of source_field_name to dest_field_name to the
  // descriptor for action_name along with field descriptors for the two fields.
  void SetUpTestFieldAssignment(const std::string& action_name,
                                const std::string& dest_field_name,
                                const std::string& source_field_name,
                                P4FieldType field_type,
                                P4HeaderType header_type_dest,
                                P4HeaderType header_type_source) {
    // As above, this AddAction may be redundant.
    table_map_generator_.AddAction(action_name);
    P4AssignSourceValue source_value;
    source_value.set_source_field_name(source_field_name);
    table_map_generator_.AssignActionSourceValueToField(
        action_name, source_value, dest_field_name);

    // Both fields in the assignment need field descriptors.
    table_map_generator_.AddField(dest_field_name);
    table_map_generator_.AddField(source_field_name);
    table_map_generator_.SetFieldAttributes(
        dest_field_name, field_type, header_type_dest, 0, 0);
    table_map_generator_.SetFieldAttributes(
        source_field_name, field_type, header_type_source, 0, 0);
  }

  // Test parameter accessors.
  P4HeaderType inner_header_type_param() const {
    return ::testing::get<0>(GetParam());
  }
  P4HeaderType outer_header_type_param() const {
    return ::testing::get<1>(GetParam());
  }
  bool test_gre_param() const {
    return ::testing::get<2>(GetParam());
  }

  // The typical test populates a P4PipelineConfig via the table_map_generator_,
  // then copies the generated_map to this mutable copy.
  hal::P4PipelineConfig test_p4_pipeline_config_;

  // The TableMapGenerator facilitates easy setup of P4PipelineConfig data.
  TableMapGenerator table_map_generator_;

  // Provides a convenient place for tests to setup tunnel type expectations.
  hal::P4ActionDescriptor::P4TunnelProperties expected_tunnel_properties_;

  // This test uses its own p4c context since it doesn't have the context
  // that IRTestHelperJson commonly provides to many backend unit tests.
  AutoCompileContext test_p4c_context_;
};

// Verifies tunnel processing of action descriptors with no tunnel_actions.
TEST_F(TunnelTypeMapperTest, TestNoTunnelActions) {
  table_map_generator_.AddAction(kTestAction);
  table_map_generator_.AddDropPrimitive(kTestAction);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Tests various encap types according to test parameter tuple.
TEST_P(TunnelTypeMapperTest, TestAllEncaps) {
  SetUpTestHeader(kTestEncapHeader, inner_header_type_param(), 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  if (test_gre_param()) {
    SetUpTestHeader(kTestGreHeader, P4_HEADER_GRE, 0,
                    kTestAction, P4_HEADER_SET_VALID);
  }
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(
      kTestOuterHeader, outer_header_type_param(), 0);
  P4FieldType field_type = P4_FIELD_TYPE_IPV4_DST;
  if (outer_header_type_param() == P4_HEADER_IPV6)
    field_type = P4_FIELD_TYPE_IPV6_DST;
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "dont-care-source",
                           field_type, outer_header_type_param(),
                           outer_header_type_param());
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_EQ(0, ::errorCount());
  expected_tunnel_properties_.mutable_encap()->add_encap_inner_headers(
      inner_header_type_param());
  expected_tunnel_properties_.mutable_encap()->set_encap_outer_header(
      outer_header_type_param());
  expected_tunnel_properties_.set_is_gre_tunnel(test_gre_param());
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         new_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, new_action_descriptor.tunnel_actions_size());
}

// Verifies tunnel processing of IP-in-non-GRE decap.
TEST_F(TunnelTypeMapperTest, TestIPNonGreDecap) {
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestDecapHeader2, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_EQ(0, ::errorCount());
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV4);
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV6);
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         new_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, new_action_descriptor.tunnel_actions_size());
}

// Verifies tunnel processing of IP-in-GRE decap.
TEST_F(TunnelTypeMapperTest, TestIPGreDecap) {
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestDecapHeader2, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_EQ(0, ::errorCount());
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV4);
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV6);
  expected_tunnel_properties_.set_is_gre_tunnel(true);
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         new_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, new_action_descriptor.tunnel_actions_size());
}

// Verifies no encap of header with zero-depth set to valid.
TEST_F(TunnelTypeMapperTest, TestNoEncapZeroDepth) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 0, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4PipelineConfig expected_pipeline_config = test_p4_pipeline_config_;

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // ProcessTunnels should remove the superfluous tunnel_actions without
  // making any other changes.
  EXPECT_EQ(0, ::errorCount());
  auto action_descriptor =
      FindMutableActionDescriptorOrDie(kTestAction, &expected_pipeline_config);
  action_descriptor->clear_tunnel_actions();
  EXPECT_TRUE(ProtoEqual(expected_pipeline_config, test_p4_pipeline_config_));
}

// Verifies no decap of header with zero-depth set to invalid.
TEST_F(TunnelTypeMapperTest, TestNoDecapZeroDepth) {
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV4, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4PipelineConfig expected_pipeline_config = test_p4_pipeline_config_;

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // ProcessTunnels should remove the superfluous tunnel_actions without
  // making any other changes.
  EXPECT_EQ(0, ::errorCount());
  auto action_descriptor =
      FindMutableActionDescriptorOrDie(kTestAction, &expected_pipeline_config);
  action_descriptor->clear_tunnel_actions();
  EXPECT_TRUE(ProtoEqual(expected_pipeline_config, test_p4_pipeline_config_));
}

// Verifies tunnels in multiple actions.
TEST_F(TunnelTypeMapperTest, TestMultipleTunnels) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_COPY_VALID);
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV4, 1,
      kTestAction2, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4PipelineConfig expected_pipeline_config = test_p4_pipeline_config_;

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_EQ(0, ::errorCount());
  expected_tunnel_properties_.mutable_encap()->add_encap_inner_headers(
      P4_HEADER_IPV4);
  const auto& first_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         first_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, first_action_descriptor.tunnel_actions_size());
  expected_tunnel_properties_.Clear();
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV4);
  expected_tunnel_properties_.mutable_ecn_value()->set_copy(true);
  expected_tunnel_properties_.mutable_dscp_value()->set_copy(true);
  expected_tunnel_properties_.mutable_ttl_value()->set_copy(true);
  const auto& second_action_descriptor =
      FindActionDescriptorOrDie(kTestAction2, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         second_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, second_action_descriptor.tunnel_actions_size());
}

// Verifies error when one action does both encap and decap.
TEST_F(TunnelTypeMapperTest, TestOneActionEncapAndDecap) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestDecapHeader2, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when one action updates multiple potential outer header types.
TEST_F(TunnelTypeMapperTest, TestOuterHeaderTypeConflict) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV4, 0);
  const char* kTestOuterHeader2 = "test-outer-header2";
  const char* kTestOuterField2 = "test-outer-header2.field";
  table_map_generator_.AddHeader(kTestOuterHeader2);
  table_map_generator_.SetHeaderAttributes(
      kTestOuterHeader2, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "dont-care-source",
                           P4_FIELD_TYPE_IPV4_DST, P4_HEADER_IPV4,
                           P4_HEADER_IPV4);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField2, "dont-care-source2",
                           P4_FIELD_TYPE_IPV6_DST, P4_HEADER_IPV6,
                           P4_HEADER_IPV6);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error for GRE tunnel without inner header encap.
TEST_F(TunnelTypeMapperTest, TestGreNoInnerEncap) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 0, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error for GRE tunnel without inner header decap.
TEST_F(TunnelTypeMapperTest, TestGreNoInnerDecap) {
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV6, 0, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when the GRE header is invalidated during tunnel encap.
TEST_F(TunnelTypeMapperTest, TestGreInvalidTunnelEncap) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when the GRE header is marked valid during tunnel decap.
TEST_F(TunnelTypeMapperTest, TestGreValidTunnelDecap) {
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when the GRE header is an inner header.
TEST_F(TunnelTypeMapperTest, TestGreInGre) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 1, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error for GRE header valid and invalid in one encap action.
TEST_F(TunnelTypeMapperTest, TestGreValidAndInvalidEncap) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error for GRE header valid and invalid in one decap action.
TEST_F(TunnelTypeMapperTest, TestGreValidAndInvalidDecap) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when GRE header is copied.
TEST_F(TunnelTypeMapperTest, TestGreCopy) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_VALID);
  SetUpTestHeader(
      kTestGreHeader, P4_HEADER_GRE, 0, kTestAction, P4_HEADER_COPY_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when an action tries to encap to multiple inner headers.
TEST_F(TunnelTypeMapperTest, TestEncapMultipleInnerHeaders) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_VALID);
  const std::string kEncapHeader2 = "test-encap-header-2";
  SetUpTestHeader(
      kEncapHeader2, P4_HEADER_IPV6, 1, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when an action attempts to encap an unsupported inner header.
TEST_F(TunnelTypeMapperTest, TestEncapUnsupportedHeaderType) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_TCP, 1, kTestAction, P4_HEADER_SET_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies error when an action attempts to decap an unsupported inner header.
TEST_F(TunnelTypeMapperTest, TestDecapUnsupportedHeaderType) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_UDP, 1, kTestAction, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(table_map_generator_.generated_map(),
                         test_p4_pipeline_config_));
}

// Verifies tunnels in multiple actions with error.
TEST_F(TunnelTypeMapperTest, TestMultipleTunnelsError) {
  // The kTestAction erroneously does both encap and decap in this setup.
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_COPY_VALID);
  SetUpTestHeader(
      kTestDecapHeader2, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_SET_INVALID);
  SetUpTestHeader(
      kTestDecapHeader1, P4_HEADER_IPV4, 1,
      kTestAction2, P4_HEADER_SET_INVALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4PipelineConfig expected_pipeline_config = test_p4_pipeline_config_;

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // The error should be reported, the failed first action should be
  // unchanged, and the second action should be updated normally.
  EXPECT_NE(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(
      FindActionDescriptorOrDie(kTestAction,
                                table_map_generator_.generated_map()),
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_)));
  expected_tunnel_properties_.mutable_decap()->add_decap_inner_headers(
      P4_HEADER_IPV4);
  const auto& second_action_descriptor =
      FindActionDescriptorOrDie(kTestAction2, test_p4_pipeline_config_);
  EXPECT_TRUE(ProtoEqual(expected_tunnel_properties_,
                         second_action_descriptor.tunnel_properties()));
  EXPECT_EQ(0, second_action_descriptor.tunnel_actions_size());
}

// Tests optimization of TTL assignment into tunnel properties.
TEST_F(TunnelTypeMapperTest, TestOptimizeTTLCopy) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "ttl-source-field",
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // The TTL copy should be optimized out of the P4ActionDescriptor assignments.
  EXPECT_EQ(0, ::errorCount());
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_EQ(0, new_action_descriptor.assignments_size());
  EXPECT_TRUE(new_action_descriptor.tunnel_properties().ttl_value().copy());
}

// Tests optimization of ECN assignment into tunnel properties.
TEST_F(TunnelTypeMapperTest, TestOptimizeECNCopy) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV6, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV4, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "ecn-source-field",
                           P4_FIELD_TYPE_ECN, P4_HEADER_IPV4, P4_HEADER_IPV6);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // The ECN copy should be optimized out of the P4ActionDescriptor assignments.
  EXPECT_EQ(0, ::errorCount());
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_EQ(0, new_action_descriptor.assignments_size());
  EXPECT_TRUE(new_action_descriptor.tunnel_properties().ttl_value().copy());
}

// Tests optimization of DSCP assignment into tunnel properties.
TEST_F(TunnelTypeMapperTest, TestOptimizeDCSPCopy) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "dscp-source-field",
                           P4_FIELD_TYPE_DSCP, P4_HEADER_IPV6, P4_HEADER_IPV4);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // The DSCP copy is optimized out of the P4ActionDescriptor assignments.
  EXPECT_EQ(0, ::errorCount());
  const hal::P4ActionDescriptor& new_action_descriptor =
      FindActionDescriptorOrDie(kTestAction, test_p4_pipeline_config_);
  EXPECT_EQ(0, new_action_descriptor.assignments_size());
  EXPECT_TRUE(new_action_descriptor.tunnel_properties().ttl_value().copy());
}

// Tests unsupported assignment of constant to TTL.
TEST_F(TunnelTypeMapperTest, TestUnsupportedAssignTTLConstant) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "ttl-source-field",
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  // This test converts the preceding assignment's source value to a constant.
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4ActionDescriptor* mutable_action = FindMutableActionDescriptorOrDie(
      kTestAction, &test_p4_pipeline_config_);
  ASSERT_EQ(1, mutable_action->assignments_size());
  auto source_value =
      mutable_action->mutable_assignments(0)->mutable_assigned_value();
  source_value->set_constant_param(1);

  auto saved_p4_pipeline_config = test_p4_pipeline_config_;
  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // A compiler error should be reported, and the pipeline config should be
  // unchanged.
  EXPECT_EQ(1, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_p4_pipeline_config, test_p4_pipeline_config_));
}

// Tests unsupported assignment of an action parameter to TTL.
TEST_F(TunnelTypeMapperTest, TestUnsupportedAssignTTLParam) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "ttl-source-field",
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  // This test converts the preceding assignment's source value to a parameter.
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4ActionDescriptor* mutable_action = FindMutableActionDescriptorOrDie(
      kTestAction, &test_p4_pipeline_config_);
  ASSERT_EQ(1, mutable_action->assignments_size());
  auto source_value =
      mutable_action->mutable_assignments(0)->mutable_assigned_value();
  source_value->set_parameter_name("ttl-param");

  auto saved_p4_pipeline_config = test_p4_pipeline_config_;
  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // A compiler error should be reported, and the pipeline config should be
  // unchanged.
  EXPECT_EQ(1, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_p4_pipeline_config, test_p4_pipeline_config_));
}

// Tests malformed TTL assignment error.
TEST_F(TunnelTypeMapperTest, TestMalformedTTLAssignError) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, "ttl-source-field",
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  // This test converts the preceding assignment's source value to a header,
  // which can't be assigned to a field.
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4ActionDescriptor* mutable_action = FindMutableActionDescriptorOrDie(
      kTestAction, &test_p4_pipeline_config_);
  ASSERT_EQ(1, mutable_action->assignments_size());
  auto source_value =
      mutable_action->mutable_assignments(0)->mutable_assigned_value();
  source_value->set_source_header_name(kTestEncapHeader);

  auto saved_p4_pipeline_config = test_p4_pipeline_config_;
  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // A compiler error should be reported, and the pipeline config should be
  // unchanged.
  EXPECT_EQ(1, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_p4_pipeline_config, test_p4_pipeline_config_));
}

// Tests TTL source field type error.
TEST_F(TunnelTypeMapperTest, TestTTLSourceFieldTypeError) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  const std::string kSourceFieldName = "ttl-source-field";
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, kSourceFieldName,
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  // This test converts the source field for the preceding assignment to a
  // non-TTL field type.
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4FieldDescriptor* mutable_field = FindMutableFieldDescriptorOrNull(
      kSourceFieldName, &test_p4_pipeline_config_);
  mutable_field->set_type(P4_FIELD_TYPE_DSCP);

  auto saved_p4_pipeline_config = test_p4_pipeline_config_;
  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // A compiler error should be reported, and the pipeline config should be
  // unchanged.
  EXPECT_EQ(1, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_p4_pipeline_config, test_p4_pipeline_config_));
}

// Tests TTL metadata source field error.
TEST_F(TunnelTypeMapperTest, TestTTLMetadataSourceFieldError) {
  SetUpTestHeader(kTestEncapHeader, P4_HEADER_IPV4, 1,
                  kTestAction, P4_HEADER_COPY_VALID);
  table_map_generator_.AddHeader(kTestOuterHeader);
  table_map_generator_.SetHeaderAttributes(kTestOuterHeader, P4_HEADER_IPV6, 0);
  const std::string kSourceFieldName = "ttl-source-field";
  SetUpTestFieldAssignment(kTestAction, kTestOuterField, kSourceFieldName,
                           P4_FIELD_TYPE_NW_TTL, P4_HEADER_IPV6,
                           P4_HEADER_IPV4);
  // This test converts the source field for the preceding assignment to a
  // local metadata field.
  test_p4_pipeline_config_ = table_map_generator_.generated_map();
  hal::P4FieldDescriptor* mutable_field = FindMutableFieldDescriptorOrNull(
      kSourceFieldName, &test_p4_pipeline_config_);
  mutable_field->set_is_local_metadata(true);

  auto saved_p4_pipeline_config = test_p4_pipeline_config_;
  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();

  // A compiler error should be reported, and the pipeline config should be
  // unchanged.
  EXPECT_EQ(1, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_p4_pipeline_config, test_p4_pipeline_config_));
}

// Verifies behavior when ProcessTunnels is called twice.
TEST_F(TunnelTypeMapperTest, TestIPv4ProcessTunnelsTwice) {
  SetUpTestHeader(
      kTestEncapHeader, P4_HEADER_IPV4, 1, kTestAction, P4_HEADER_COPY_VALID);
  test_p4_pipeline_config_ = table_map_generator_.generated_map();

  TunnelTypeMapper test_tunnel_mapper(&test_p4_pipeline_config_);
  test_tunnel_mapper.ProcessTunnels();
  ASSERT_EQ(0, ::errorCount());
  EXPECT_FALSE(ProtoEqual(table_map_generator_.generated_map(),
                          test_p4_pipeline_config_));
  hal::P4PipelineConfig saved_pipeline_config = test_p4_pipeline_config_;
  test_tunnel_mapper.ProcessTunnels();
  EXPECT_EQ(0, ::errorCount());
  EXPECT_TRUE(ProtoEqual(saved_pipeline_config, test_p4_pipeline_config_));
}

INSTANTIATE_TEST_SUITE_P(
  TestedEncaps,
  TunnelTypeMapperTest,
  ::testing::Values(
      std::make_tuple(P4_HEADER_IPV4, P4_HEADER_IPV4, false),
      std::make_tuple(P4_HEADER_IPV4, P4_HEADER_IPV4, true),
      std::make_tuple(P4_HEADER_IPV4, P4_HEADER_IPV6, false),
      std::make_tuple(P4_HEADER_IPV4, P4_HEADER_IPV6, true),
      std::make_tuple(P4_HEADER_IPV6, P4_HEADER_IPV4, false),
      std::make_tuple(P4_HEADER_IPV6, P4_HEADER_IPV4, true),
      std::make_tuple(P4_HEADER_IPV6, P4_HEADER_IPV6, false),
      std::make_tuple(P4_HEADER_IPV6, P4_HEADER_IPV6, true)));

}  // namespace p4c_backends
}  // namespace stratum
