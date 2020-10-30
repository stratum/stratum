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

DECLARE_bool(incompatible_enable_p4_device_config_tar);

namespace stratum {
namespace hal {
namespace barefoot {

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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
