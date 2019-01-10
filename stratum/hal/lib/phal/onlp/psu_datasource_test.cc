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

#include "stratum/hal/lib/phal/onlp/psu_datasource.h"

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

TEST(PsuDatasourceTest, InitializeFailedNoPsu) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  std::string error_message =
      "The PSU with OID 12345 is not currently present.";
  EXPECT_THAT(OnlpPsuDataSource::Make(12345, &mock_onlp_interface, nullptr),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST(PsuDatasourceTest, InitializePSUWithEmptyInfo) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillOnce(Return(OidInfo(mock_oid_info)));

  onlp_psu_info_t mock_psu_info = {};
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  EXPECT_CALL(mock_onlp_interface, GetPsuInfo(12345))
      .Times(2)
      .WillRepeatedly(Return(PsuInfo(mock_psu_info)));

  ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> result =
      OnlpPsuDataSource::Make(12345, &mock_onlp_interface, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpPsuDataSource> psu_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(psu_datasource.get(), nullptr);
}

TEST(PsuDatasourceTest, GetPsuData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(mock_onlp_interface, GetOidInfo(12345))
      .WillRepeatedly(Return(OidInfo(mock_oid_info)));

  onlp_psu_info_t mock_psu_info = {};
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  strncpy(mock_psu_info.model, "test_psu_model",
              sizeof(mock_psu_info.model));
  strncpy(mock_psu_info.serial, "test_psu_serial",
              sizeof(mock_psu_info.serial));

  mock_psu_info.mvin = 1111;
  mock_psu_info.mvout = 2222;
  mock_psu_info.miin = 3333;
  mock_psu_info.miout = 4444;
  mock_psu_info.mpin = 5555;
  mock_psu_info.mpout = 6666;
  mock_psu_info.type = ONLP_PSU_TYPE_AC;
  mock_psu_info.caps = ONLP_PSU_CAPS_GET_VIN;

  EXPECT_CALL(mock_onlp_interface, GetPsuInfo(12345))
      .WillRepeatedly(Return(PsuInfo(mock_psu_info)));

  ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> result =
      OnlpPsuDataSource::Make(12345, &mock_onlp_interface, nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpPsuDataSource> psu_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(psu_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(psu_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  EXPECT_THAT(psu_datasource->GetPsuModel(),
              ContainsValue<std::string>("test_psu_model"));
  EXPECT_THAT(psu_datasource->GetPsuSerialNumber(),
              ContainsValue<std::string>("test_psu_serial"));
  EXPECT_THAT(psu_datasource->GetPsuId(), ContainsValue<OnlpOid>(12345));

  EXPECT_THAT(psu_datasource->GetPsuInputVoltage(),
              ContainsValue<double>(1111 / 1000.0));
  EXPECT_THAT(psu_datasource->GetPsuOutputVoltage(),
              ContainsValue<double>(2222 / 1000.0));
  EXPECT_THAT(psu_datasource->GetPsuInputCurrent(),
              ContainsValue<double>(3333 / 1000.0));
  EXPECT_THAT(psu_datasource->GetPsuOutputCurrent(),
              ContainsValue<double>(4444 / 1000.0));
  EXPECT_THAT(psu_datasource->GetPsuInputPower(),
              ContainsValue<double>(5555 / 1000.0));
  EXPECT_THAT(psu_datasource->GetPsuOutputPower(),
              ContainsValue<double>(6666 / 1000.0));
  EXPECT_THAT(
      psu_datasource->GetPsuType(),
      ContainsValue(PsuType_descriptor()->FindValueByName("PSU_TYPE_AC")));
  EXPECT_THAT(
      psu_datasource->GetPsuCaps(),
      ContainsValue(PsuCaps_descriptor()->FindValueByName("PSU_CAPS_GET_VIN")));

  EXPECT_THAT(
      psu_datasource->GetPsuHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
