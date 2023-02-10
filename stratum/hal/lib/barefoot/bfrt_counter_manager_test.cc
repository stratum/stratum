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
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using test_utils::EqualsProto;
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
    bfrt_p4runtime_translator_mock_ =
        absl::make_unique<BfrtP4RuntimeTranslatorMock>();
    bfrt_counter_manager_ = BfrtCounterManager::CreateInstance(
        bf_sde_wrapper_mock_.get(), bfrt_p4runtime_translator_mock_.get(),
        kDevice1);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtP4RuntimeTranslatorMock> bfrt_p4runtime_translator_mock_;
  std::unique_ptr<BfrtCounterManager> bfrt_counter_manager_;
};

constexpr int BfrtCounterManagerTest::kDevice1;

TEST_F(BfrtCounterManagerTest, ModifyIndirectCounterSuccessTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_, GetBfRtId(kCounterId))
      .WillOnce(Return(kBfRtCounterId));

  const std::string kIndirectCounterEntryText = R"pb(
    counter_id: 55
    index {
      index: 100
    }
    data {
      byte_count: 100
      packet_count: 200
    }
  )pb";
  ::p4::v1::CounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kIndirectCounterEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslateCounterEntry(EqualsProto(entry), true))
      .WillOnce(Return(::util::StatusOr<::p4::v1::CounterEntry>(entry)));
  EXPECT_OK(bfrt_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry));
}

TEST_F(BfrtCounterManagerTest, RejectIndirectCounterInsertTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  const std::string kIndirectCounterEntryText = R"pb(
    counter_id: 55
    index {
      index: 100
    }
    data {
      byte_count: 100
      packet_count: 200
    }
  )pb";
  ::p4::v1::CounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kIndirectCounterEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslateCounterEntry(EqualsProto(entry), true))
      .WillOnce(Return(::util::StatusOr<::p4::v1::CounterEntry>(entry)));
  ::util::Status ret = bfrt_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(), HasSubstr("Update type of CounterEntry"));
}

TEST_F(BfrtCounterManagerTest, RejectIndirectCounterNoIndexTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  const std::string kIndirectCounterEntryText = R"pb(
    counter_id: 55
    data {
      byte_count: 100
      packet_count: 200
    }
  )pb";
  ::p4::v1::CounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kIndirectCounterEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslateCounterEntry(EqualsProto(entry), true))
      .WillOnce(Return(::util::StatusOr<::p4::v1::CounterEntry>(entry)));
  ::util::Status ret = bfrt_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Modifying an indirect counter without counter index "
                        "is currently not"));
}

TEST_F(BfrtCounterManagerTest, RejectIndirectCounterIndexZeroTest) {
  constexpr int kCounterId = 55;
  constexpr int kBfRtCounterId = 66;
  constexpr int kIndex = 100;
  auto session_mock = std::make_shared<SessionMock>();

  const std::string kIndirectCounterEntryText = R"pb(
    counter_id: 55
    index {
      index: -1
    }
    data {
      byte_count: 100
      packet_count: 200
    }
  )pb";
  ::p4::v1::CounterEntry entry;
  ASSERT_OK(ParseProtoFromString(kIndirectCounterEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslateCounterEntry(EqualsProto(entry), true))
      .WillOnce(Return(::util::StatusOr<::p4::v1::CounterEntry>(entry)));
  ::util::Status ret = bfrt_counter_manager_->WriteIndirectCounterEntry(
      session_mock, ::p4::v1::Update::MODIFY, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Counter index must be greater than or equal to zero"));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
