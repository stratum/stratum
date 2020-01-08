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

#include "stratum/hal/lib/phal/onlp/psu_datasource.h"

#include <memory>
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;

class PsuDatasourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    id_ = 12345;
    oid_ = ONLP_PSU_ID_CREATE(id_);
    onlp_wrapper_mock_ = absl::make_unique<OnlpWrapperMock>();
  }

  int id_;       // Id for this PSU
  OnlpOid oid_;  // OID for this PSU (i.e. Type + Id)
  onlp_oid_hdr_t mock_oid_info_;
  std::unique_ptr<OnlpWrapperMock> onlp_wrapper_mock_;
};

TEST_F(PsuDatasourceTest, InitializePSUWithEmptyInfo) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(oid_))
      .WillOnce(Return(OidInfo(mock_oid_info_)));

  onlp_psu_info_t mock_psu_info = {};
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;

  EXPECT_CALL(*onlp_wrapper_mock_, GetPsuInfo(oid_))
      .Times(2)
      .WillRepeatedly(Return(PsuInfo(mock_psu_info)));

  ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> result =
      OnlpPsuDataSource::Make(id_, onlp_wrapper_mock_.get(), nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpPsuDataSource> psu_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(psu_datasource.get(), nullptr);
}

TEST_F(PsuDatasourceTest, GetPsuData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

  onlp_psu_info_t mock_psu_info = {};
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  strncpy(mock_psu_info.model, "test_psu_model", sizeof(mock_psu_info.model));
  strncpy(mock_psu_info.serial, "test_psu_serial",
          sizeof(mock_psu_info.serial));

  mock_psu_info.mvin = 1111;
  mock_psu_info.mvout = 2222;
  mock_psu_info.miin = 3333;
  mock_psu_info.miout = 4444;
  mock_psu_info.mpin = 5555;
  mock_psu_info.mpout = 6666;
  mock_psu_info.type = ONLP_PSU_TYPE_AC;
  mock_psu_info.caps = (ONLP_PSU_CAPS_GET_VIN | ONLP_PSU_CAPS_GET_IIN);

  EXPECT_CALL(*onlp_wrapper_mock_, GetPsuInfo(oid_))
      .WillRepeatedly(Return(PsuInfo(mock_psu_info)));

  ::util::StatusOr<std::shared_ptr<OnlpPsuDataSource>> result =
      OnlpPsuDataSource::Make(id_, onlp_wrapper_mock_.get(), nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpPsuDataSource> psu_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(psu_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(psu_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());

  // Check capabilities
  EXPECT_THAT(psu_datasource->GetCapGetType(), ContainsValue(false));
  EXPECT_THAT(psu_datasource->GetCapGetVIn(), ContainsValue(true));
  EXPECT_THAT(psu_datasource->GetCapGetVOut(), ContainsValue(false));
  EXPECT_THAT(psu_datasource->GetCapGetIIn(), ContainsValue(true));
  EXPECT_THAT(psu_datasource->GetCapGetIOut(), ContainsValue(false));
  EXPECT_THAT(psu_datasource->GetCapGetPIn(), ContainsValue(false));
  EXPECT_THAT(psu_datasource->GetCapGetPOut(), ContainsValue(false));

  EXPECT_THAT(psu_datasource->GetPsuModel(),
              ContainsValue<std::string>("test_psu_model"));
  EXPECT_THAT(psu_datasource->GetPsuSerialNumber(),
              ContainsValue<std::string>("test_psu_serial"));
  EXPECT_THAT(psu_datasource->GetPsuId(), ContainsValue<int>(id_));

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
      psu_datasource->GetPsuHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
