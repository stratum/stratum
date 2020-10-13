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

static constexpr char bf_config_tar_str[] = R"PROTO(
  p4_name: "my_prog"
  bfruntime_info: "{\"bfrt\": 1}"
  profiles {
    profile_name: "pipe"
    pipe_scope: 0
    pipe_scope: 1
    pipe_scope: 2
    pipe_scope: 3
    context: "{\"ctx\":2}"
    binary: "<bin data>"
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

TEST(ExtractBfPipelineTest, ExtractBfPipelineConfigFromTarGzip) {
  // embedded my_pipe.tgz
  //
  //   Generated using:
  //     tar --sort=name --owner=root:0 --group=root:0
  //         --mtime='UTC 2019-01-01' -c . |
  //     gzip -9 | xxd -ps -c 16 |
  //     sed -e 's/../\\x&/g' | sed -e 's/\(^\|$\)/"/g'
  //
  //   Contents of my_pipe.tgz:
  //     ./
  //     ./bfrt.json
  //     ./my_prog.conf
  //     ./p4info.txt
  //     ./pipe/
  //     ./pipe/context.json
  //     ./pipe/tofino.bin
  //   Contents of my_prog.conf:
  //     {
  //       "p4_devices": [{
  //         "p4_programs": [{
  //             "p4_pipelines": [{
  //                 "context": "/tmp/pipe/context.json",
  //                 "pipe_scope": [
  //                     0,
  //                     1,
  //                     2,
  //                     3
  //                 ],
  //                 "p4_pipeline_name": "pipe",
  //                 "config": "/tmp/pipe/tofino.bin",
  //                 "path": "/tmp"
  //             }],
  //             "bfrt-config": "/tmp/bfrt.json",
  //             "program-name": "my_prog"
  //         }],
  //         "device-id": 1
  //       }]
  //     }
  static constexpr char my_pipe_tgz[] =
      "\x1f\x8b\x08\x00\x24\x2a\x7d\x5f\x02\x03\xed\x98\x4b\x6e\x83\x30"
      "\x10\x86\x59\x73\x0a\xe4\x75\x02\xd8\x3c\x22\x45\x55\x2f\x52\x45"
      "\xc8\xe1\x91\xba\x0a\x18\x81\x5b\xa5\x8a\xb8\x7b\x6d\x0a\x49\xa1"
      "\x0d\xa8\x8b\x90\x96\xcc\xb7\x01\x99\xb1\x35\xf6\x78\x7e\x8f\x31"
      "\x2d\xed\xea\xd8\x92\x95\xe7\xd5\x4f\x49\xff\x59\xbf\x63\xc7\xc5"
      "\xc4\x23\xbe\x5f\xb7\xaf\x56\xd8\xd6\x0c\x4f\x9b\x80\xd7\x52\xd0"
      "\xc2\x30\xb4\x82\x73\x31\x64\x37\xf6\xfd\x9f\x62\x5a\xdb\xa4\x10"
      "\xe6\x4b\xc9\xb3\xab\xc6\xdf\x77\xdd\xcb\xf1\xc7\x4e\x37\xfe\x18"
      "\x7b\x0e\xd1\x0c\x1b\xe2\x7f\x75\x8e\x48\xc5\x1f\xad\x0d\x5c\x69"
      "\xc0\xfd\x61\x5a\xe9\x7b\x90\x17\x7c\x67\x86\x3c\x4b\x6e\x92\xff"
      "\xd8\xf5\x70\x2f\xff\x09\x71\x5c\xc8\xff\x49\xf2\x5f\x37\x24\x28"
      "\x77\x83\x28\x7e\x63\x61\x5c\x4a\x29\x78\xaa\xdb\x14\xc7\xd3\x5b"
      "\x6b\xa5\xb6\x4a\x41\xd3\xae\xd9\xcf\xe6\x9d\x6e\x2c\x8f\xf7\x2c"
      "\x8b\x7f\xee\x37\xdc\xff\x34\x8e\xdc\xa1\x22\x3e\x28\xb1\x42\x96"
      "\x48\x73\x4b\x0d\x6a\x35\x8d\xf5\x09\x86\x16\xc3\x03\xa8\x0e\x41"
      "\x19\xf2\x3c\x1e\x74\xa3\xc5\x5e\x8c\x9a\xe0\x71\x13\x32\x6e\xe2"
      "\x0c\x5a\x6c\xc6\x66\x75\x5e\xde\x20\xa3\xa9\x9a\x5b\x3d\xd3\xb1"
      "\xd5\x50\x09\xcf\x76\xdd\xd5\x14\x3c\x61\x19\x37\xb7\x6c\x7c\x2d"
      "\xa9\x78\x6e\xfb\xa2\x8b\xa6\x95\xfe\x8b\x19\xd5\x47\xd1\xb2\xe7"
      "\xd6\xa9\x3c\xb9\xe0\x10\x6a\x76\xe4\xb2\x9d\x7a\xa3\x67\xdf\x7d"
      "\xea\xfa\xd2\xf3\x01\x7d\x6e\xff\x25\x8b\xd4\x59\xa8\x77\xfb\x6c"
      "\xf4\x4a\x9f\xad\xfe\xe7\x2e\xcb\x12\x6e\x8a\xc3\xd5\xe4\x6d\xac"
      "\xfe\x93\x72\xdf\xaf\xff\x7c\xd7\x01\xfd\x9f\x02\x1a\x0a\xc6\xb3"
      "\xb2\x96\xde\xbc\x88\x69\xba\xdd\xc7\x8d\x0e\xb3\x68\xdd\x28\x93"
      "\x4a\xad\x26\xb3\xa4\x48\x98\x51\xc1\x9b\x94\xa7\x7b\x46\x4b\xf9"
      "\xa5\x6d\xa9\xe6\x9b\x28\xf3\xcd\x7f\xa5\xfb\x7f\xeb\xfe\x8f\xa5"
      "\x5c\x78\x70\xff\x9f\x30\xfe\x5f\xab\xa8\x1b\xdc\xff\xfb\xf5\xbf"
      "\x43\x30\xd4\xff\x13\xdd\xff\x43\x71\x40\x6b\x02\xb7\xff\xbb\xd6"
      "\xff\x73\xdd\x7f\x93\xff\x7f\xfd\xfa\x8f\xf8\xc4\x87\xfc\x9f\x82"
      "\x07\x19\x72\x23\xa2\x82\x3e\x42\x2e\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\xc0\x5c\xf9\x00\xbe\x32\xc5\x34\x00\x28\x00\x00";

  BfPipelineConfig bf_config;
  ASSERT_OK(ParseProtoFromString(bf_config_tar_str, &bf_config));
  ::p4::v1::ForwardingPipelineConfig p4_config;
  p4_config.set_p4_device_config(std::string(my_pipe_tgz, sizeof(my_pipe_tgz)));

  // Reverts FLAGS_incompatible_enable_p4_device_config_tar to default.
  ::gflags::FlagSaver flag_saver_;

  BfPipelineConfig extracted_bf_config;
  FLAGS_incompatible_enable_p4_device_config_tar = false;
  EXPECT_FALSE(ExtractBfPipelineConfig(p4_config, &extracted_bf_config).ok());

  FLAGS_incompatible_enable_p4_device_config_tar = true;
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

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
