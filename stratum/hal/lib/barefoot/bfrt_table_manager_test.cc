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
#include "stratum/hal/lib/common/writer_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

// FIXME
DEFINE_string(bfrt_sde_config_dir, "/var/run/stratum/bfrt_config",
              "The dir used by the SDE to load the device configuration.");

namespace stratum {
namespace hal {
namespace barefoot {

using test_utils::EqualsProto;
using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Optional;
using ::testing::Return;
using ::testing::SetArgPointee;

class BfrtTableManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_table_manager_ = BfrtTableManager::CreateInstance(
        OPERATION_MODE_STANDALONE, bf_sde_wrapper_mock_.get(), kDevice1);
  }

  ::util::Status PushTestConfig() {
    const std::string kSamplePipelineText = R"PROTO(
      programs {
        name: "test pipeline config",
        p4info {
          pkg_info {
            arch: "tna"
          }
          tables {
            preamble {
              id: 33583783
              name: "Ingress.control.table1"
            }
            match_fields {
              id: 1
              name: "field1"
              bitwidth: 9
              match_type: EXACT
            }
            match_fields {
              id: 2
              name: "field2"
              bitwidth: 12
              match_type: TERNARY
            }
            action_refs {
              id: 16794911
            }
            const_default_action_id: 16836487
            direct_resource_ids: 318814845
            size: 1024
          }
          actions {
            preamble {
              id: 16794911
              name: "Ingress.control.action1"
            }
            params {
              id: 1
              name: "vlan_id"
              bitwidth: 12
            }
          }
          direct_counters {
            preamble {
              id: 318814845
              name: "Ingress.control.counter1"
            }
            spec {
              unit: BOTH
            }
            direct_table_id: 33583783
          }
          meters {
            preamble {
              id: 55555
              name: "Ingress.control.meter_bytes"
              alias: "meter_bytes"
            }
            spec {
              unit: BYTES
            }
            size: 500
          }
          meters {
            preamble {
              id: 55556
              name: "Ingress.control.meter_packets"
              alias: "meter_packets"
            }
            spec {
              unit: PACKETS
            }
            size: 500
          }
        }
      }
    )PROTO";
    BfrtDeviceConfig config;
    RETURN_IF_ERROR(ParseProtoFromString(kSamplePipelineText, &config));
    return bfrt_table_manager_->PushForwardingPipelineConfig(config);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtTableManager> bfrt_table_manager_;
};

constexpr int BfrtTableManagerTest::kDevice1;

TEST_F(BfrtTableManagerTest, WriteDirectCounterEntryTest) {
  ASSERT_OK(PushTestConfig());
  constexpr int kP4TableId = 33583783;
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
      table_id: 33583783
      match {
        field_id: 1
        exact { value: "\000\001" }
      }
      match {
        field_id: 2
        ternary { value: "\x000" mask: "\xfff" }
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

TEST_F(BfrtTableManagerTest, WriteIndirectMeterEntryTest) {
  ASSERT_OK(PushTestConfig());
  constexpr int kP4MeterId = 55555;
  constexpr int kBfRtTableId = 11111;
  constexpr int kMeterIndex = 12345;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_, GetBfRtId(kP4MeterId))
      .WillOnce(Return(kBfRtTableId));
  // TODO(max): figure out how to expect the session mock here.
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              WriteIndirectMeter(kDevice1, _, kBfRtTableId,
                                 Optional(kMeterIndex), false, 1, 100, 2, 200))
      .WillOnce(Return(::util::OkStatus()));

  const std::string kMeterEntryText = R"PROTO(
    meter_id: 55555
    index {
      index: 12345
    }
    config {
      cir: 1
      cburst: 100
      pir: 2
      pburst: 200
    }
  )PROTO";
  ::p4::v1::MeterEntry entry;
  ASSERT_OK(ParseProtoFromString(kMeterEntryText, &entry));

  EXPECT_OK(bfrt_table_manager_->WriteMeterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry));
}

TEST_F(BfrtTableManagerTest, RejectMeterEntryModifyWithoutMeterId) {
  ASSERT_OK(PushTestConfig());
  auto session_mock = std::make_shared<SessionMock>();

  const std::string kMeterEntryText = R"PROTO(
    meter_id: 0
    index {
      index: 12345
    }
    config {
      cir: 1
      cburst: 100
      pir: 2
      pburst: 200
    }
  )PROTO";
  ::p4::v1::MeterEntry entry;
  ASSERT_OK(ParseProtoFromString(kMeterEntryText, &entry));

  ::util::Status ret = bfrt_table_manager_->WriteMeterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(), HasSubstr("Missing meter id"));
}

TEST_F(BfrtTableManagerTest, RejectMeterEntryInsertDelete) {
  ASSERT_OK(PushTestConfig());
  auto session_mock = std::make_shared<SessionMock>();

  const std::string kMeterEntryText = R"PROTO(
    meter_id: 55555
    index {
      index: 12345
    }
    config {
      cir: 1
      cburst: 100
      pir: 2
      pburst: 200
    }
  )PROTO";
  ::p4::v1::MeterEntry entry;
  ASSERT_OK(ParseProtoFromString(kMeterEntryText, &entry));

  ::util::Status ret = bfrt_table_manager_->WriteMeterEntry(
      session_mock, ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());

  ret = bfrt_table_manager_->WriteMeterEntry(session_mock,
                                             ::p4::v1::Update::DELETE, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
}

TEST_F(BfrtTableManagerTest, ReadSingleIndirectMeterEntryTest) {
  ASSERT_OK(PushTestConfig());
  auto session_mock = std::make_shared<SessionMock>();
  constexpr int kP4MeterId = 55555;
  constexpr int kBfRtTableId = 11111;
  constexpr int kMeterIndex = 12345;
  WriterMock<::p4::v1::ReadResponse> writer_mock;

  {
    EXPECT_CALL(*bf_sde_wrapper_mock_, GetBfRtId(kP4MeterId))
        .WillOnce(Return(kBfRtTableId));

    std::vector<uint32> meter_indices = {kMeterIndex};
    std::vector<uint64> cirs = {1};
    std::vector<uint64> cbursts = {100};
    std::vector<uint64> pirs = {2};
    std::vector<uint64> pbursts = {200};
    std::vector<bool> in_pps = {true};
    EXPECT_CALL(*bf_sde_wrapper_mock_,
                ReadIndirectMeters(kDevice1, _, kBfRtTableId,
                                   Optional(kMeterIndex), _, _, _, _, _, _))
        .WillOnce(DoAll(SetArgPointee<4>(meter_indices), SetArgPointee<5>(cirs),
                        SetArgPointee<6>(cbursts), SetArgPointee<7>(pirs),
                        SetArgPointee<8>(pbursts), SetArgPointee<9>(in_pps),
                        Return(::util::OkStatus())));
    const std::string kMeterResponseText = R"PROTO(
      entities {
        meter_entry {
          meter_id: 55555
          index {
            index: 12345
          }
          config {
            cir: 1
            cburst: 100
            pir: 2
            pburst: 200
          }
        }
      }
    )PROTO";
    ::p4::v1::ReadResponse resp;
    ASSERT_OK(ParseProtoFromString(kMeterResponseText, &resp));
    EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));
  }

  const std::string kMeterEntryText = R"PROTO(
    meter_id: 55555
    index {
      index: 12345
    }
  )PROTO";
  ::p4::v1::MeterEntry entry;
  ASSERT_OK(ParseProtoFromString(kMeterEntryText, &entry));

  EXPECT_OK(
      bfrt_table_manager_->ReadMeterEntry(session_mock, entry, &writer_mock));
}

TEST_F(BfrtTableManagerTest, RejectMeterEntryReadWithoutId) {
  ASSERT_OK(PushTestConfig());
  auto session_mock = std::make_shared<SessionMock>();
  WriterMock<::p4::v1::ReadResponse> writer_mock;

  const std::string kMeterEntryText = R"PROTO(
    meter_id: 0
    index {
      index: 12345
    }
    config {
      cir: 1
      cburst: 100
      pir: 2
      pburst: 200
    }
  )PROTO";
  ::p4::v1::MeterEntry entry;
  ASSERT_OK(ParseProtoFromString(kMeterEntryText, &entry));

  ::util::Status ret =
      bfrt_table_manager_->ReadMeterEntry(session_mock, entry, &writer_mock);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
