// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// Unit tests for p4_utils.

#include "stratum/hal/lib/barefoot/bf_pipeline_utils.h"

// #include "absl/strings/substitute.h"
#include "absl/strings/escaping.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "google/protobuf/text_format.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"

// #include "stratum/glue/gtl/map_util.h"
// #include "stratum/glue/status/status_test_util.h"
// #include "stratum/lib/test_utils/matchers.h"
// #include "stratum/lib/utils.h"
// #include "stratum/public/proto/error.pb.h"

// using ::testing::HasSubstr;

namespace stratum {
namespace hal {
namespace barefoot {

TEST(ExtractBfPipelineTest, FromProto) {
    BfPipelineConfig bf_config;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        R"pb(device: 1
                programs {
                    name: "prog1"
                    bfrt: "{json: true}"
                    pipelines {
                        name: "pipe1"
                        context: "{json: true}"
                        config: "<raw bin>"
                    }
                })pb", &bf_config));

    std::string bf_config_bytes;
    ASSERT_TRUE(bf_config.SerializeToString(&bf_config_bytes));

    ::p4::v1::ForwardingPipelineConfig p4_config;
    p4_config.set_p4_device_config(bf_config_bytes);

    BfPipelineConfig extracted_bf_config;
    EXPECT_OK(ExtractBfDeviceConfig(p4_config, &extracted_bf_config));

    EXPECT_TRUE(ProtoEqual(bf_config, extracted_bf_config));
}

TEST(ExtractBfPipelineTest, FromProtoMultiPipe) {
    BfPipelineConfig bf_config;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        R"pb(device: 1
             programs {
                 name: "prog1"
                 bfrt: "{json: true}"
                 pipelines {
                     name: "pipe1"
                     context: "{json: true}"
                     config: "<raw bin>"
                 }
                 pipelines {
                     name: "pipe2"
                     context: "{json: true}"
                     config: "<raw bin>"
                 }
                 })pb", &bf_config));

    std::string bf_config_bytes;
    ASSERT_TRUE(bf_config.SerializeToString(&bf_config_bytes));

    ::p4::v1::ForwardingPipelineConfig p4_config;
    p4_config.set_p4_device_config(bf_config_bytes);

    BfPipelineConfig extracted_bf_config;
    EXPECT_OK(ExtractBfDeviceConfig(p4_config, &extracted_bf_config));

    EXPECT_TRUE(ProtoEqual(bf_config, extracted_bf_config));
}


TEST(ExtractBfPipelineTest, FromTar) {
    //TODO
}

TEST(ExtractBfPipelineTest, RandomBytes) {
    ::p4::v1::ForwardingPipelineConfig p4_config;
    p4_config.set_p4_device_config("<random vendor blob>");

    BfPipelineConfig extracted_bf_config;
    EXPECT_FALSE(ExtractBfDeviceConfig(p4_config, &extracted_bf_config).ok());
}

TEST(BfPipelineConvertTest, ToLegacyBfPiFormat) {
    BfPipelineConfig bf_config;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        R"pb(device: 1
             programs {
                 name: "prog1"
                 bfrt: "{json: true}"
                 pipelines {
                     name: "pipe1"
                     context: "{json: true}"
                     config: "<raw bin>"
                 }
             })pb", &bf_config));

    std::string expected_config;
    absl::CUnescape(
        "\\x5\\0\\0\\0prog1\\x9\\0\\0\\0<raw bin>\\xc\\0\\0\\0{json: true}",
        &expected_config);
    LOG(INFO) << absl::CHexEscape(expected_config);

    std::string extracted_config;
    EXPECT_OK(BfPipelineConfigToPiConfig(bf_config, &extracted_config));

    LOG(INFO) << absl::CHexEscape(extracted_config);
    EXPECT_EQ(expected_config, extracted_config);
}

TEST(BfPipelineConvertTest, MultiPipeFail) {
    BfPipelineConfig bf_config;
    ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
        R"pb(device: 1
             programs {
                 name: "prog1"
                 bfrt: "{json: true}"
                 pipelines {
                     name: "pipe1"
                     context: "{json: true}"
                     config: "<raw bin>"
                }
                pipelines {
                     name: "pipe2"
                     context: "{json: true}"
                     config: "<raw bin>"
                }
             })pb", &bf_config));

    std::string extracted_config;
    EXPECT_FALSE(BfPipelineConfigToPiConfig(bf_config, &extracted_config).ok());
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
