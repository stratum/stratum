// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

// BcmTunnelManager tests.

#include "stratum/hal/lib/bcm/bcm_tunnel_manager.h"

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_sdk_mock.h"
#include "stratum/hal/lib/bcm/bcm_table_manager_mock.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {

namespace hal {
namespace bcm {

class BcmTunnelManagerTest : public ::testing::Test {
 protected:
  static constexpr int kUnit = 2;

  void SetUp() override {
    bcm_sdk_mock_ = absl::make_unique<BcmSdkMock>();
    bcm_table_manager_mock_ = absl::make_unique<BcmTableManagerMock>();
    test_tunnel_manager_ = BcmTunnelManager::CreateInstance(
        bcm_sdk_mock_.get(), bcm_table_manager_mock_.get(), kUnit);
  }

  std::unique_ptr<BcmTunnelManager> test_tunnel_manager_;

  std::unique_ptr<BcmSdkMock> bcm_sdk_mock_;
  std::unique_ptr<BcmTableManagerMock> bcm_table_manager_mock_;
};

// TODO(teverman): All tests currently expect their respective stubs to
// return an OK status.
TEST_F(BcmTunnelManagerTest, TestPushChassisConfig) {
  ChassisConfig config;
  EXPECT_OK(test_tunnel_manager_->PushChassisConfig(config, 0));
}

TEST_F(BcmTunnelManagerTest, TestVerifyChassisConfig) {
  ChassisConfig config;
  EXPECT_OK(test_tunnel_manager_->VerifyChassisConfig(config, 0));
}

TEST_F(BcmTunnelManagerTest, TestPushForwardingPipelineConfig) {
  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_OK(test_tunnel_manager_->PushForwardingPipelineConfig(config));
}

TEST_F(BcmTunnelManagerTest, TestVerifyForwardingPipelineConfig) {
  ::p4::v1::ForwardingPipelineConfig config;
  EXPECT_OK(test_tunnel_manager_->VerifyForwardingPipelineConfig(config));
}

TEST_F(BcmTunnelManagerTest, TestShutdown) {
  EXPECT_OK(test_tunnel_manager_->Shutdown());
}

TEST_F(BcmTunnelManagerTest, TestInsertTableEntry) {
  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(test_tunnel_manager_->InsertTableEntry(table_entry));
}

TEST_F(BcmTunnelManagerTest, TestModifyTableEntry) {
  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(test_tunnel_manager_->ModifyTableEntry(table_entry));
}

TEST_F(BcmTunnelManagerTest, TestDeleteTableEntry) {
  ::p4::v1::TableEntry table_entry;
  EXPECT_OK(test_tunnel_manager_->DeleteTableEntry(table_entry));
}

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
