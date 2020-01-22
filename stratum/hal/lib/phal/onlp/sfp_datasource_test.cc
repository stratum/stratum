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

#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"

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

class SfpDatasourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    id_ = 12345;
    oid_ = ONLP_SFP_ID_CREATE(id_);
    onlp_wrapper_mock_ = absl::make_unique<OnlpWrapperMock>();
  }

  int id_;       // Id for this SFP
  OnlpOid oid_;  // OID for this SFP (i.e. Type + Id)
  onlp_oid_hdr_t mock_oid_info_;
  std::unique_ptr<OnlpWrapperMock> onlp_wrapper_mock_;
};

TEST_F(SfpDatasourceTest, InitializeSFPWithEmptyInfo) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(oid_))
      .WillOnce(Return(OidInfo(mock_oid_info_)));

  onlp_sfp_info_t mock_sfp_info = {};
  mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info.dom.nchannels = 0;
  mock_sfp_info.sff.sfp_type = SFF_SFP_TYPE_SFP;
  EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(oid_))
      .Times(2)
      .WillRepeatedly(Return(SfpInfo(mock_sfp_info)));

  ::util::StatusOr<std::shared_ptr<OnlpSfpDataSource>> result =
      OnlpSfpDataSource::Make(id_, onlp_wrapper_mock_.get(), nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpSfpDataSource> sfp_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(sfp_datasource.get(), nullptr);
}

TEST_F(SfpDatasourceTest, GetSfpData) {
  mock_oid_info_.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(oid_))
      .WillRepeatedly(Return(OidInfo(mock_oid_info_)));

  onlp_sfp_info_t mock_sfp_info = {};
  mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info.type = ONLP_SFP_TYPE_SFP;
  mock_sfp_info.sff.sfp_type = SFF_SFP_TYPE_SFP;
  mock_sfp_info.sff.module_type = SFF_MODULE_TYPE_1G_BASE_SX;
  mock_sfp_info.sff.caps = static_cast<sff_module_caps_t>(
      SFF_MODULE_CAPS_F_1G | SFF_MODULE_CAPS_F_100G);
  mock_sfp_info.sff.length = 100;

  // FIXME
  /*
  safestrncpy(mock_sfp_info.sff.length_desc, "test_cable_len",
              sizeof(mock_sfp_info.sff.length_desc));
  safestrncpy(mock_sfp_info.sff.vendor, "test_sfp_vendor",
              sizeof(mock_sfp_info.sff.vendor));
  safestrncpy(mock_sfp_info.sff.model, "test_sfp_model",
              sizeof(mock_sfp_info.sff.model));
  safestrncpy(mock_sfp_info.sff.serial, "test_sfp_serial",
              sizeof(mock_sfp_info.sff.serial));
  */
  strncpy(mock_sfp_info.sff.length_desc, "test_cable_len",
          sizeof(mock_sfp_info.sff.length_desc));
  strncpy(mock_sfp_info.sff.vendor, "test_sfp_vendor",
          sizeof(mock_sfp_info.sff.vendor));
  strncpy(mock_sfp_info.sff.model, "test_sfp_model",
          sizeof(mock_sfp_info.sff.model));
  strncpy(mock_sfp_info.sff.serial, "test_sfp_serial",
          sizeof(mock_sfp_info.sff.serial));

  // Mock sfp_dom info response.
  SffDomInfo *mock_sfp_dom_info = &mock_sfp_info.dom;
  mock_sfp_dom_info->temp = 123;
  mock_sfp_dom_info->nchannels = 2;
  mock_sfp_dom_info->voltage = 234;
  mock_sfp_dom_info->channels[0].tx_power = 1111;
  mock_sfp_dom_info->channels[0].rx_power = 2222;
  mock_sfp_dom_info->channels[0].bias_cur = 3333;
  mock_sfp_dom_info->channels[1].tx_power = 4444;
  mock_sfp_dom_info->channels[1].rx_power = 5555;
  mock_sfp_dom_info->channels[1].bias_cur = 6666;
  EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(oid_))
      .WillRepeatedly(Return(SfpInfo(mock_sfp_info)));

  ::util::StatusOr<std::shared_ptr<OnlpSfpDataSource>> result =
      OnlpSfpDataSource::Make(id_, onlp_wrapper_mock_.get(), nullptr);
  ASSERT_OK(result);
  std::shared_ptr<OnlpSfpDataSource> sfp_datasource =
      result.ConsumeValueOrDie();
  EXPECT_NE(sfp_datasource.get(), nullptr);

  // Update value and check attribute fields.
  EXPECT_OK(sfp_datasource->UpdateValuesUnsafelyWithoutCacheOrLock());
  EXPECT_THAT(sfp_datasource->GetSfpVendor(),
              ContainsValue<std::string>("test_sfp_vendor"));
  EXPECT_THAT(sfp_datasource->GetSfpModel(),
              ContainsValue<std::string>("test_sfp_model"));
  EXPECT_THAT(sfp_datasource->GetSfpSerialNumber(),
              ContainsValue<std::string>("test_sfp_serial"));
  EXPECT_THAT(sfp_datasource->GetSfpId(), ContainsValue<int>(id_));
  // Convert 0.1uW to dBm unit.
  EXPECT_THAT(sfp_datasource->GetSfpTxPower(0),
              ContainsValue<double>(10 * log10(1111.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpRxPower(0),
              ContainsValue<double>(10 * log10(2222.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpTxBias(0),
              ContainsValue<double>(3333.0 / 500.0));
  EXPECT_THAT(sfp_datasource->GetSfpTxPower(1),
              ContainsValue<double>(10 * log10(4444.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpRxPower(1),
              ContainsValue<double>(10 * log10(5555.0 / (10.0 * 1000.0))));
  EXPECT_THAT(sfp_datasource->GetSfpTxBias(1),
              ContainsValue<double>(6666.0 / 500.0));
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
  EXPECT_THAT(
      sfp_datasource->GetSfpType(),
      ContainsValue(SfpType_descriptor()->FindValueByName("SFP_TYPE_SFP")));
  EXPECT_THAT(sfp_datasource->GetSfpModuleType(),
              ContainsValue(SfpModuleType_descriptor()->FindValueByName(
                  "SFP_MODULE_TYPE_1G_BASE_SX")));

  // Check Module Caps
  EXPECT_THAT(sfp_datasource->GetModCapF100(), ContainsValue(false));
  EXPECT_THAT(sfp_datasource->GetModCapF1G(), ContainsValue(true));
  EXPECT_THAT(sfp_datasource->GetModCapF10G(), ContainsValue(false));
  EXPECT_THAT(sfp_datasource->GetModCapF40G(), ContainsValue(false));
  EXPECT_THAT(sfp_datasource->GetModCapF100G(), ContainsValue(true));

  EXPECT_THAT(sfp_datasource->GetSfpCableLength(), ContainsValue<int>(100));
  EXPECT_THAT(sfp_datasource->GetSfpCableLengthDesc(),
              ContainsValue<std::string>("test_cable_len"));
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
