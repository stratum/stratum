/* Copyright 2019-present Barefoot Networks, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/barefoot/bf_pal_mock.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "absl/time/time.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace barefoot {

namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr uint64 kNodeId = 7654321ULL;
// For Tofino, unit is the 0-based index of the node in the ChassisConfig.
constexpr int kUnit = 0;
constexpr int kSlot = 1;
constexpr int kPort = 1;
constexpr uint32 kPortId = 12345;
constexpr uint64 kDefaultSpeedBps = 100000000000;  // 100Gbps

// A helper class to build a single-node ChassisConfig message.
class ChassisConfigBuilder {
 public:
  ChassisConfigBuilder(uint64 node_id = kNodeId)
      : node_id(node_id) {
    config_.set_description("Test config for BFChassisManager");
    auto* chassis = config_.mutable_chassis();
    chassis->set_platform(PLT_BAREFOOT_TOFINO);
    chassis->set_name("Tofino");

    auto* node = config_.add_nodes();
    node->set_id(node_id);
    node->set_slot(kSlot);
  }

  SingletonPort* AddPort(uint32 port_id, int32 port,
                         AdminState admin_state,
                         uint64 speed_bps = kDefaultSpeedBps) {
    auto* sport = config_.add_singleton_ports();
    sport->set_id(port_id);
    sport->set_node(node_id);
    sport->set_port(port);
    sport->set_slot(kSlot);
    sport->set_channel(0);
    sport->set_speed_bps(speed_bps);
    sport->mutable_config_params()->set_admin_state(admin_state);
    return sport;
  }

  SingletonPort* GetPort(uint32 port_id) {
    for (auto& sport : *config_.mutable_singleton_ports()) {
      if (sport.id() == port_id) return &sport;
    }
    return nullptr;
  }

  void RemoveLastPort() {
    config_.mutable_singleton_ports()->RemoveLast();
  }

  const ChassisConfig& Get() const { return config_; }

 private:
  uint64 node_id;
  ChassisConfig config_;
};

}  // namespace

class BFChassisManagerTest : public ::testing::Test {
 protected:
  BFChassisManagerTest() { }

  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    bf_pal_mock_ = absl::make_unique<BFPalMock>();
    bf_chassis_manager_ = BFChassisManager::CreateInstance(
        phal_mock_.get(), bf_pal_mock_.get());
  }

  ::util::Status CheckCleanInternalState() {
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->unit_to_node_id_.empty());
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->node_id_to_unit_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_port_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_port_config_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->node_id_to_port_id_to_singleton_port_key_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->xcvr_port_key_to_xcvr_state_.empty());
    CHECK_RETURN_IF_FALSE(
        bf_chassis_manager_->port_status_change_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_channel_ == nullptr);
    CHECK_RETURN_IF_FALSE(bf_chassis_manager_->xcvr_event_reader_ == nullptr);
    return ::util::OkStatus();
  }

  bool Initialized() { return bf_chassis_manager_->initialized_; }

  ::util::Status PushChassisConfig(const ChassisConfig& config) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->PushChassisConfig(config);
  }

  ::util::Status PushChassisConfig(const ChassisConfigBuilder& builder) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->PushChassisConfig(builder.Get());
  }

  ::util::Status PushBaseChassisConfig(ChassisConfigBuilder* builder) {
    CHECK_RETURN_IF_FALSE(!Initialized())
        << "Can only call PushBaseChassisConfig() for first ChassisConfig!";
    builder->AddPort(kPortId, kPort, ADMIN_STATE_ENABLED);

    // PortStatusChangeRegisterEventWriter called because this is the first call
    // to PushChassisConfig
    EXPECT_CALL(*bf_pal_mock_, PortStatusChangeRegisterEventWriter(_));
    EXPECT_CALL(*bf_pal_mock_, PortAdd(kUnit, kPortId, kDefaultSpeedBps));
    EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId));

    EXPECT_CALL(*phal_mock_,
                RegisterTransceiverEventWriter(
                    _, PhalInterface::kTransceiverEventWriterPriorityHigh))
        .WillOnce(Return(kTestTransceiverWriterId));
    EXPECT_CALL(*phal_mock_,
                UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
        .WillOnce(Return(::util::OkStatus()));

    RETURN_IF_ERROR(PushChassisConfig(builder->Get()));
    auto unit = GetUnitFromNodeId(kNodeId);
    CHECK_RETURN_IF_FALSE(unit.ok());
    CHECK_RETURN_IF_FALSE(unit.ValueOrDie() == kUnit)
        << "Invalid unit number!";
    CHECK_RETURN_IF_FALSE(Initialized())
        << "Class is not initialized after push!";
    return ::util::OkStatus();
  }

  ::util::Status ReplayPortsConfig(uint64 node_id) {
    absl::WriterMutexLock l(&chassis_lock);
    return bf_chassis_manager_->ReplayPortsConfig(node_id);
  }

  ::util::Status PushBaseChassisConfig() {
    ChassisConfigBuilder builder;
    return PushBaseChassisConfig(&builder);
  }

  ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const {
    absl::ReaderMutexLock l(&chassis_lock);
    return bf_chassis_manager_->GetUnitFromNodeId(node_id);
  }

  ::util::Status Shutdown() {
    return bf_chassis_manager_->Shutdown();
  }

  ::util::Status ShutdownAndTestCleanState() {
    EXPECT_CALL(*bf_pal_mock_,
                PortStatusChangeUnregisterEventWriter())
        .WillOnce(Return(::util::OkStatus()));
    RETURN_IF_ERROR(Shutdown());
    RETURN_IF_ERROR(CheckCleanInternalState());
    return ::util::OkStatus();
  }

  std::unique_ptr<ChannelWriter<TransceiverEvent>> GetTransceiverEventWriter() {
    absl::WriterMutexLock l(&chassis_lock);
    CHECK(bf_chassis_manager_->xcvr_event_channel_ != nullptr)
        << "xcvr channel is null!";
    return ChannelWriter<PhalInterface::TransceiverEvent>::Create(
        bf_chassis_manager_->xcvr_event_channel_);
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<BFPalMock> bf_pal_mock_;
  std::unique_ptr<BFChassisManager> bf_chassis_manager_;

  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_F(BFChassisManagerTest, PreFirstConfigPushState) {
  ASSERT_OK(CheckCleanInternalState());
  EXPECT_FALSE(Initialized());
  // TODO(antonin): add more checks (to verify that method calls fail as
  // expected)
}

TEST_F(BFChassisManagerTest, FirstConfigPush) {
  ASSERT_OK(PushBaseChassisConfig());
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, RemovePort) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  builder.RemoveLastPort();
  EXPECT_CALL(*bf_pal_mock_, PortDelete(kUnit, kPortId));
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, ReplayPorts) {
  ASSERT_OK(PushBaseChassisConfig());

  EXPECT_CALL(*bf_pal_mock_, PortAdd(kUnit, kPortId, kDefaultSpeedBps));
  EXPECT_CALL(*bf_pal_mock_, PortEnable(kUnit, kPortId));

  // For now, when replaying the port configuration, we set the mtu and autoneg
  // even if the values where already the defaults. This seems like a good idea
  // to ensure configuration consistency.
  EXPECT_CALL(*bf_pal_mock_, PortMtuSet(kUnit, kPortId, 0)).Times(AtMost(1));
  EXPECT_CALL(*bf_pal_mock_,
              PortAutonegPolicySet(kUnit, kPortId, TRI_STATE_UNKNOWN))
      .Times(AtMost(1));

  EXPECT_OK(ReplayPortsConfig(kNodeId));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(BFChassisManagerTest, TransceiverEvent) {
  ASSERT_OK(PushBaseChassisConfig());
  auto xcvr_event_writer = GetTransceiverEventWriter();

  EXPECT_CALL(*phal_mock_, GetFrontPanelPortInfo(kSlot, kPort, _));

  EXPECT_OK(xcvr_event_writer->Write(
      TransceiverEvent{kSlot, kPort, HW_STATE_PRESENT},
      absl::InfiniteDuration()));

  ASSERT_OK(ShutdownAndTestCleanState());
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
