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

TEST(LedDatasourceTest, InitializeFailedNoLed) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  std::string error_message =
      "The LED with OID 12345 is not currently present.";
  EXPECT_THAT(OnlpLedDataSource::Make(12345, &mock_onlp_interface, nullptr),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST(LedDatasourceTest, InitializeLEDWithEmptyInfo) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetLedInfo(12345))
      .Times(2)
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(12345, &mock_onlp_interface, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(led_datasource.get(), nullptr);
}

TEST(LedDatasourceTest, GetLedData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_led_info.character = 11;
  mock_led_info.caps = (ONLP_LED_CAPS_RED | ONLP_LED_CAPS_GREEN);
  mock_led_info.mode = ONLP_LED_MODE_RED;

  EXPECT_CALL(mock_onlp_interface, GetLedInfo(12345))
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(12345, &mock_onlp_interface, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(led_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(led_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  EXPECT_OK(led_datasource->IsCapable((LedCaps)(ONLP_LED_CAPS_RED
            |ONLP_LED_CAPS_GREEN)));

  EXPECT_THAT(led_datasource->GetLedId(),
              ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(led_datasource->GetLedChar(),
              ContainsValue<char>(11));

  EXPECT_THAT(
      led_datasource->GetLedMode(),
      ContainsValue(LedMode_descriptor()->FindValueByName("LED_MODE_RED")));

  EXPECT_THAT(
      led_datasource->GetLedHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

TEST(LedDatasourceTest, SetLedData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_led_info_t mock_led_info = {};
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  mock_led_info.character = 11;
  mock_led_info.caps = (ONLP_LED_CAPS_RED | ONLP_LED_CAPS_GREEN);
  mock_led_info.mode = ONLP_LED_MODE_RED;

  EXPECT_CALL(mock_onlp_interface, GetLedInfo(12345))
      .WillRepeatedly(Return(LedInfo(mock_led_info)));

  ::util::StatusOr<std::shared_ptr<OnlpLedDataSource>> result =
      OnlpLedDataSource::Make(12345, &mock_onlp_interface, nullptr);

  ASSERT_OK(result);

  std::shared_ptr<OnlpLedDataSource> led_datasource =
      result.ConsumeValueOrDie();

  EXPECT_NE(led_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(led_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  EXPECT_OK(led_datasource->IsCapable((LedCaps)(ONLP_LED_CAPS_RED
            |ONLP_LED_CAPS_GREEN)));

  EXPECT_THAT(led_datasource->GetLedId(),
              ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(led_datasource->GetLedChar(),
              ContainsValue<char>(11));

  EXPECT_THAT(
      led_datasource->GetLedMode(),
      ContainsValue(LedMode_descriptor()->FindValueByName("LED_MODE_RED")));

  // Write to the system.
  EXPECT_TRUE(led_datasource->GetLedMode()->CanSet());

  EXPECT_CALL(mock_onlp_interface, SetLedMode(12345, LedMode::LED_MODE_GREEN))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      led_datasource->GetLedMode()
      ->Set(LedMode_descriptor()->FindValueByName("LED_MODE_GREEN")));

  EXPECT_TRUE(led_datasource->GetLedChar()->CanSet());

  EXPECT_CALL(mock_onlp_interface, SetLedCharacter(12345, '2'))
      .WillOnce(Return(::util::OkStatus()));

  EXPECT_OK(
      led_datasource->GetLedChar()->Set('2'));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
