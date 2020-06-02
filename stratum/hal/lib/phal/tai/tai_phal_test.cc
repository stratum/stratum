// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/tai/tai_phal.h"

#include <memory>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/tai/tai_interface_mock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

class TaiPhalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tai_interface_ = absl::make_unique<TaiInterfaceMock>();
  }

  std::unique_ptr<TaiInterfaceMock> tai_interface_;
};

TEST_F(TaiPhalTest, PushChassisConfig) {
  uint32 id = 1;
  uint64 frequency = 100000000000;
  EXPECT_CALL(*tai_interface_, SetTxLaserFrequency(id, frequency))
      .WillOnce(::testing::Return(::util::OkStatus()));
  double output_power = -10.5;
  EXPECT_CALL(*tai_interface_, SetTargetOutputPower(id, output_power))
      .WillOnce(::testing::Return(::util::OkStatus()));
  uint64 modulation_format = 42;
  EXPECT_CALL(*tai_interface_, SetModulationFormat(id, modulation_format))
      .WillOnce(::testing::Return(::util::OkStatus()));

  ChassisConfig config;
  auto* netif = config.add_optical_network_interfaces();
  netif->set_id(id);
  netif->set_name("netif-1");
  netif->set_module(1);
  netif->set_network_interface(1);
  netif->set_frequency(frequency);
  netif->set_target_output_power(output_power);
  netif->set_operational_mode(modulation_format);
  netif->set_line_port("card-1");

  TaiPhal* tai_phal = TaiPhal::CreateSingleton(tai_interface_.get());
  EXPECT_OK(tai_phal->PushChassisConfig(config));
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
