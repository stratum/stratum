// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/tai/tai_optics_datasource.h"

#include <memory>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/tai/tai_interface_mock.h"
#include "stratum/lib/macros.h"

namespace stratum {
namespace hal {
namespace phal {
namespace tai {

const int32 kOid = 10;
const uint64 kNetIf = 1;
const uint64 kFreq = 195000000000;
const uint64 kModFormat = 2;
const double kOutputPower = -3.14;
const double kInputPower = -1;
const double kTargetOutputPower = -3.14;

class TaiOpticasDataSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tai_interface_ = absl::make_unique<TaiInterfaceMock>();
    netif_config_.set_network_interface(kNetIf);
    netif_config_.set_vendor_specific_id(kOid);
  }

  std::unique_ptr<TaiInterfaceMock> tai_interface_;
  PhalOpticalModuleConfig::NetworkInterface netif_config_;
};

TEST_F(TaiOpticasDataSourceTest, BasicTests) {
  // When the data source initialized, it will try to grab initial values from
  // TAI interface.
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
  auto status_or =
      TaiOpticsDataSource::Make(netif_config_, tai_interface_.get());
  ASSERT_OK(status_or);

  // Get UpdateValues
  auto datasource = status_or.ValueOrDie();
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
  datasource->UpdateValuesAndLock();

  // Get individual values
  // ID
  {
    auto attr = datasource->GetId();
    auto status_or_val = attr->ReadValue<int32>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kNetIf);
  }

  // Tx Frequency
  {
    auto attr = datasource->GetTxLaserFrequency();
    auto status_or_val = attr->ReadValue<uint64>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kFreq);
  }

  // OperationalMode (Module format)
  {
    auto attr = datasource->GetOperationalMode();
    auto status_or_val = attr->ReadValue<uint64>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kModFormat);
  }

  // Output power
  {
    auto attr = datasource->GetCurrentOutputPower();
    auto status_or_val = attr->ReadValue<double>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kOutputPower);
  }

  // Input power
  {
    auto attr = datasource->GetCurrentInputPower();
    auto status_or_val = attr->ReadValue<double>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kInputPower);
  }

  // Target output power
  {
    auto attr = datasource->GetTargetOutputPower();
    auto status_or_val = attr->ReadValue<double>();
    ASSERT_OK(status_or_val);
    ASSERT_EQ(status_or_val.ValueOrDie(), kTargetOutputPower);
  }

  // Set attributes
  // Tx Frequency
  {
    uint64 new_frequency = 100000000000;
    EXPECT_CALL(*tai_interface_, SetTxLaserFrequency(kOid, new_frequency))
        .WillOnce(::testing::Return(::util::OkStatus()));
    auto attr = datasource->GetTxLaserFrequency();
    ASSERT_TRUE(attr->CanSet());
    Attribute val = new_frequency;
    ASSERT_OK(attr->Set(val));
  }
  // Target output power
  {
    double new_power = -10.5;
    EXPECT_CALL(*tai_interface_, SetTargetOutputPower(kOid, new_power))
        .WillOnce(::testing::Return(::util::OkStatus()));
    auto attr = datasource->GetTargetOutputPower();
    ASSERT_TRUE(attr->CanSet());
    Attribute val = new_power;
    ASSERT_OK(attr->Set(val));
  }

  // OperationalMode (Modulation format)
  {
    uint64 new_modulation_format = 42;
    EXPECT_CALL(*tai_interface_,
                SetModulationFormat(kOid, new_modulation_format))
        .WillOnce(::testing::Return(::util::OkStatus()));
    auto attr = datasource->GetOperationalMode();
    ASSERT_TRUE(attr->CanSet());
    Attribute val = new_modulation_format;
    ASSERT_OK(attr->Set(val));
  }
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
