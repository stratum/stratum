// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_counter_manager.h"

#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

class BfrtCounterManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_counter_manager_ = BfrtCounterManager::CreateInstance(
        bf_sde_wrapper_mock_.get(), kDevice1);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtCounterManager> bfrt_counter_manager_;
};

constexpr int BfrtCounterManagerTest::kDevice1;

TEST_F(BfrtCounterManagerTest, ModifyIndirectCounterTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_, GetBfRtId(kCounterId))
      .WillOnce(Return(kBfRtCounterId));

  const std::string kIndirectCounterEntryText = R"PROTO(
    counter_id: 55
    index {
      index: 100
    }
    data {
      byte_count: 100
      packet_count: 200
    }
  )PROTO";
  ::p4::v1::CounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kIndirectCounterEntryText, &entry));

  EXPECT_OK(bfrt_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
