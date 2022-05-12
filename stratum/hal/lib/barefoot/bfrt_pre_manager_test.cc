// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"

#include <string>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_sde_mock.h"
#include "stratum/hal/lib/barefoot/bfrt_p4runtime_translator_mock.h"
#include "stratum/hal/lib/common/writer_mock.h"
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
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;

class BfrtPreManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<StrictMock<BfSdeMock>>();
    bfrt_p4runtime_translator_mock_ =
        absl::make_unique<BfrtP4RuntimeTranslatorMock>();
    bfrt_pre_manager_ = BfrtPreManager::CreateInstance(
        bf_sde_wrapper_mock_.get(), bfrt_p4runtime_translator_mock_.get(),
        kDevice1);
  }

  static constexpr int kDevice1 = 0;

  // Strict mock to ensure we capture all SDE calls.
  std::unique_ptr<StrictMock<BfSdeMock>> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtP4RuntimeTranslatorMock> bfrt_p4runtime_translator_mock_;
  std::unique_ptr<BfrtPreManager> bfrt_pre_manager_;
};

constexpr int BfrtPreManagerTest::kDevice1;

TEST_F(BfrtPreManagerTest, PushForwardingPipelineConfigSuccess) {
  BfrtDeviceConfig config;

  EXPECT_OK(bfrt_pre_manager_->PushForwardingPipelineConfig(config));
}

TEST_F(BfrtPreManagerTest, InsertMulticastGroupSuccess) {
  const std::string kMulticastGroupEntryText = R"pb(
    multicast_group_entry {
      multicast_group_id: 55
      replicas {
        egress_port: 1
        instance: 789
      }
      replicas {
        egress_port: 2
        instance: 789
      }
      replicas {
        egress_port: 3
        instance: 654
      }
    }
  )pb";
  constexpr int kGroupId = 55;
  const std::vector<uint32> egress_ports_group1 = {1, 2};
  const std::vector<uint32> egress_ports_group2 = {3};
  const std::vector<uint32> instances = {789, 654};
  const std::vector<uint32> mc_node_ids = {9864, 1234};
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(
      *bf_sde_wrapper_mock_,
      CreateMulticastNode(kDevice1, _, instances[0], _,
                          UnorderedElementsAreArray(egress_ports_group1)))
      .WillOnce(Return(mc_node_ids[0]));
  EXPECT_CALL(
      *bf_sde_wrapper_mock_,
      CreateMulticastNode(kDevice1, _, instances[1], _,
                          UnorderedElementsAreArray(egress_ports_group2)))
      .WillOnce(Return(mc_node_ids[1]));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              InsertMulticastGroup(kDevice1, _, kGroupId,
                                   UnorderedElementsAreArray(mc_node_ids)))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupEntryText, &entry));
  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));
  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::INSERT, entry));
}

TEST_F(BfrtPreManagerTest, ModifyMulticastGroupSuccess) {
  const std::string kMulticastGroupEntryText = R"pb(
    multicast_group_entry {
      multicast_group_id: 55
      replicas {
        egress_port: 5
        instance: 432
      }
      replicas {
        egress_port: 6
        instance: 432
      }
      replicas {
        egress_port: 7
        instance: 987
      }
    }
  )pb";
  constexpr int kGroupId = 55;
  const std::vector<uint32> egress_ports_group1 = {5, 6};
  const std::vector<uint32> egress_ports_group2 = {7};
  const std::vector<uint32> instances = {432, 987};
  const std::vector<uint32> old_mc_node_ids = {1234, 9864};
  const std::vector<uint32> new_mc_node_ids = {6543, 3210};
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetNodesInMulticastGroup(kDevice1, _, kGroupId))
      .WillOnce(Return(old_mc_node_ids));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              DeleteMulticastNodes(kDevice1, _,
                                   UnorderedElementsAreArray(old_mc_node_ids)))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(
      *bf_sde_wrapper_mock_,
      CreateMulticastNode(kDevice1, _, instances[0], _,
                          UnorderedElementsAreArray(egress_ports_group1)))
      .WillOnce(Return(new_mc_node_ids[0]));
  EXPECT_CALL(
      *bf_sde_wrapper_mock_,
      CreateMulticastNode(kDevice1, _, instances[1], _,
                          UnorderedElementsAreArray(egress_ports_group2)))
      .WillOnce(Return(new_mc_node_ids[1]));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              ModifyMulticastGroup(kDevice1, _, kGroupId,
                                   UnorderedElementsAreArray(new_mc_node_ids)))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupEntryText, &entry));
  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));
  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::MODIFY, entry));
}

TEST_F(BfrtPreManagerTest, DeleteMulticastGroupSuccess) {
  const std::string kMulticastGroupEntryText = R"pb(
    multicast_group_entry {
      multicast_group_id: 55
    }
  )pb";
  constexpr int kGroupId = 55;
  const std::vector<uint32> nodes = {1, 2, 3};
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetNodesInMulticastGroup(kDevice1, _, kGroupId))
      .WillOnce(Return(nodes));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              DeleteMulticastGroup(kDevice1, _, kGroupId))
      .WillOnce(Return(::util::OkStatus()));
  EXPECT_CALL(*bf_sde_wrapper_mock_, DeleteMulticastNodes(kDevice1, _, nodes))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::DELETE, entry));
}

TEST_F(BfrtPreManagerTest, ReadMulticastGroupSuccess) {
  auto session_mock = std::make_shared<SessionMock>();
  WriterMock<::p4::v1::ReadResponse> writer_mock;
  const std::string kMulticastGroupRequestText = R"pb(
    multicast_group_entry {
      multicast_group_id: 55
    }
  )pb";
  const std::string kMulticastGroupResponseText = R"pb(
    entities {
      packet_replication_engine_entry {
        multicast_group_entry {
          multicast_group_id: 55
          replicas {
            instance: 111
            egress_port: 1
          }
          replicas {
            instance: 111
            egress_port: 2
          }
          replicas {
            instance: 222
            egress_port: 3
          }
          replicas {
            instance: 222
            egress_port: 4
          }
        }
      }
    }
  )pb";
  const int kMcNodeId1 = 100;
  const int kReplicationId1 = 111;
  const std::vector<uint32> kLagIds1 = {0, 0};
  const std::vector<uint32> kEgressPorts1 = {1, 2};

  const int kMcNodeId2 = 200;
  const int kReplicationId2 = 222;
  const std::vector<uint32> kLagIds2 = {0, 0};
  const std::vector<uint32> kEgressPorts2 = {3, 4};

  const int kMulticastGroupId = 55;
  std::vector<uint32> group_ids = {kMulticastGroupId};
  std::vector<std::vector<uint32>> mc_node_ids = {{kMcNodeId1, kMcNodeId2}};

  ::p4::v1::ReadResponse resp;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupResponseText, &resp));

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetMulticastGroups(kDevice1, _, kMulticastGroupId, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(group_ids),
                      SetArgPointee<4>(mc_node_ids),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetMulticastNode(kDevice1, _, kMcNodeId1, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<3>(kReplicationId1), SetArgPointee<4>(kLagIds1),
                SetArgPointee<5>(kEgressPorts1), Return(::util::OkStatus())));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetMulticastNode(kDevice1, _, kMcNodeId2, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<3>(kReplicationId2), SetArgPointee<4>(kLagIds2),
                SetArgPointee<5>(kEgressPorts2), Return(::util::OkStatus())));
  EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupRequestText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  const auto& resp_pre_entry =
      resp.entities(0).packet_replication_engine_entry();
  EXPECT_CALL(
      *bfrt_p4runtime_translator_mock_,
      TranslatePacketReplicationEngineEntry(EqualsProto(resp_pre_entry), false))
      .WillOnce(Return(::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
          resp_pre_entry)));

  EXPECT_OK(bfrt_pre_manager_->ReadPreEntry(session_mock, entry, &writer_mock));
}

TEST_F(BfrtPreManagerTest, ReadMulticastGroupWildcardSuccess) {
  auto session_mock = std::make_shared<SessionMock>();
  WriterMock<::p4::v1::ReadResponse> writer_mock;
  const std::string kMulticastGroupRequestText = R"pb(
    multicast_group_entry {
      multicast_group_id: 0
    }
  )pb";
  const std::string kMulticastGroupResponseText = R"pb(
    entities {
      packet_replication_engine_entry {
        multicast_group_entry {
          multicast_group_id: 55
          replicas {
            instance: 111
            egress_port: 1
          }
          replicas {
            instance: 111
            egress_port: 2
          }
        }
      }
    }
    entities {
      packet_replication_engine_entry {
        multicast_group_entry {
          multicast_group_id: 88
          replicas {
            instance: 222
            egress_port: 3
          }
          replicas {
            instance: 222
            egress_port: 4
          }
        }
      }
    }
  )pb";
  const int kMcNodeId1 = 100;
  const int kReplicationId1 = 111;
  const std::vector<uint32> kLagIds1 = {0, 0};
  const std::vector<uint32> kEgressPorts1 = {1, 2};

  const int kMcNodeId2 = 200;
  const int kReplicationId2 = 222;
  const std::vector<uint32> kLagIds2 = {0, 0};
  const std::vector<uint32> kEgressPorts2 = {3, 4};

  const int kMulticastGroupId1 = 55;
  const int kMulticastGroupId2 = 88;
  std::vector<uint32> group_ids = {kMulticastGroupId1, kMulticastGroupId2};
  std::vector<std::vector<uint32>> mc_node_ids = {{kMcNodeId1}, {kMcNodeId2}};

  ::p4::v1::ReadResponse resp;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupResponseText, &resp));

  EXPECT_CALL(*bf_sde_wrapper_mock_, GetMulticastGroups(kDevice1, _, 0, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(group_ids),
                      SetArgPointee<4>(mc_node_ids),
                      Return(::util::OkStatus())));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetMulticastNode(kDevice1, _, kMcNodeId1, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<3>(kReplicationId1), SetArgPointee<4>(kLagIds1),
                SetArgPointee<5>(kEgressPorts1), Return(::util::OkStatus())));
  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetMulticastNode(kDevice1, _, kMcNodeId2, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<3>(kReplicationId2), SetArgPointee<4>(kLagIds2),
                SetArgPointee<5>(kEgressPorts2), Return(::util::OkStatus())));
  EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupRequestText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  for (const auto& entity : resp.entities()) {
    const auto& resp_pre_entry = entity.packet_replication_engine_entry();
    EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
                TranslatePacketReplicationEngineEntry(
                    EqualsProto(resp_pre_entry), false))
        .WillOnce(
            Return(::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
                resp_pre_entry)));
  }

  EXPECT_OK(bfrt_pre_manager_->ReadPreEntry(session_mock, entry, &writer_mock));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionSuccess) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 123
        instance: 0
      }
      class_of_service: 2
      packet_length_bytes: 1500
    }
  )pb";
  constexpr uint32 kSessionId = 55;
  constexpr int kEgressPort = 123;
  constexpr int kCos = 2;
  constexpr int kPacketLength = 1500;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              InsertCloneSession(kDevice1, _, kSessionId, kEgressPort, 0, kCos,
                                 kPacketLength))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::INSERT, entry));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionInvalidSessionIdFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 1016
      replicas {
        egress_port: 123
      }
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(), HasSubstr("Invalid session id"));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionInvalidPacketLengthFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 123
      }
      packet_length_bytes: 65536
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Packet length exceeds maximum value"));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionMultipleReplicasFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 123
      }
      replicas {
        egress_port: 456
      }
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Multiple replicas are not supported"));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionInstanceSetFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 123
        instance: 1
      }
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Instances on Replicas are not supported"));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionInvalidEgressPortFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 0
      }
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(), HasSubstr("Invalid egress port"));
}

TEST_F(BfrtPreManagerTest, InsertCloneSessionInvalidCosFail) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 123
      }
      class_of_service: 9
    }
  )pb";
  auto session_mock = std::make_shared<SessionMock>();

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  auto ret = bfrt_pre_manager_->WritePreEntry(session_mock,
                                              ::p4::v1::Update::INSERT, entry);
  ASSERT_FALSE(ret.ok());
  EXPECT_EQ(ERR_INVALID_PARAM, ret.error_code());
  EXPECT_THAT(ret.error_message(),
              HasSubstr("Class of service must be smaller than 8"));
}

TEST_F(BfrtPreManagerTest, ModifyCloneSessionSuccess) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
      replicas {
        egress_port: 654
        instance: 0
      }
      class_of_service: 4
      packet_length_bytes: 510
    }
  )pb";
  constexpr uint32 kSessionId = 55;
  constexpr int kEgressPort = 654;
  constexpr int kCos = 4;
  constexpr int kPacketLength = 510;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              ModifyCloneSession(kDevice1, _, kSessionId, kEgressPort, 0, kCos,
                                 kPacketLength))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::MODIFY, entry));
}

TEST_F(BfrtPreManagerTest, DeleteCloneSessionSuccess) {
  const std::string kCloneSessionEntryText = R"pb(
    clone_session_entry {
      session_id: 55
    }
  )pb";
  constexpr uint32 kSessionId = 55;
  auto session_mock = std::make_shared<SessionMock>();

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              DeleteCloneSession(kDevice1, _, kSessionId))
      .WillOnce(Return(::util::OkStatus()));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::DELETE, entry));
}

TEST_F(BfrtPreManagerTest, ReadCloneSessionSuccess) {
  auto session_mock = std::make_shared<SessionMock>();
  WriterMock<::p4::v1::ReadResponse> writer_mock;
  const std::string kCloneSessionEntryRequestText = R"pb(
    clone_session_entry {
      session_id: 55
    }
  )pb";
  const std::string kCloneSessionEntryResponseText = R"pb(
    entities {
      packet_replication_engine_entry {
        clone_session_entry {
          session_id: 55
          replicas {
            egress_port: 123
            instance: 0
          }
          class_of_service: 2
          packet_length_bytes: 1500
        }
      }
    }
  )pb";
  const int kSessionId = 55;
  std::vector<uint32> session_ids = {kSessionId};
  std::vector<int> egress_ports = {123};
  std::vector<int> coss = {2};
  std::vector<int> max_pkt_lens = {1500};

  ::p4::v1::ReadResponse resp;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryResponseText, &resp));

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetCloneSessions(kDevice1, _, kSessionId, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(session_ids),
                      SetArgPointee<4>(egress_ports), SetArgPointee<5>(coss),
                      SetArgPointee<6>(max_pkt_lens),
                      Return(::util::OkStatus())));
  EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryRequestText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  const auto& resp_pre_entry =
      resp.entities(0).packet_replication_engine_entry();
  EXPECT_CALL(
      *bfrt_p4runtime_translator_mock_,
      TranslatePacketReplicationEngineEntry(EqualsProto(resp_pre_entry), false))
      .WillOnce(Return(::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
          resp_pre_entry)));

  EXPECT_OK(bfrt_pre_manager_->ReadPreEntry(session_mock, entry, &writer_mock));
}

TEST_F(BfrtPreManagerTest, ReadCloneSessionWildcardSuccess) {
  auto session_mock = std::make_shared<SessionMock>();
  WriterMock<::p4::v1::ReadResponse> writer_mock;
  const std::string kCloneSessionEntryRequestText = R"pb(
    clone_session_entry {
      session_id: 0
    }
  )pb";
  const std::string kCloneSessionEntryResponseText = R"pb(
    entities {
      packet_replication_engine_entry {
        clone_session_entry {
          session_id: 55
          replicas {
            egress_port: 123
            instance: 0
          }
          class_of_service: 2
          packet_length_bytes: 1500
        }
      }
    }
    entities {
      packet_replication_engine_entry {
        clone_session_entry {
          session_id: 88
          replicas {
            egress_port: 456
            instance: 0
          }
          class_of_service: 6
          packet_length_bytes: 510
        }
      }
    }
  )pb";
  const int kSessionId1 = 55;
  const int kSessionId2 = 88;
  std::vector<uint32> session_ids = {kSessionId1, kSessionId2};
  std::vector<int> egress_ports = {123, 456};
  std::vector<int> coss = {2, 6};
  std::vector<int> max_pkt_lens = {1500, 510};

  ::p4::v1::ReadResponse resp;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryResponseText, &resp));

  EXPECT_CALL(*bf_sde_wrapper_mock_,
              GetCloneSessions(kDevice1, _, 0, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(session_ids),
                      SetArgPointee<4>(egress_ports), SetArgPointee<5>(coss),
                      SetArgPointee<6>(max_pkt_lens),
                      Return(::util::OkStatus())));
  EXPECT_CALL(writer_mock, Write(EqualsProto(resp))).WillOnce(Return(true));

  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kCloneSessionEntryRequestText, &entry));

  EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
              TranslatePacketReplicationEngineEntry(EqualsProto(entry), true))
      .WillOnce(Return(
          ::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(entry)));

  for (const auto& entity : resp.entities()) {
    const auto& resp_pre_entry = entity.packet_replication_engine_entry();
    EXPECT_CALL(*bfrt_p4runtime_translator_mock_,
                TranslatePacketReplicationEngineEntry(
                    EqualsProto(resp_pre_entry), false))
        .WillOnce(
            Return(::util::StatusOr<::p4::v1::PacketReplicationEngineEntry>(
                resp_pre_entry)));
  }

  EXPECT_OK(bfrt_pre_manager_->ReadPreEntry(session_mock, entry, &writer_mock));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
