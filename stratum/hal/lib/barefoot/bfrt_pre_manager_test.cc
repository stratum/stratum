// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_pre_manager.h"

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

class BfrtPreManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_pre_manager_ =
        BfrtPreManager::CreateInstance(bf_sde_wrapper_mock_.get(), kDevice1);
  }

  static constexpr int kDevice1 = 0;

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtPreManager> bfrt_pre_manager_;
};

constexpr int BfrtPreManagerTest::kDevice1;

TEST_F(BfrtPreManagerTest, DeleteMulticastGroupTest) {
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

  // TODO(max): remove replicas, ignored on delete.
  const std::string kMulticastGroupEntryText = R"PROTO(
    multicast_group_entry {
      multicast_group_id: 55
      replicas {
        egress_port: 1
        instance: 0
      }
      replicas {
        egress_port: 2
        instance: 0
      }
      replicas {
        egress_port: 3
        instance: 0
      }
    }
  )PROTO";
  ::p4::v1::PacketReplicationEngineEntry entry;
  ASSERT_OK(ParseProtoFromString(kMulticastGroupEntryText, &entry));

  EXPECT_OK(bfrt_pre_manager_->WritePreEntry(session_mock,
                                             ::p4::v1::Update::DELETE, entry));
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
