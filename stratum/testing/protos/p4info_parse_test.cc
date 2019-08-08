// Copyright 2018 Google LLC
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

// This file contains tests to make sure the p4info files in this directory can
// be parsed and interpreted as valid P4Info.  It also checks for consistency
// between P4Info and P4PipelineConfig data.

#include <memory>
#include <string>

#include "gflags/gflags.h"
#include "google/protobuf/util/message_differencer.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/p4/p4_info_manager.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "p4/v1/p4runtime.pb.h"

// P4ConfigVerifier flags to override in test environment.
DECLARE_string(match_field_error_level);
DECLARE_string(action_field_error_level);

namespace stratum {

namespace hal {

// The test parameter is a tuple of P4Info/P4PipelineConfig files to
// be verified:
//  First element: P4 info text file.
//  Second element: P4 pipeline config text file.
//  Third element: P4 pipeline config binary file.
class P4InfoFilesTest
    : public testing::TestWithParam<std::tuple<
        std::string, std::string, std::string>> {
 protected:
  void SetUp() override {
    // All tests raise the P4ConfigVerifier error level to "warn" if it's
    // still "vlog".
    if (FLAGS_match_field_error_level == "vlog")
      FLAGS_match_field_error_level = "warn";
    if (FLAGS_action_field_error_level == "vlog")
      FLAGS_action_field_error_level = "warn";
  }

  std::string GetTestFilePath() {
    return "platforms/networking/stratum/testing/protos/";
  }

  // Tests read the P4Info file into this message.
  ::p4::config::v1::P4Info p4_info_;

  // Tests read the P4PipelineConfig text file into this message.
  P4PipelineConfig p4_pipeline_config_;

  // These objects validate p4_info_ and p4_pipeline_config_ contents from a
  // switch perspective.
  std::unique_ptr<P4InfoManager> p4_info_manager_;
  std::unique_ptr<P4TableMapper> p4_table_mapper_;
};

// Does two checks on the P4Info given by the file parameter:
// 1) Makes sure the file reads and parses correctly into p4_info_.
// 2) Verifies that P4InfoManager can successfully handle the file's p4_info_.
TEST_P(P4InfoFilesTest, TestP4InfoValidity) {
  const std::string p4_info_test_file =
      GetTestFilePath() + std::get<0>(GetParam());
  ASSERT_TRUE(ReadProtoFromTextFile(p4_info_test_file, &p4_info_).ok());
  p4_info_manager_ = absl::make_unique<P4InfoManager>(p4_info_);
  EXPECT_TRUE(p4_info_manager_->InitializeAndVerify().ok());
}

// Verifies P4PipelineConfig text file can be parsed.
TEST_P(P4InfoFilesTest, TestP4PipelineConfigValidity) {
  const std::string p4_pipeline_config_file =
      GetTestFilePath() + std::get<1>(GetParam());
  EXPECT_TRUE(
      ReadProtoFromTextFile(p4_pipeline_config_file, &p4_pipeline_config_)
          .ok());
}

// Verifies consistency between P4PipelineConfig and P4Info.
TEST_P(P4InfoFilesTest, TestP4PipelineSpecConsistency) {
  // The test sets up the P4Info first.
  const std::string p4_info_test_file =
      GetTestFilePath() + std::get<0>(GetParam());
  ASSERT_TRUE(ReadProtoFromTextFile(p4_info_test_file, &p4_info_).ok());

  // The P4PipelineConfig file needs to be read and serialized.
  const std::string p4_pipeline_config_file =
      GetTestFilePath() + std::get<1>(GetParam());
  ASSERT_TRUE(
      ReadProtoFromTextFile(p4_pipeline_config_file, &p4_pipeline_config_)
          .ok());
  std::string table_map_spec;
  ASSERT_TRUE(p4_pipeline_config_.SerializeToString(&table_map_spec));

  // The test uses forwarding pipeline config verify and push operations to
  // verify consistency.
  p4_table_mapper_ = P4TableMapper::CreateInstance();
  ::p4::v1::ForwardingPipelineConfig config;
  *config.mutable_p4info() = p4_info_;
  config.set_p4_device_config(table_map_spec);
  EXPECT_TRUE(p4_table_mapper_->VerifyForwardingPipelineConfig(config).ok());
  EXPECT_TRUE(p4_table_mapper_->PushForwardingPipelineConfig(config).ok());
}

// Verifies consistency of the static table entries in P4PipelineConfig.
TEST_P(P4InfoFilesTest, TestP4PipelineStaticEntries) {
  // The test sets up the P4Info first.
  const std::string p4_info_test_file =
      GetTestFilePath() + std::get<0>(GetParam());
  ASSERT_TRUE(ReadProtoFromTextFile(p4_info_test_file, &p4_info_).ok());

  // The P4PipelineConfig file needs to be read and serialized.
  const std::string p4_pipeline_config_file =
      GetTestFilePath() + std::get<1>(GetParam());
  ASSERT_TRUE(
      ReadProtoFromTextFile(p4_pipeline_config_file, &p4_pipeline_config_)
          .ok());
  std::string table_map_spec;
  ASSERT_TRUE(p4_pipeline_config_.SerializeToString(&table_map_spec));

  // After pushing the forwarding pipeline config, the static entries can be
  // verified according to whether p4_table_mapper_->MapFlowEntry succeeds.
  p4_table_mapper_ = P4TableMapper::CreateInstance();
  ::p4::v1::ForwardingPipelineConfig config;
  *config.mutable_p4info() = p4_info_;
  config.set_p4_device_config(table_map_spec);
  EXPECT_TRUE(p4_table_mapper_->PushForwardingPipelineConfig(config).ok());

  for (const auto& static_entry :
       p4_pipeline_config_.static_table_entries().updates()) {
    ASSERT_TRUE(static_entry.entity().has_table_entry());
    CommonFlowEntry flow_entry;
    p4_table_mapper_->EnableStaticTableUpdates();
    EXPECT_TRUE(p4_table_mapper_->MapFlowEntry(
        static_entry.entity().table_entry(), static_entry.type(), &flow_entry)
            .ok());
    p4_table_mapper_->DisableStaticTableUpdates();
    EXPECT_FALSE(p4_table_mapper_->MapFlowEntry(
        static_entry.entity().table_entry(), static_entry.type(), &flow_entry)
            .ok());
  }
}

// This test verifies that nothing in the P4 program breaks parser field
// mapping.  It makes sure that one field in every header type has a known
// field type.
TEST_P(P4InfoFilesTest, TestParserMappedFieldTypes) {
  const std::string p4_pipeline_config_file =
      GetTestFilePath() + std::get<1>(GetParam());
  SCOPED_TRACE(absl::StrCat("Testing file: ", p4_pipeline_config_file));
  ASSERT_TRUE(
      ReadProtoFromTextFile(p4_pipeline_config_file, &p4_pipeline_config_)
          .ok());

  const std::vector<std::string> test_fields{
      "hdr.arp.target_proto_addr",
      "hdr.ethernet.dst_addr",
      "hdr.gre.flags",
      "hdr.icmp.code",
      "hdr.inner.ipv4.dst_addr",
      "hdr.inner.ipv6.dst_addr",
      "hdr.ipv4_base.src_addr",
      "hdr.ipv6_base.src_addr",
      "hdr.tcp.dst_port",
      "hdr.udp.src_port",
      "hdr.vlan_tag[0].ether_type",
      "hdr.vlan_tag[1].vid",
  };
  for (const auto& field_name : test_fields) {
    SCOPED_TRACE(absl::StrCat("Testing field: ", field_name));
    const auto& iter = p4_pipeline_config_.table_map().find(field_name);
    if (iter == p4_pipeline_config_.table_map().end()) {
      ADD_FAILURE() << "Missing field key in pipeline config.";
      continue;
    }
    if (!iter->second.has_field_descriptor()) {
      ADD_FAILURE() << "Missing field descriptor in pipeline config.";
      continue;
    }
    EXPECT_NE(P4_FIELD_TYPE_ANNOTATED, iter->second.field_descriptor().type());
    EXPECT_NE(P4_FIELD_TYPE_UNKNOWN, iter->second.field_descriptor().type());
  }
}

// This test verifies that the text and binary version of the same P4 pipeline
// config are equivalent.
TEST_P(P4InfoFilesTest, TestPipelineConfigTextEqualsBinary) {
  const std::string p4_pipeline_text_file =
      GetTestFilePath() + std::get<1>(GetParam());
  ASSERT_TRUE(
      ReadProtoFromTextFile(p4_pipeline_text_file, &p4_pipeline_config_)
          .ok());
  const std::string p4_pipeline_bin_file =
      GetTestFilePath() + std::get<2>(GetParam());
  P4PipelineConfig p4_pipeline_config_bin;
  ASSERT_TRUE(
      ReadProtoFromBinFile(p4_pipeline_bin_file, &p4_pipeline_config_bin)
          .ok());
  google::protobuf::util::MessageDifferencer msg_differencer;
  EXPECT_TRUE(msg_differencer.Equivalent(
      p4_pipeline_config_, p4_pipeline_config_bin));
}

// P4InfoFilesTest expects the test files to be in this path:
// "platforms/networking/stratum/hal/config/"
INSTANTIATE_TEST_SUITE_P(
    P4InfoFiles, P4InfoFilesTest,
    ::testing::Values(
        std::make_tuple("test_p4_info_stratum_b4.pb.txt",
                        "test_p4_pipeline_config_stratum_b4.pb.txt",
                        "test_p4_pipeline_config_stratum_b4.pb.bin"),
        std::make_tuple("test_p4_info_stratum_fbr_s2.pb.txt",
                        "test_p4_pipeline_config_stratum_fbr_s2.pb.txt",
                        "test_p4_pipeline_config_stratum_fbr_s2.pb.bin"),
        std::make_tuple("test_p4_info_stratum_fbr_s3.pb.txt",
                        "test_p4_pipeline_config_stratum_fbr_s3.pb.txt",
                        "test_p4_pipeline_config_stratum_fbr_s3.pb.bin"),
        std::make_tuple("test_p4_info_stratum_middleblock.pb.txt",
                        "test_p4_pipeline_config_stratum_middleblock.pb.txt",
                        "test_p4_pipeline_config_stratum_middleblock.pb.bin"),
        std::make_tuple("test_p4_info_stratum_spine.pb.txt",
                        "test_p4_pipeline_config_stratum_spine.pb.txt",
                        "test_p4_pipeline_config_stratum_spine.pb.bin"),
        std::make_tuple("test_p4_info_stratum_tor.pb.txt",
                        "test_p4_pipeline_config_stratum_tor.pb.txt",
                        "test_p4_pipeline_config_stratum_tor.pb.bin"),
        // These files are auto-generated based on the Orion P4 files.
        std::make_tuple("stratum_tor_p4_info.pb.txt",
                        "stratum_tor_p4_pipeline.pb.txt",
                        "stratum_tor_p4_pipeline.pb.bin"),
        std::make_tuple("stratum_spine_p4_info.pb.txt",
                        "stratum_spine_p4_pipeline.pb.txt",
                        "stratum_spine_p4_pipeline.pb.bin"),
        std::make_tuple("stratum_middleblock_p4_info.pb.txt",
                        "stratum_middleblock_p4_pipeline.pb.txt",
                        "stratum_middleblock_p4_pipeline.pb.bin"),
        std::make_tuple("stratum_b4_p4_info.pb.txt",
                        "stratum_b4_p4_pipeline.pb.txt",
                        "stratum_b4_p4_pipeline.pb.bin"),
        std::make_tuple("stratum_fbr_s2_p4_info.pb.txt",
                        "stratum_fbr_s2_p4_pipeline.pb.txt",
                        "stratum_fbr_s2_p4_pipeline.pb.bin"),
        std::make_tuple("stratum_fbr_s3_p4_info.pb.txt",
                        "stratum_fbr_s3_p4_pipeline.pb.txt",
                        "stratum_fbr_s3_p4_pipeline.pb.bin")));

}  // namespace hal

}  // namespace stratum
