// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bfrt_packetio_manager.h"

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

class BfrtPacketioManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    bf_sde_wrapper_mock_ = absl::make_unique<BfSdeMock>();
    bfrt_packetio_manager_ = BfrtPacketioManager::CreateInstance(
        kDevice1, bf_sde_wrapper_mock_.get());
  }

  static constexpr int kDevice1 = 0;
  static constexpr char kP4Info[] = R"PROTO(
    controller_packet_metadata {
      preamble {
        id: 67146229
        name: "packet_in"
        alias: "packet_in"
        annotations: "@controller_header(\"packet_in\")"
      }
      metadata {
        id: 1
        name: "ingress_port"
        bitwidth: 9
      }
      metadata {
        id: 2
        name: "_pad0"
        bitwidth: 7
      }
    }
    controller_packet_metadata {
      preamble {
        id: 67121543
        name: "packet_out"
        alias: "packet_out"
        annotations: "@controller_header(\"packet_out\")"
      }
      metadata {
        id: 1
        name: "egress_port"
        bitwidth: 9
      }
      metadata {
        id: 2
        name: "cpu_loopback_mode"
        bitwidth: 2
      }
      metadata {
        id: 3
        name: "pad0"
        annotations: "@padding"
        bitwidth: 85
      }
      metadata {
        id: 4
        name: "ether_type"
        bitwidth: 16
      }
    }
  )PROTO";

  std::unique_ptr<BfSdeMock> bf_sde_wrapper_mock_;
  std::unique_ptr<BfrtPacketioManager> bfrt_packetio_manager_;
  std::shared_ptr<WriterInterface<::p4::v1::PacketIn>> packet_rx_writer;
};

// Basic set up and shutdown test

// TODO(Yi Tseng): These two methods will always return OK status
// We can add tests for these methods if we modify them.
// TEST_F(BfrtPacketioManagerTest, PushChassisConfig) {}
// TEST_F(BfrtPacketioManagerTest, VerifyChassisConfig) {}

constexpr int BfrtPacketioManagerTest::kDevice1;
constexpr char BfrtPacketioManagerTest::kP4Info[];

TEST_F(BfrtPacketioManagerTest, PushForwardingPipelineConfigAndShutdown) {
  BfrtDeviceConfig config;
  auto* program = config.add_programs();
  ASSERT_OK(ParseProtoFromString(kP4Info, program->mutable_p4info()));

  // What we expected when calling PushForwardingPipelineConfig with valid config:
  // - Metadata mepping will be built
  // - StartPacketIo of SDE interface will be invoked
  EXPECT_CALL(*bf_sde_wrapper_mock_, StartPacketIo(kDevice1));
  // - Packet Rx channel will br created and new thread will be create to handle Rx
  // - RegisterPacketReceiveWriter of SDE interface will be invoked
  EXPECT_CALL(*bf_sde_wrapper_mock_, RegisterPacketReceiveWriter(kDevice1, _));
  ASSERT_OK(bfrt_packetio_manager_->PushForwardingPipelineConfig(config));

  // Make sure everything like Rx threads will be cleaned up.
  EXPECT_CALL(*bf_sde_wrapper_mock_, StopPacketIo(kDevice1));
  EXPECT_CALL(*bf_sde_wrapper_mock_, UnregisterPacketReceiveWriter(kDevice1));
  ASSERT_OK(bfrt_packetio_manager_->Shutdown());
}

TEST_F(BfrtPacketioManagerTest, TransmitPacketAfterChassisConfigPush) {
  // TODO
  EXPECT_TRUE(false);
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
