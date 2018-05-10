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


#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_sim_test_fixture.h"
#include "sandblaze/p4lang/p4/p4runtime.pb.h"

namespace stratum {
namespace hal {
namespace bcm {

namespace {

constexpr char kTestP4InfoFile[] =
    "stratum/testing/protos/"
    "test_p4_info_hercules_tor.pb.txt";

constexpr char kTestP4PipelineConfigFile[] =
    "stratum/testing/protos/"
    "test_p4_pipeline_config_hercules_tor.pb.txt";

}  // namespace

class BcmAclProfileTest : public BcmSimTestFixture {
 protected:
  BcmAclProfileTest() {}
  ~BcmAclProfileTest() override {}

  void SetUp() override { BcmSimTestFixture::SetUp(); }
  void TearDown() override { BcmSimTestFixture::TearDown(); }
};

TEST_F(BcmAclProfileTest, VerifyAclProfileQualifiers) {
  ASSERT_EQ(1, chassis_config().nodes_size())
      << "We only support one node with ID " << kNodeId << ".";
  ASSERT_EQ(kNodeId, chassis_config().nodes(0).id())
      << "We only support one node with ID " << kNodeId << ".";

  // Construct FowardingPipelineConfig proto.
  ::p4::ForwardingPipelineConfig forwarding_pipeline_config;
  ASSERT_OK(ReadProtoFromTextFile(kTestP4InfoFile,
                                  forwarding_pipeline_config.mutable_p4info()))
      << "Failed to read p4 info config proto from file " << kTestP4InfoFile
      << ".";
  P4PipelineConfig p4_pipeline_config;
  ASSERT_OK(
      ReadProtoFromTextFile(kTestP4PipelineConfigFile, &p4_pipeline_config))
      << "Failed to read p4 pipeline config proto from file "
      << kTestP4PipelineConfigFile << ".";
  ASSERT_TRUE(p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config.mutable_p4_device_config()))
      << "Failed to serialize p4_pipeline_config to string.";
  // Push the FowardingPipelineConfig proto.
  ASSERT_OK(p4_table_mapper().PushForwardingPipelineConfig(
      forwarding_pipeline_config))
      << "P4 table mapper failed to push forwarding_pipeline_config: "
      << forwarding_pipeline_config.DebugString();
  ASSERT_OK(bcm_acl_manager().PushForwardingPipelineConfig(
      forwarding_pipeline_config))
      << "Bcm ACL manager failed to push forwarding_pipeline_config: "
      << forwarding_pipeline_config.DebugString();
  // TODO:
  // We may want to read the qualifiers back and verify the settings are
  // correct.
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
