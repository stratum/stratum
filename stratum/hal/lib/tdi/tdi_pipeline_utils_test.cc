// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// adapted from bf_pipeline_utils_test.cc

// Unit tests for tdi_pipeline_utils.

#include "stratum/hal/lib/tdi/tdi_pipeline_utils.h"

#include "absl/strings/escaping.h"
#include "gflags/gflags.h"
#include "gtest/gtest.h"
#include "p4/v1/p4runtime.pb.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

static constexpr char bf_config_1pipe_str[] = R"PROTO(
  p4_name: "prog1"
  bfruntime_info: "{json: true}"
  profiles {
    profile_name: "pipe1"
    context: "{json: true}"
    binary: "<raw bin>"
  })PROTO";

static constexpr char bf_config_2pipe_str[] = R"PROTO(
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
  })PROTO";

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

TEST(BfPipelineConvertTest, BfPipelineConfigToLegacyBfPiConfigSuccess) {
  static constexpr char expected_bytes[] =
      "\x5\0\0\0prog1\x9\0\0\0<raw bin>\xc\0\0\0{json: true}";
  BfPipelineConfig bf_config;
  ASSERT_OK(ParseProtoFromString(bf_config_1pipe_str, &bf_config));
  std::string extracted_config;
  EXPECT_OK(BfPipelineConfigToPiConfig(bf_config, &extracted_config));
  // Convert to a string, excluding the implicit null terminator.
  std::string expected_config(expected_bytes, sizeof(expected_bytes) - 1);
  EXPECT_EQ(expected_config, extracted_config);
}

TEST(BfPipelineConvertTest, BfPipelineConfigToLegacyBfPiConfigMultiPipeFail) {
  BfPipelineConfig bf_config;
  ASSERT_OK(ParseProtoFromString(bf_config_2pipe_str, &bf_config));
  std::string extracted_config;
  EXPECT_FALSE(BfPipelineConfigToPiConfig(bf_config, &extracted_config).ok());
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
