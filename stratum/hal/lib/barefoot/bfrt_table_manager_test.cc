// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_table_manager.h"

#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/lib/utils.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

// FIXME
DEFINE_string(bfrt_sde_config_dir, "/var/run/stratum/bfrt_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace hal {
namespace barefoot {

class BfrtTableManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_table_manager_ = BfrtTableManager::CreateInstance(
        OPERATION_MODE_STANDALONE, bf_sde_wrapper_mock_.get(), kDevice1);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtTableManager> bfrt_table_manager_;
};

constexpr int BfrtTableManagerTest::kDevice1;

TEST_F(BfrtTableManagerTest, WriteDirectCounterEntryTest) {
  constexpr int kP4TableId = 10;
  constexpr int kBfRtTableId = 20;
  constexpr int kBfrtPriority = 16777205;  // Inverted
  auto table_key_mock = absl::make_unique<TableKeyMock>();
  auto table_data_mock = absl::make_unique<TableDataMock>();
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*table_key_mock, SetPriority(kBfrtPriority))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*table_data_mock, SetOnlyCounterData(200, 100))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_sde_wrapper_mock_, GetBfRtId(kP4TableId))
      .WillOnce(Return(kBfRtTableId));
  // TODO(max): figure out how to expect the session mock here.
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              ModifyTableEntry(kDevice1, _, kBfRtTableId, table_key_mock.get(),
                               table_data_mock.get()))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_sde_wrapper_mock_, CreateTableKey(kBfRtTableId))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<BfSdeInterface::TableKeyInterface>>(
              std::move(table_key_mock)))));
  EXPECT_CALL(*bf_sde_wrapper_mock_, CreateTableData(kBfRtTableId, _))
      .WillOnce(Return(ByMove(
          ::util::StatusOr<std::unique_ptr<BfSdeInterface::TableDataInterface>>(
              std::move(table_data_mock)))));

  const std::string kDirectCounterEntryText = R"PROTO(
    table_entry {
      table_id: 10
      match {
        field_id: 1
        exact { value: "\000\001" }
      }
      match {
        field_id: 2
        exact { value: "\000" }
      }
      priority: 10
    }
    data {
      byte_count: 200
      packet_count: 100
    }
  )PROTO";
  ::p4::v1::DirectCounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kDirectCounterEntryText, &entry));

  EXPECT_OK(bfrt_table_manager_->WriteDirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
