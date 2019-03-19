// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "stratum/hal/lib/phal/onlp/fan_datasource.h"

#include <memory>
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
// FIXME #include "absl/strings/util.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/glue/status/status_test_util.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::stratum::test_utils::StatusIs;

TEST(FanDatasourceTest, InitializeFailedNoFan) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  std::string error_message =
      "The FAN with OID 12345 is not currently present.";
  EXPECT_THAT(OnlpFanDataSource::Make(12345, &mock_onlp_interface, nullptr),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST(FanDatasourceTest, InitializeFANWithEmptyInfo) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  onlp_fan_info_t mock_fan_info = {};
  mock_fan_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetFanInfo(12345))
      .Times(2)
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(12345, &mock_onlp_interface, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(fan_datasource.get(), nullptr);
}

TEST(FanDatasourceTest, GetFanData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_fan_info_t mock_fan_info = {};
  mock_fan_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  strncpy(mock_fan_info.model, "test_fan_model",
              sizeof(mock_fan_info.model));
  strncpy(mock_fan_info.serial, "test_fan_serial",
              sizeof(mock_fan_info.serial));

  mock_fan_info.percentage = 1111;
  mock_fan_info.rpm = 2222;
  mock_fan_info.dir = ONLP_FAN_DIR_F2B;
  mock_fan_info.caps = (ONLP_FAN_CAPS_SET_DIR | ONLP_FAN_CAPS_GET_RPM);

  EXPECT_CALL(mock_onlp_interface, GetFanInfo(12345))
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(12345, &mock_onlp_interface, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(fan_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(fan_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  EXPECT_OK(fan_datasource->IsCapable((FanCaps)(ONLP_FAN_CAPS_SET_DIR
            |ONLP_FAN_CAPS_GET_RPM)));
  EXPECT_THAT(fan_datasource->GetFanModel(),
              ContainsValue<std::string>("test_fan_model"));
  EXPECT_THAT(fan_datasource->GetFanSerialNumber(),
              ContainsValue<std::string>("test_fan_serial"));

  EXPECT_THAT(fan_datasource->GetFanId(), ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(fan_datasource->GetFanPercentage(),
              ContainsValue<int>(1111));
  EXPECT_THAT(fan_datasource->GetFanRPM(),
              ContainsValue<int>(2222));

  EXPECT_THAT(
      fan_datasource->GetFanDirection(),
      ContainsValue(FanDir_descriptor()->FindValueByName("FAN_DIR_F2B")));

  EXPECT_THAT(
      fan_datasource->GetFanHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

TEST(FanDatasourceTest, SetFanData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_fan_info_t mock_fan_info = {};
  mock_fan_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  strncpy(mock_fan_info.model, "test_fan_model",
              sizeof(mock_fan_info.model));
  strncpy(mock_fan_info.serial, "test_fan_serial",
              sizeof(mock_fan_info.serial));

  mock_fan_info.percentage = 1111;
  mock_fan_info.rpm = 2222;
  mock_fan_info.dir = ONLP_FAN_DIR_F2B;
  mock_fan_info.caps = (ONLP_FAN_CAPS_SET_DIR | ONLP_FAN_CAPS_GET_RPM);
  EXPECT_CALL(mock_onlp_interface, GetFanInfo(12345))
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(12345, &mock_onlp_interface, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(fan_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(fan_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  EXPECT_OK(fan_datasource->IsCapable((FanCaps)(ONLP_FAN_CAPS_SET_DIR
            |ONLP_FAN_CAPS_GET_RPM)));
  EXPECT_THAT(fan_datasource->GetFanModel(),
              ContainsValue<std::string>("test_fan_model"));
  EXPECT_THAT(fan_datasource->GetFanSerialNumber(),
              ContainsValue<std::string>("test_fan_serial"));

  EXPECT_THAT(fan_datasource->GetFanId(), ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(fan_datasource->GetFanPercentage(),
              ContainsValue<int>(1111));
  EXPECT_THAT(fan_datasource->GetFanRPM(),
              ContainsValue<int>(2222));

  EXPECT_THAT(
      fan_datasource->GetFanDirection(),
      ContainsValue(FanDir_descriptor()->FindValueByName("FAN_DIR_F2B")));

  // Write to the system.
  EXPECT_TRUE(fan_datasource->GetFanPercentage()->CanSet());

  EXPECT_CALL(mock_onlp_interface, SetFanPercent(12345, 3333))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      fan_datasource->GetFanPercentage()->Set(3333));

  EXPECT_TRUE(fan_datasource->GetFanRPM()->CanSet());

  EXPECT_CALL(mock_onlp_interface, SetFanRpm(12345, 4444))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      fan_datasource->GetFanRPM()->Set(4444));

  EXPECT_TRUE(fan_datasource->GetFanDirection()->CanSet());

  EXPECT_CALL(mock_onlp_interface, SetFanDir(12345, FanDir::FAN_DIR_B2F))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      fan_datasource->GetFanDirection()
      ->Set(FanDir_descriptor()->FindValueByName("FAN_DIR_B2F")));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
