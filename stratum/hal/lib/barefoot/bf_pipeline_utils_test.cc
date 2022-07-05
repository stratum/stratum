// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Unit tests for bf_pipeline_utils.

#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"

#include "absl/strings/escaping.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

static constexpr char bf_config_1pipe_str[] = R"pb(
  p4_name: "prog1"
  bfruntime_info: "{json: true}"
  profiles {
    profile_name: "pipe1"
    context: "{json: true}"
    binary: "<raw bin>"
  })pb";

static constexpr char bf_config_2pipe_str[] = R"pb(
  p4_name: "prog1"
  bfruntime_info: "{json: true}"
  profiles {
    profile_name: "pipe1"
    context: "{json: true}"
    binary: "<raw bin>"
  }
  profiles {
    profile_name: "pipe2"
    context: "{json: true}"
    binary: "<raw bin>"
  })pb";

TEST(ExtractBfPipelineTest, ExtractBfPipelineConfigFromProtoSuccess) {
  BfPipelineConfig bf_config;
  ASSERT_OK(ParseProtoFromString(bf_config_1pipe_str, &bf_config));
  ::p4::v1::ForwardingPipelineConfig p4_config;
  {
    std::string bf_config_bytes;
    ASSERT_TRUE(bf_config.SerializeToString(&bf_config_bytes));
    p4_config.set_p4_device_config(bf_config_bytes);
  }
  BfPipelineConfig extracted_bf_config;
  EXPECT_OK(ExtractBfPipelineConfig(p4_config, &extracted_bf_config));
  EXPECT_TRUE(ProtoEqual(bf_config, extracted_bf_config))
      << "Expected: " << bf_config.ShortDebugString()
      << ", got: " << extracted_bf_config.ShortDebugString();
}

TEST(ExtractBfPipelineTest, ExtractBfPipelineConfigFromProto2PipesSuccess) {
  BfPipelineConfig bf_config;
  ASSERT_OK(ParseProtoFromString(bf_config_2pipe_str, &bf_config));
  ::p4::v1::ForwardingPipelineConfig p4_config;
  {
    std::string bf_config_bytes;
    ASSERT_TRUE(bf_config.SerializeToString(&bf_config_bytes));
    p4_config.set_p4_device_config(bf_config_bytes);
  }
  BfPipelineConfig extracted_bf_config;
  EXPECT_OK(ExtractBfPipelineConfig(p4_config, &extracted_bf_config));
  EXPECT_TRUE(ProtoEqual(bf_config, extracted_bf_config))
      << "Expected: " << bf_config.ShortDebugString()
      << ", got: " << extracted_bf_config.ShortDebugString();
}

TEST(ExtractBfPipelineTest, ExtractBfPipelineConfigFromRandomBytesFail) {
  ::p4::v1::ForwardingPipelineConfig p4_config;
  p4_config.set_p4_device_config("<random vendor blob>");

  BfPipelineConfig extracted_bf_config;
  EXPECT_FALSE(ExtractBfPipelineConfig(p4_config, &extracted_bf_config).ok());
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
