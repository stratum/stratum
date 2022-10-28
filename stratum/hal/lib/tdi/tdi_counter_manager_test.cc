// Copyright 2020-present Open Networking Foundation
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// adapted from bfrt_counter_manager_test.cc

#include "stratum/hal/lib/tdi/tdi_counter_manager.h"

#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/tdi/tdi_sde_mock.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace tdi {

using ::testing::_;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

class TdiCounterManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tdi_sde_wrapper_mock_ = absl::make_unique<TdiSdeMock>();
    tdi_counter_manager_ = TdiCounterManager::CreateInstance(
        tdi_sde_wrapper_mock_.get(), kDevice1);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<TdiSdeMock> tdi_sde_wrapper_mock_;
  std::unique_ptr<TdiCounterManager> tdi_counter_manager_;
};

constexpr int TdiCounterManagerTest::kDevice1;

TEST_F(TdiCounterManagerTest, ModifyIndirectCounterTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*tdi_sde_wrapper_mock_, GetTdiRtId(kCounterId))
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

  EXPECT_OK(tdi_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry));
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
