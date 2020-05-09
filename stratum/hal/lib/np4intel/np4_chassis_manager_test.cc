// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2019-present Dell EMC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/np4intel/np4_chassis_manager.h"

#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/phal_mock.h"
#include "stratum/lib/constants.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::Return;

namespace stratum {
namespace hal {
namespace np4intel {

namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr uint64 kNodeId = 7654321ULL;
// For Netcope, unit is the 0-based index of the node in the ChassisConfig.
constexpr int kUnit = 0;
constexpr int kSlot = 1;
constexpr int kPort = 1;
constexpr uint32 kPortId = 12345;
constexpr uint64 kDefaultSpeedBps = kHundredGigBps;
constexpr FecMode kDefaultFecMode = FEC_MODE_UNKNOWN;

// A helper class to build a single-node ChassisConfig message.
class ChassisConfigBuilder {
 public:
  explicit ChassisConfigBuilder(uint64 node_id = kNodeId) : node_id(node_id) {
    config_.set_description("Test config for NP4ChassisManager");
    auto* chassis = config_.mutable_chassis();
    chassis->set_platform(PLT_NP4_INTEL_N3000);
    chassis->set_name("N3000");

    auto* node = config_.add_nodes();
    node->set_id(node_id);
    node->set_slot(kSlot);
  }

  SingletonPort* AddPort(uint32 port_id, int32 port, AdminState admin_state,
                         uint64 speed_bps = kDefaultSpeedBps,
                         FecMode fec_mode = kDefaultFecMode) {
    auto* sport = config_.add_singleton_ports();
    sport->set_id(port_id);
    sport->set_node(node_id);
    sport->set_port(port);
    sport->set_slot(kSlot);
    sport->set_channel(0);
    sport->set_speed_bps(speed_bps);
    sport->mutable_config_params()->set_admin_state(admin_state);
    sport->mutable_config_params()->set_fec_mode(fec_mode);
    return sport;
  }

  SingletonPort* GetPort(uint32 port_id) {
    for (auto& sport : *config_.mutable_singleton_ports()) {
      if (sport.id() == port_id) return &sport;
    }
    return nullptr;
  }

  void RemoveLastPort() { config_.mutable_singleton_ports()->RemoveLast(); }

  const ChassisConfig& Get() const { return config_; }

 private:
  uint64 node_id;
  ChassisConfig config_;
};

}  // namespace

class NP4ChassisManagerTest : public ::testing::Test {
 protected:
  NP4ChassisManagerTest() {}

  void SetUp() override {
    phal_mock_ = absl::make_unique<PhalMock>();
    np4_chassis_manager_ = NP4ChassisManager::CreateInstance(phal_mock_.get());
  }

  ::util::Status CheckCleanInternalState() {
    CHECK_RETURN_IF_FALSE(
        np4_chassis_manager_->node_id_to_port_id_to_port_state_.empty());
    CHECK_RETURN_IF_FALSE(
        np4_chassis_manager_->node_id_to_port_id_to_port_config_.empty());
    CHECK_RETURN_IF_FALSE(
        np4_chassis_manager_->port_status_change_event_channel_ == nullptr);
    return ::util::OkStatus();
  }

  bool Initialized() { return np4_chassis_manager_->initialized_; }

  ::util::Status PushChassisConfig(const ChassisConfig& config) {
    absl::WriterMutexLock l(&chassis_lock);
    return np4_chassis_manager_->PushChassisConfig(config);
  }

  ::util::Status PushChassisConfig(const ChassisConfigBuilder& builder) {
    absl::WriterMutexLock l(&chassis_lock);
    return np4_chassis_manager_->PushChassisConfig(builder.Get());
  }

  ::util::Status PushBaseChassisConfig(ChassisConfigBuilder* builder) {
    CHECK_RETURN_IF_FALSE(!Initialized())
        << "Can only call PushBaseChassisConfig() for first ChassisConfig!";
    builder->AddPort(kPortId, kPort, ADMIN_STATE_ENABLED);

    // EXPECT_CALL(*phal_mock_,
    //             RegisterTransceiverEventWriter(
    //                 _, PhalInterface::kTransceiverEventWriterPriorityHigh))
    //     .WillOnce(Return(kTestTransceiverWriterId));
    // EXPECT_CALL(*phal_mock_,
    //             UnregisterTransceiverEventWriter(kTestTransceiverWriterId))
    //     .WillOnce(Return(::util::OkStatus()));

    RETURN_IF_ERROR(PushChassisConfig(builder->Get()));
    CHECK_RETURN_IF_FALSE(Initialized())
        << "Class is not initialized after push!";
    return ::util::OkStatus();
  }

  ::util::Status PushBaseChassisConfig() {
    ChassisConfigBuilder builder;
    return PushBaseChassisConfig(&builder);
  }

  ::util::Status Shutdown() { return np4_chassis_manager_->Shutdown(); }

  ::util::Status ShutdownAndTestCleanState() {
    RETURN_IF_ERROR(Shutdown());
    RETURN_IF_ERROR(CheckCleanInternalState());
    return ::util::OkStatus();
  }

  std::unique_ptr<PhalMock> phal_mock_;
  std::unique_ptr<NP4ChassisManager> np4_chassis_manager_;

  static constexpr int kTestTransceiverWriterId = 20;
};

TEST_F(NP4ChassisManagerTest, PreFirstConfigPushState) {
  ASSERT_OK(CheckCleanInternalState());
  EXPECT_FALSE(Initialized());
}

TEST_F(NP4ChassisManagerTest, FirstConfigPush) {
  ASSERT_OK(PushBaseChassisConfig());
  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(NP4ChassisManagerTest, RemovePort) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  builder.RemoveLastPort();
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(NP4ChassisManagerTest, AddPortFec) {
  ChassisConfigBuilder builder;
  ASSERT_OK(PushBaseChassisConfig(&builder));

  auto portId = kPortId + 1;
  auto port = kPort + 1;

  builder.AddPort(portId, port, ADMIN_STATE_ENABLED, kHundredGigBps,
                  FEC_MODE_ON);
  ASSERT_OK(PushChassisConfig(builder));

  ASSERT_OK(ShutdownAndTestCleanState());
}

TEST_F(NP4ChassisManagerTest, TransceiverEvent) {
  ASSERT_OK(PushBaseChassisConfig());

  // EXPECT_CALL(*phal_mock_, GetFrontPanelPortInfo(kSlot, kPort, _));

  ASSERT_OK(ShutdownAndTestCleanState());
}

}  // namespace np4intel
}  // namespace hal
}  // namespace stratum
