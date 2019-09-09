// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
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

class FanDatasourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    id_ = 12345;
    oid_ = ONLP_FAN_ID_CREATE(id_);
  }

  int id_;             // Id for this FAN
  OnlpOid oid_;        // OID for this FAN (i.e. Type + Id)
  onlp_oid_hdr_t mock_oid_info_;
  MockOnlpWrapper mock_onlp_interface_;
};

TEST_F(FanDatasourceTest, InitializeFANWithEmptyInfo) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillOnce(Return(OidInfo(mock_oid_info_)));

  onlp_fan_info_t mock_fan_info = {};
  mock_fan_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetFanInfo(oid_))
      .Times(2)
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(id_, &mock_onlp_interface_, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(fan_datasource.get(), nullptr);
}

TEST_F(FanDatasourceTest, GetFanData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

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

  EXPECT_CALL(mock_onlp_interface_, GetFanInfo(oid_))
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(id_, &mock_onlp_interface_, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(fan_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(fan_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(fan_datasource->GetCapSetDir(), ContainsValue(true));
  EXPECT_THAT(fan_datasource->GetCapGetDir(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapSetRpm(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapSetPercentage(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapGetRpm(), ContainsValue(true));
  EXPECT_THAT(fan_datasource->GetCapGetPercentage(), ContainsValue(false));

  EXPECT_THAT(fan_datasource->GetFanModel(),
              ContainsValue<std::string>("test_fan_model"));
  EXPECT_THAT(fan_datasource->GetFanSerialNumber(),
              ContainsValue<std::string>("test_fan_serial"));

  EXPECT_THAT(fan_datasource->GetFanId(), ContainsValue<int>(id_));

  EXPECT_THAT(fan_datasource->GetFanPercentage(),
              ContainsValue<int>(1111));
  EXPECT_THAT(fan_datasource->GetFanRPM(),
              ContainsValue<double>(2222));

  EXPECT_THAT(
      fan_datasource->GetFanDirection(),
      ContainsValue(FanDir_descriptor()->FindValueByName("FAN_DIR_F2B")));

  EXPECT_THAT(
      fan_datasource->GetFanHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

TEST_F(FanDatasourceTest, SetFanData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

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
  EXPECT_CALL(mock_onlp_interface_, GetFanInfo(oid_))
      .WillRepeatedly(Return(FanInfo(mock_fan_info)));

  ::util::StatusOr<std::shared_ptr<OnlpFanDataSource>> result =
      OnlpFanDataSource::Make(id_, &mock_onlp_interface_, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpFanDataSource> fan_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(fan_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(fan_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(fan_datasource->GetCapSetDir(), ContainsValue(true));
  EXPECT_THAT(fan_datasource->GetCapGetDir(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapSetRpm(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapSetPercentage(), ContainsValue(false));
  EXPECT_THAT(fan_datasource->GetCapGetRpm(), ContainsValue(true));
  EXPECT_THAT(fan_datasource->GetCapGetPercentage(), ContainsValue(false));

  EXPECT_THAT(fan_datasource->GetFanModel(),
              ContainsValue<std::string>("test_fan_model"));
  EXPECT_THAT(fan_datasource->GetFanSerialNumber(),
              ContainsValue<std::string>("test_fan_serial"));

  EXPECT_THAT(fan_datasource->GetFanId(), ContainsValue<int>(id_));

  EXPECT_THAT(fan_datasource->GetFanPercentage(),
              ContainsValue<int>(1111));
  EXPECT_THAT(fan_datasource->GetFanRPM(),
              ContainsValue<double>(2222));

  EXPECT_THAT(
      fan_datasource->GetFanDirection(),
      ContainsValue(FanDir_descriptor()->FindValueByName("FAN_DIR_F2B")));

  // Write to the system.
  EXPECT_TRUE(fan_datasource->GetFanPercentage()->CanSet());

  EXPECT_CALL(mock_onlp_interface_, SetFanPercent(oid_, 3333))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      fan_datasource->GetFanPercentage()->Set(3333));

  EXPECT_TRUE(fan_datasource->GetFanRPM()->CanSet());

  EXPECT_CALL(mock_onlp_interface_, SetFanRpm(oid_, 4444))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(fan_datasource->GetFanRPM()->Set(4444.0));

  // RPM bigger than an int should fail
  std::string error_message = "Set Fan RPM bigger than an integer";
  EXPECT_THAT(fan_datasource->GetFanRPM()->Set(32768.0),
      StatusIs(_, _, HasSubstr(error_message)));

  EXPECT_TRUE(fan_datasource->GetFanDirection()->CanSet());

  EXPECT_CALL(mock_onlp_interface_, SetFanDir(oid_, FanDir::FAN_DIR_B2F))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      fan_datasource->GetFanDirection()
      ->Set(FanDir_descriptor()->FindValueByName("FAN_DIR_B2F")));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
