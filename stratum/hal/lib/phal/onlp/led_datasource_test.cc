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

#include "stratum/hal/lib/phal/onlp/led_datasource.h"

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

class LedDatasourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    id_ = 12345;
    oid_ = ONLP_LED_ID_CREATE(id_);
  }
  int id_;             // Id for this LED
  OnlpOid oid_;        // OID for this LED (i.e. Type + Id)
  onlp_oid_hdr_t mock_oid_info_;
  MockOnlpWrapper mock_onlp_interface_;
};

TEST_F(LedDatasourceTest, InitializeLEDWithEmptyInfo) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillOnce(Return(OidInfo(mock_oid_info_)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetLedInfo(oid_))
      .Times(2)
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(id_, &mock_onlp_interface_, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(led_datasource.get(), nullptr);
}

TEST_F(LedDatasourceTest, GetLedData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_led_info.character = 11;
  mock_led_info.caps = (ONLP_LED_CAPS_RED | ONLP_LED_CAPS_GREEN);
  mock_led_info.mode = ONLP_LED_MODE_RED;

  EXPECT_CALL(mock_onlp_interface_, GetLedInfo(oid_))
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(id_, &mock_onlp_interface_, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(led_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(led_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(led_datasource->GetCapOff(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapAuto(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapAutoBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapChar(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapRed(), ContainsValue(true));
  EXPECT_THAT(led_datasource->GetCapRedBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapOrange(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapOrangeBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapYellow(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapYellowBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapGreen(), ContainsValue(true));
  EXPECT_THAT(led_datasource->GetCapGreenBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapBlue(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapBlueBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapPurple(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapPurpleBlinking(), ContainsValue(false));

  EXPECT_THAT(led_datasource->GetLedId(),
              ContainsValue<int>(id_));

  EXPECT_THAT(led_datasource->GetLedChar(),
              ContainsValue<int>(11));

  EXPECT_THAT(
      led_datasource->GetLedMode(),
      ContainsValue(LedMode_descriptor()->FindValueByName("LED_MODE_RED")));

  EXPECT_THAT(
      led_datasource->GetLedHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

TEST_F(LedDatasourceTest, SetLedData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_led_info.character = 11;
  mock_led_info.caps = (ONLP_LED_CAPS_RED | ONLP_LED_CAPS_GREEN);
  mock_led_info.mode = ONLP_LED_MODE_RED;

  EXPECT_CALL(mock_onlp_interface_, GetLedInfo(oid_))
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(id_, &mock_onlp_interface_, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(led_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(led_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(led_datasource->GetCapOff(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapAuto(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapAutoBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapChar(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapRed(), ContainsValue(true));
  EXPECT_THAT(led_datasource->GetCapRedBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapOrange(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapOrangeBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapYellow(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapYellowBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapGreen(), ContainsValue(true));
  EXPECT_THAT(led_datasource->GetCapGreenBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapBlue(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapBlueBlinking(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapPurple(), ContainsValue(false));
  EXPECT_THAT(led_datasource->GetCapPurpleBlinking(), ContainsValue(false));


  EXPECT_THAT(led_datasource->GetLedId(),
              ContainsValue<int>(id_));

  EXPECT_THAT(led_datasource->GetLedChar(),
              ContainsValue<int>(11));

  EXPECT_THAT(
      led_datasource->GetLedMode(),
      ContainsValue(LedMode_descriptor()->FindValueByName("LED_MODE_RED")));

  // Write to the system.
  EXPECT_TRUE(led_datasource->GetLedMode()->CanSet());

  EXPECT_CALL(mock_onlp_interface_, SetLedMode(oid_, LedMode::LED_MODE_GREEN))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      led_datasource->GetLedMode()
      ->Set(LedMode_descriptor()->FindValueByName("LED_MODE_GREEN")));

  EXPECT_TRUE(led_datasource->GetLedChar()->CanSet());

  EXPECT_CALL(mock_onlp_interface_, SetLedCharacter(oid_,
    static_cast<int>('2'))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      led_datasource->GetLedChar()->Set(static_cast<int>('2'));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
