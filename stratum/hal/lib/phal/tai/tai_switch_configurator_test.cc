// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/tai/tai_interface_mock.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

const int32 kOid = 10;
const uint64 kFreq = 195000000000;
const uint64 kModFormat = 2;
const double kOutputPower = -3.14;
const double kInputPower = -1;
const double kTargetOutputPower = -3.14;

class TaiSwitchConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tai_interface_ = absl::make_unique<TaiInterfaceMock>();
  }

  std::unique_ptr<TaiInterfaceMock> tai_interface_;
  std::vector<uint64> module_ids_ = {15};
  std::vector<uint64> netif_ids_ = {10};
  std::vector<uint64> host_id_ids = {10};
};

TEST_F(TaiSwitchConfiguratorTest, GenerateDefaultPhalConfig) {
  EXPECT_CALL(*tai_interface_, GetModuleIds())
      .WillOnce(::testing::Return(
          ::util::StatusOr<std::vector<uint64>>(module_ids_)));
  EXPECT_CALL(*tai_interface_, GetNetworkInterfaceIds(::testing::_))
      .WillOnce(
          ::testing::Return(::util::StatusOr<std::vector<uint64>>(netif_ids_)));

  auto status_or = TaiSwitchConfigurator::Make(tai_interface_.get());
  ASSERT_OK(status_or);
  auto switch_configurator = std::move(status_or.ConsumeValueOrDie());

  // Get default config
  PhalInitConfig config;
  switch_configurator->CreateDefaultConfig(&config);
  ASSERT_EQ(config.optical_modules_size(), 1);

  auto module_config = config.optical_modules(0);
  ASSERT_EQ(module_config.module(), 1);  // module id
  // TAI object ID, from TAI interface
  ASSERT_EQ(module_config.vendor_specific_id(), 15);
  ASSERT_EQ(module_config.network_interfaces_size(), 1);
  auto netif_config = module_config.network_interfaces(0);
  ASSERT_EQ(netif_config.network_interface(), 1);  // network interface id
  // TAI object ID, from TAI interface
  ASSERT_EQ(netif_config.vendor_specific_id(), 10);
}

TEST_F(TaiSwitchConfiguratorTest, ConfigurPhalDb) {
  auto status_or = TaiSwitchConfigurator::Make(tai_interface_.get());
  ASSERT_OK(status_or);
  auto switch_configurator = std::move(status_or.ConsumeValueOrDie());

  // make default config
  PhalInitConfig config;
  auto module_config = config.add_optical_modules();
  module_config->set_module(1);  // module id
  module_config->set_vendor_specific_id(15);
  auto netif = module_config->add_network_interfaces();
  netif->set_network_interface(1);  // network_interface id
  netif->set_vendor_specific_id(10);

  // The configurator will create a data source for a network interface
  EXPECT_CALL(*tai_interface_, GetTxLaserFrequency(kOid))
      .WillOnce(::testing::Return(::util::StatusOr<uint64>(kFreq)));
  EXPECT_CALL(*tai_interface_, GetModulationFormat(kOid))
      .WillOnce(::testing::Return(::util::StatusOr<uint64>(kModFormat)));
  EXPECT_CALL(*tai_interface_, GetCurrentOutputPower(kOid))
      .WillOnce(::testing::Return(::util::StatusOr<double>(kOutputPower)));
  EXPECT_CALL(*tai_interface_, GetCurrentInputPower(kOid))
      .WillOnce(::testing::Return(::util::StatusOr<double>(kInputPower)));
  EXPECT_CALL(*tai_interface_, GetTargetOutputPower(kOid))
      .WillOnce(
          ::testing::Return(::util::StatusOr<double>(kTargetOutputPower)));

  std::unique_ptr<AttributeGroup> root_group =
      AttributeGroup::From(PhalDB::descriptor());
  switch_configurator->ConfigurePhalDB(&config, root_group.get());
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
