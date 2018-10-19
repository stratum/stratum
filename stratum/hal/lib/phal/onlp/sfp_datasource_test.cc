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

#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"

#include <memory>
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/util.h"
#include "util/task/status.h"
#include "util/task/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::status::StatusIs;

TEST(SfpDatasourceTest, InitializeFailedNoSfp) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_sfp_info_t mock_sfp_info;
  mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_UNPLUGGED;
  EXPECT_CALL(mock_onlp_interface, GetSfpInfo(12345))
      .WillOnce(Return(SfpInfo(mock_sfp_info)));

  std::string error_message =
      "The SFP with OID 12345 is not currently present.";
  EXPECT_THAT(OnlpSfpDataSource::Make(12345, &mock_onlp_interface, nullptr),
              StatusIs(_, _, HasSubstr(error_message)));
}

TEST(SfpDatasourceTest, InitializeSFPWithEmptyInfo) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_sfp_info_t mock_sfp_info;
  mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info.dom.nchannels = 0;
  mock_sfp_info.sff.sfp_type = SFF_SFP_TYPE_SFP;
  EXPECT_CALL(mock_onlp_interface, GetSfpInfo(12345))
      .Times(2)
      .WillRepeatedly(Return(SfpInfo(mock_sfp_info)));

  ASSERT_OK_AND_ASSIGN(
      std::shared_ptr<OnlpSfpDataSource> sfp_datasource,
      OnlpSfpDataSource::Make(12345, &mock_onlp_interface, nullptr));
  EXPECT_NE(sfp_datasource.get(), nullptr);
}


TEST(SfpDatasourceTest, GetSfpData) {
  MockOnlpWrapper mock_onlp_interface;
  onlp_sfp_info_t mock_info = {};
  mock_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_info.type = ONLP_SFP_TYPE_SFP;
  mock_info.sff.sfp_type = SFF_SFP_TYPE_SFP;
  safestrncpy(mock_info.sff.vendor, "test_sfp_vendor",
              sizeof(mock_info.sff.vendor));
  safestrncpy(mock_info.sff.model, "test_sfp_model",
              sizeof(mock_info.sff.model));
  safestrncpy(mock_info.sff.serial, "test_sfp_serial",
              sizeof(mock_info.sff.serial));
  // Mock sfp_dom info response.
  SffDomInfo *mock_sfp_dom_info = &mock_info.dom;
  mock_sfp_dom_info->temp = 123;
  mock_sfp_dom_info->nchannels = 1;
  mock_sfp_dom_info->voltage = 234;
  mock_sfp_dom_info->channels[0].tx_power = 1111;
  mock_sfp_dom_info->channels[0].rx_power = 2222;
  mock_sfp_dom_info->channels[0].bias_cur = 3333;

  EXPECT_CALL(mock_onlp_interface, GetSfpInfo(12345))
      .WillRepeatedly(Return(SfpInfo(mock_info)));

  ASSERT_OK_AND_ASSIGN(
      std::shared_ptr<OnlpSfpDataSource> sfp_datasource,
      OnlpSfpDataSource::Make(12345, &mock_onlp_interface, nullptr));
  EXPECT_NE(sfp_datasource.get(), nullptr);
  // Update value and check attribute fields.
  EXPECT_OK(sfp_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  EXPECT_THAT(sfp_datasource->GetSfpVendor(),
              ContainsValue<std::string>("test_sfp_vendor"));
  EXPECT_THAT(sfp_datasource->GetSfpModel(),
              ContainsValue<std::string>("test_sfp_model"));
  EXPECT_THAT(sfp_datasource->GetSfpSerialNumber(),
              ContainsValue<std::string>("test_sfp_serial"));
  EXPECT_THAT(sfp_datasource->GetSfpId(), ContainsValue<OnlpOid>(12345));
  // Convert 0.1uW to dBm unit.
  EXPECT_THAT(sfp_datasource->GetSfpTxPower(0),
              ContainsValue<double>(10 * log10(1111.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpRxPower(0),
              ContainsValue<double>(10 * log10(2222.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpTxBias(0),
              ContainsValue<double>(3333.0 / 500.0));
  EXPECT_THAT(sfp_datasource->GetSfpVoltage(),
              ContainsValue<double>(234.0 / 10000.0));
  EXPECT_THAT(sfp_datasource->GetSfpTemperature(),
              ContainsValue<double>(123.0 / 256.0));
  EXPECT_THAT(
      sfp_datasource->GetSfpHardwareState(),
      ContainsValue(HwState_descriptor()->FindValueByName("HW_STATE_PRESENT")));
  EXPECT_THAT(
      sfp_datasource->GetSfpMediaType(),
      ContainsValue(MediaType_descriptor()->FindValueByName("MEDIA_TYPE_SFP")));
}
}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
