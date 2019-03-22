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

// Contains unit tests for HeaderPathInspector.

#include "stratum/p4c_backends/fpm/header_path_inspector.h"

#include <memory>
#include <string>
#include "stratum/p4c_backends/fpm/p4_model_names.pb.h"
#include "stratum/p4c_backends/fpm/utils.h"
#include "stratum/p4c_backends/test/ir_test_helpers.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/substitute.h"
#include "external/com_github_p4lang_p4c/ir/ir.h"

namespace stratum {
namespace p4c_backends {

// This test fixture depends on an IRTestHelperJson to generate a set of p4c IR
// data for test use.
class HeaderPathInspectorTest : public testing::Test {
 protected:
  void SetUp() override {
    SetUpTestP4ModelNames();
  }

  // The SetUpTestIR method loads an IR file in JSON format, then applies a
  // ProgramInspector to record IR nodes that contain some PathExpressions
  // to test.
  void SetUpTestIR(const std::string& ir_file) {
    ir_helper_ = absl::make_unique<IRTestHelperJson>();
    const std::string kTestP4File =
        "stratum/p4c_backends/fpm/testdata/" + ir_file;
    ASSERT_TRUE(ir_helper_->GenerateTestIRAndInspectProgram(kTestP4File));
  }

  // Tested HeaderPathInspector.
  std::unique_ptr<HeaderPathInspector> inspector_;

  std::unique_ptr<IRTestHelperJson> ir_helper_;  // Provides an IR for tests.

  // The merged_output_map_ collects output from successive calls to
  // HeaderPathInspector::Inspect.  Since a typical P4 program has multiple
  // IR::PathExpression nodes in an unspecified order, tests inspect each
  // expression, merge the output, and then verify that the merged output
  // from all Inspect calls has the expected entries.
  HeaderPathInspector::PathToHeaderTypeMap merged_output_map_;
};

// Tests the HeaderPathInspector for basic unnested, unstacked packet headers.
TEST_F(HeaderPathInspectorTest, TestBasicHeaders) {
  SetUpTestIR("header_decode_basic.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& enet_iter = merged_output_map_.find("hdr.ethernet");
  ASSERT_TRUE(enet_iter != merged_output_map_.end());
  EXPECT_EQ("ethernet_t", enet_iter->second);
  const auto& ipv4_iter = merged_output_map_.find("hdr.ipv4");
  ASSERT_TRUE(ipv4_iter != merged_output_map_.end());
  EXPECT_EQ("ipv4_t", ipv4_iter->second);
}

// Tests the same input as TestBasicHeaders, with the output qualified to
// ignore the "hdr" prefix as in P4_14 mode.
TEST_F(HeaderPathInspectorTest, TestIgnorePrefix) {
  SetUpTestIR("header_decode_basic.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  P4ModelNames p4_model_with_ignored_prefixes = GetP4ModelNames();
  (*p4_model_with_ignored_prefixes.mutable_strip_path_prefixes())["hdr"] = 0;
  SetP4ModelNames(p4_model_with_ignored_prefixes);
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& enet_iter = merged_output_map_.find("ethernet");
  ASSERT_TRUE(enet_iter != merged_output_map_.end());
  EXPECT_EQ("ethernet_t", enet_iter->second);
  const auto& ipv4_iter = merged_output_map_.find("ipv4");
  ASSERT_TRUE(ipv4_iter != merged_output_map_.end());
  EXPECT_EQ("ipv4_t", ipv4_iter->second);
}

// Tests HeaderPathInspector for multiple levels of nested metadata.
TEST_F(HeaderPathInspectorTest, TestNestedMetadata) {
  SetUpTestIR("nested_metadata_test.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& meta_iter = merged_output_map_.find("meta");
  ASSERT_TRUE(meta_iter != merged_output_map_.end());
  EXPECT_EQ("metadata_t", meta_iter->second);
  const auto& meta1_iter = merged_output_map_.find("meta.meta_outer.meta1");
  ASSERT_TRUE(meta1_iter != merged_output_map_.end());
  EXPECT_EQ("metadata1_t", meta1_iter->second);
  const auto& meta2_iter = merged_output_map_.find("meta.meta_outer.meta2");
  ASSERT_TRUE(meta2_iter != merged_output_map_.end());
  EXPECT_EQ("metadata2_t", meta2_iter->second);
  const auto& meta_outer = merged_output_map_.find("meta.meta_outer");
  ASSERT_TRUE(meta_outer != merged_output_map_.end());
  EXPECT_EQ("metadata_outer_t", meta_outer->second);
}

// Tests HeaderPathInspector with stacked headers.
TEST_F(HeaderPathInspectorTest, TestHeaderStack) {
  SetUpTestIR("header_decode_stack.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  constexpr int kHeaderStackSize = 4;
  for (int s = 0; s < kHeaderStackSize; ++s) {
    const std::string stack_path = absl::Substitute("hdr.stacked[$0]", s);
    const auto& stack_iter = merged_output_map_.find(stack_path);
    ASSERT_TRUE(stack_iter != merged_output_map_.end());
    EXPECT_EQ("stacked_header_t", stack_iter->second);
  }
  const auto& stack_iter = merged_output_map_.find("hdr.stacked.last");
  ASSERT_TRUE(stack_iter != merged_output_map_.end());
  EXPECT_EQ("stacked_header_t", stack_iter->second);
}

// Tests the HeaderPathInspector with two fields of the same type within the
// same header.
TEST_F(HeaderPathInspectorTest, TestHeaderTwoFieldsSameType) {
  SetUpTestIR("header_decode_advanced.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& ether_outer_iter = merged_output_map_.find("hdr.ether_outer");
  ASSERT_TRUE(ether_outer_iter != merged_output_map_.end());
  EXPECT_EQ("ethernet_t", ether_outer_iter->second);
  const auto& ether_inner_iter = merged_output_map_.find("hdr.ether_inner");
  ASSERT_TRUE(ether_inner_iter != merged_output_map_.end());
  EXPECT_EQ("ethernet_t", ether_inner_iter->second);
}

// Tests the HeaderPathInspector with a header union.
TEST_F(HeaderPathInspectorTest, TestHeaderUnion) {
  SetUpTestIR("header_decode_advanced.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& union_iter1 = merged_output_map_.find("hdr.proto_union.proto1");
  ASSERT_TRUE(union_iter1 != merged_output_map_.end());
  EXPECT_EQ("proto1_t", union_iter1->second);
  const auto& union_iter2 = merged_output_map_.find("hdr.proto_union.proto2");
  ASSERT_TRUE(union_iter2 != merged_output_map_.end());
  EXPECT_EQ("proto2_t", union_iter2->second);
}

// Tests the HeaderPathInspector with the same type inside and outside a union.
TEST_F(HeaderPathInspectorTest, TestHeaderTypeBeforeAndAfterUnion) {
  SetUpTestIR("header_decode_advanced.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  const auto& before_iter = merged_output_map_.find("hdr.proto_before_union");
  ASSERT_TRUE(before_iter != merged_output_map_.end());
  EXPECT_EQ("proto1_t", before_iter->second);
  const auto& after_iter = merged_output_map_.find("hdr.proto_after_union");
  ASSERT_TRUE(after_iter != merged_output_map_.end());
  EXPECT_EQ("proto2_t", after_iter->second);
}

// Tests the HeaderPathInspector with a header enum type.
TEST_F(HeaderPathInspectorTest, TestHeaderEnum) {
  SetUpTestIR("header_decode_advanced.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  ASSERT_NE(0, ir_helper_->program_inspector().p4_enums().size());
  for (auto path : ir_helper_->program_inspector().struct_paths()) {
    inspector_ = absl::make_unique<HeaderPathInspector>();
    EXPECT_TRUE(inspector_->Inspect(*path));
    EXPECT_FALSE(inspector_->path_to_header_type_map().empty());
    merged_output_map_.insert(inspector_->path_to_header_type_map().begin(),
                              inspector_->path_to_header_type_map().end());
  }

  // Since the enum_color field terminates the path, it doesn't appear in the
  // header path output map.  Only the enclosing "meta" header will be present.
  const auto& enum_iter1 = merged_output_map_.find("meta");
  ASSERT_TRUE(enum_iter1 != merged_output_map_.end());
  EXPECT_EQ("test_metadata_t", enum_iter1->second);
  EXPECT_TRUE(merged_output_map_.find("meta.enum_color") ==
              merged_output_map_.end());
}

// Tests for failure after calling HeaderPathInspector::Inspect a second time.
TEST_F(HeaderPathInspectorTest, TestFailMultipleInspects) {
  SetUpTestIR("header_decode_basic.ir.json");
  ASSERT_NE(0, ir_helper_->program_inspector().struct_paths().size());
  inspector_ = absl::make_unique<HeaderPathInspector>();
  EXPECT_TRUE(inspector_->Inspect(
      *(ir_helper_->program_inspector().struct_paths()[0])));
  EXPECT_FALSE(inspector_->Inspect(
      *(ir_helper_->program_inspector().struct_paths()[0])));
}

}  // namespace p4c_backends
}  // namespace stratum
