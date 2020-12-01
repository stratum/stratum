// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/lib/utils.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtPacketioManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_packetio_manager_ = BfrtPacketioManager::CreateInstance(
        kDevice1, bf_sde_wrapper_mock_.get());
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtPacketioManager> bfrt_packetio_manager_;
};

constexpr int BfrtPacketioManagerTest::kDevice1;

TEST_F(BfrtPacketioManagerTest, TransmitPacketAfterChassisConfigPush) {
  // TODO
  EXPECT_TRUE(false);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum