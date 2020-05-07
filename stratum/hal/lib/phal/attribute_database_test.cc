// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/attribute_database.h"

#include "google/protobuf/util/message_differencer.h"
#include "stratum/hal/lib/phal/attribute_group_mock.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/dummy_threadpool.h"
#include "stratum/hal/lib/phal/system_fake.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/channel/channel_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {

namespace hal {
namespace phal {

using test_utils::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::Return;
using ::testing::StrictMock;

class AttributeDatabaseTest : public ::testing::Test {
 public:
  void SetUp() override {
    auto mock_group =
        absl::make_unique<StrictMock<AttributeGroupMock>>(PhalDB::descriptor());
    auto dummy_threadpool = absl::make_unique<DummyThreadpool>();
    mock_group_ = mock_group.get();
    ASSERT_OK_AND_ASSIGN(
        database_, AttributeDatabase::Make(std::move(mock_group),
                                           std::move(dummy_threadpool), false));
  }

  absl::Time NextPollingTime() {
    absl::MutexLock lock(&database_->polling_lock_);
    return database_->GetNextPollingTime();
  }

  ::util::Status PollQueries() {
    absl::MutexLock lock(&database_->polling_lock_);
    return database_->PollQueries();
  }

  ::util::Status FlushQueries() {
    absl::MutexLock lock(&database_->polling_lock_);
    return database_->FlushQueries();
  }

 protected:
  std::unique_ptr<AttributeDatabase> database_;
  // Stores a pointer to the root attribute group mock used by database_.
  // Ownership of this group is managed by database_.
  AttributeGroupMock* mock_group_;
};

namespace {

std::vector<Path> GetTestPath() {
  return {{
      PathEntry("cards", 0),
      PathEntry("ports", 0),
      PathEntry("transceiver"),
      PathEntry("info"),
      PathEntry("state"),
  }};
}

TEST_F(AttributeDatabaseTest, CanCreateAndDelete) {
  // Exercise the desctructor.
  database_ = nullptr;
}

TEST_F(AttributeDatabaseTest, CanMakeAndDeleteQuery) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));
  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

TEST_F(AttributeDatabaseTest, CanGetFromQuery) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));
  EXPECT_CALL(*mock_group_, TraverseQuery(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_TRUE(query->Get().ok());
  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

TEST_F(AttributeDatabaseTest, QueryPolls) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));

  DatabaseQuery* db_query = reinterpret_cast<DatabaseQuery*>(query.get());
  EXPECT_CALL(*mock_group_, TraverseQuery(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());
  EXPECT_TRUE(db_query->Poll(absl::Now()).ok());
  EXPECT_TRUE(db_query->InternalQuery()->IsUpdated());
  // The query has already polled and found itself updated, so this invocation
  // of Poll does not call TraverseQuery again.
  EXPECT_TRUE(db_query->Poll(absl::Now()).ok());

  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

TEST_F(AttributeDatabaseTest, QuerySubscriptionUpdatesPollingTime) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));

  DatabaseQuery* db_query = reinterpret_cast<DatabaseQuery*>(query.get());
  std::unique_ptr<ChannelWriter<PhalDB>> writer =
      absl::make_unique<ChannelWriterMock<PhalDB>>();
  EXPECT_EQ(db_query->GetNextPollingTime(), absl::InfiniteFuture());
  EXPECT_EQ(NextPollingTime(), absl::InfiniteFuture());
  EXPECT_OK(db_query->Subscribe(std::move(writer), absl::Seconds(1)));
  EXPECT_LT(db_query->GetNextPollingTime(), absl::InfiniteFuture());
  EXPECT_LT(NextPollingTime(), absl::InfiniteFuture());

  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

TEST_F(AttributeDatabaseTest, QueryNotUpdatedBeforePollingInterval) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));

  DatabaseQuery* db_query = reinterpret_cast<DatabaseQuery*>(query.get());
  std::unique_ptr<ChannelWriter<PhalDB>> writer =
      absl::make_unique<ChannelWriterMock<PhalDB>>();
  EXPECT_OK(db_query->Subscribe(std::move(writer), absl::InfiniteDuration()));
  EXPECT_OK(db_query->Poll(absl::Now()));
  db_query->InternalQuery()->ClearUpdated();
  // db_query now has an up-to-date last polling time, but is not marked as
  // updated. Because its polling duration is infinite, it should not be marked
  // as updated if we poll all queries.
  EXPECT_OK(PollQueries());
  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());

  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

TEST_F(AttributeDatabaseTest, QueryFlushClearsUpdatedQueries) {
  EXPECT_CALL(*mock_group_, RegisterQuery(_, _))
      .WillOnce(Return(::util::OkStatus()));
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));

  DatabaseQuery* db_query = reinterpret_cast<DatabaseQuery*>(query.get());
  auto writer = absl::make_unique<ChannelWriterMock<PhalDB>>();
  ChannelWriterMock<PhalDB>* writer_ptr = writer.get();

  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());
  EXPECT_OK(db_query->Subscribe(std::move(writer), absl::Seconds(1)));
  EXPECT_TRUE(db_query->InternalQuery()->IsUpdated());

  // Flush the initial query response that happens on subscription.
  EXPECT_CALL(*mock_group_, TraverseQuery(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*writer_ptr, TryWrite(A<const PhalDB&>()))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(FlushQueries());
  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());

  // We flush again after a query is marked as updated.
  db_query->InternalQuery()->MarkUpdated();
  EXPECT_CALL(*mock_group_, TraverseQuery(_, _, _))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*writer_ptr, TryWrite(A<const PhalDB&>()))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_OK(FlushQueries());
  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());

  // No query is marked as updated, so this is a no-op.
  EXPECT_OK(FlushQueries());
  EXPECT_FALSE(db_query->InternalQuery()->IsUpdated());

  EXPECT_CALL(*mock_group_, UnregisterQuery(_)).WillOnce(Return());
  query = nullptr;
}

/* FIXME(boc) google only
// Run a few tests using an end-to-end attribute database with a fake system.
// These tests take a bit longer (~1 sec) because they are exercising all of the
// real threading in the attribute database.
class RealAttributeDatabaseTest : public ::testing::Test {
 public:
  ~RealAttributeDatabaseTest() override {
    // Delete the database before we delete our SystemFake.
    database_ = nullptr;
  }

  void SetUp() override {
    ASSERT_OK_AND_ASSIGN(database_,
                         AttributeDatabase::MakeGoogle(
                             "platforms/networking/stratum/hal/config/"
                             "legacy_phal_init_config_generic_trident2_stratum.pb.txt",
                             &system_));

    ASSERT_OK(ParseProtoFromString(
        "cards:{ports:{transceiver:{info:{state:HW_STATE_NOT_PRESENT}}}}",
        &hardware_not_present_));
    ASSERT_OK(ParseProtoFromString(
        "cards:{ports:{transceiver:{info:{state:HW_STATE_PRESENT}}}}",
        &hardware_present_));
  }

 protected:
  // Waits for a sane initial message to appear on the given query subscription.
  // If we subscribe to a query before the attribute database has configured
  // itself, we may receive a few uninteresting messages while the database is
  // populating. Returns once all of these messages have been received.
  ::util::Status WaitForSetup(ChannelReader<PhalDB>* reader) {
    PhalDB query_result;
    // Wait for the database to initialize HW_STATE_NOT_PRESENT and send a
    // message. We've subscribed early enough that the database might not be
    // done with its initial setup, so we discard intermediate results.
    while (query_result.cards_size() == 0 ||
           query_result.cards(0).ports_size() == 0 ||
           !query_result.cards(0).ports(0).has_transceiver() ||
           !query_result.cards(0).ports(0).transceiver().has_info()) {
      RETURN_IF_ERROR(reader->Read(&query_result, absl::Seconds(30)));
    }

    CHECK_RETURN_IF_FALSE(google::protobuf::util::MessageDifferencer::Equals(
        query_result, hardware_not_present_));
    return ::util::OkStatus();
  }

  std::unique_ptr<AttributeDatabase> database_;
  SystemFake system_;
  PhalDB hardware_present_;
  PhalDB hardware_not_present_;
};

TEST_F(RealAttributeDatabaseTest, CanSetupAndTeardownRealAttributeDatabase) {
}

TEST_F(RealAttributeDatabaseTest, StreamingQuerySendsUpdates) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));
  std::shared_ptr<Channel<PhalDB>> streaming_channel =
      Channel<PhalDB>::Create(5);
  auto reader = ChannelReader<PhalDB>::Create(streaming_channel);
  auto writer = ChannelWriter<PhalDB>::Create(streaming_channel);
  ASSERT_OK(query->Subscribe(std::move(writer), absl::Milliseconds(100)));

  PhalDB query_result;
  ASSERT_OK(WaitForSetup(reader.get()));

  system_.AddFakeFile("/sys/bus/i2c/devices/30201-0050/eeprom-nc", "");
  for (int i = 0; i < 10; i++) {
    // Test that we get a message for hardware insertion.
    system_.SendUdevUpdate("gfpga-removables",
                           "/sys/devices/gfpga-pci/pci_qsfp_1", i * 2 + 1,
                           "add", true);
    ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
    EXPECT_THAT(query_result, EqualsProto(hardware_present_));

    // Test that we get a message for hardware removal as well.
    system_.SendUdevUpdate("gfpga-removables",
                           "/sys/devices/gfpga-pci/pci_qsfp_1", i * 2 + 2,
                           "remove", true);
    ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
    EXPECT_THAT(query_result, EqualsProto(hardware_not_present_));
  }
}

TEST_F(RealAttributeDatabaseTest, OneQueryAllowsMultipleSubscriptions) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));
  std::shared_ptr<Channel<PhalDB>> streaming_channel =
      Channel<PhalDB>::Create(5);
  auto reader = ChannelReader<PhalDB>::Create(streaming_channel);
  auto writer = ChannelWriter<PhalDB>::Create(streaming_channel);
  ASSERT_OK(query->Subscribe(std::move(writer), absl::Milliseconds(100)));

  PhalDB query_result;
  ASSERT_OK(WaitForSetup(reader.get()));

  // Test that we get a message for hardware insertion.
  system_.AddFakeFile("/sys/bus/i2c/devices/30201-0050/eeprom-nc", "");
  system_.SendUdevUpdate("gfpga-removables",
                         "/sys/devices/gfpga-pci/pci_qsfp_1", 10, "add", true);
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_present_));

  std::shared_ptr<Channel<PhalDB>> streaming_channel_2 =
      Channel<PhalDB>::Create(5);
  auto reader_2 = ChannelReader<PhalDB>::Create(streaming_channel);
  auto writer_2 = ChannelWriter<PhalDB>::Create(streaming_channel);
  ASSERT_OK(query->Subscribe(std::move(writer_2), absl::Milliseconds(100)));

  // Both subscribers get a message when the new channel subscribes.
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_present_));
  ASSERT_OK(reader_2->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_present_));

  // Test that we get a message for hardware removal as well.
  system_.SendUdevUpdate("gfpga-removables",
                         "/sys/devices/gfpga-pci/pci_qsfp_1", 20, "remove",
                         true);
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_not_present_));
  ASSERT_OK(reader_2->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_not_present_));
}

TEST_F(RealAttributeDatabaseTest, CanUnsubscribeAndResubscribeToQuery) {
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Query> query,
                       database_->MakeQuery(GetTestPath()));
  std::shared_ptr<Channel<PhalDB>> streaming_channel =
      Channel<PhalDB>::Create(5);
  auto reader = ChannelReader<PhalDB>::Create(streaming_channel);
  auto writer = ChannelWriter<PhalDB>::Create(streaming_channel);
  ASSERT_OK(query->Subscribe(std::move(writer), absl::Milliseconds(100)));

  PhalDB query_result;
  ASSERT_OK(WaitForSetup(reader.get()));

  // Test that we get a message for hardware insertion.
  system_.AddFakeFile("/sys/bus/i2c/devices/30201-0050/eeprom-nc", "");
  system_.SendUdevUpdate("gfpga-removables",
                         "/sys/devices/gfpga-pci/pci_qsfp_1", 10, "add", true);
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_present_));

  // Delete the old channel (unsubscribe) and make a new one.
  streaming_channel = Channel<PhalDB>::Create(5);
  reader = ChannelReader<PhalDB>::Create(streaming_channel);
  writer = ChannelWriter<PhalDB>::Create(streaming_channel);
  ASSERT_OK(query->Subscribe(std::move(writer), absl::Milliseconds(100)));

  // We receive an initial message.
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_present_));

  // Test that we get a message for hardware removal as well.
  system_.SendUdevUpdate("gfpga-removables",
                         "/sys/devices/gfpga-pci/pci_qsfp_1", 20, "remove",
                         true);
  ASSERT_OK(reader->Read(&query_result, absl::Seconds(30)));
  EXPECT_THAT(query_result, EqualsProto(hardware_not_present_));
}
 */

}  // namespace
}  // namespace phal
}  // namespace hal

}  // namespace stratum
