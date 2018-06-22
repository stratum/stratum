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


#include "stratum/hal/lib/phal/transceiver_datasources.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/fixed_stringsource.h"
#include "stratum/hal/lib/phal/test_util.h"
#include "gtest/gtest.h"
#include "absl/time/time.h"

namespace stratum {
namespace hal {
namespace phal {

using google::protobuf::EnumValueDescriptor;

class QSFPDataSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Set the time zone to UTC.
    tzname[0] = tzname[1] = const_cast<char*>("GMT");
    timezone = 0;
    daylight = 0;
    setenv("TZ", "UTC", 1);
  }

  ::util::Status ParseData(const std::string& data) {
    std::unique_ptr<StringSourceInterface> stringsource(
        new FixedStringSource(data));
    datasource_ = QSFPDataSource::Make(std::move(stringsource), new NoCache());
    ::util::Status status = datasource_->UpdateValuesAndLock();
    datasource_->Unlock();
    return status;
  }

  template <typename T>
  T ReadAttribute(const std::string& name) {
    return absl::get<T>(*datasource_->GetAttribute(name).ValueOrDie()->GetValue());
  }

  template <typename T>
  void CheckNumberedValues(const std::string& name_prefix, T val1, T val2,
                           T val3, T val4) {
    EXPECT_EQ(ReadAttribute<T>(name_prefix + "_1"), val1);
    EXPECT_EQ(ReadAttribute<T>(name_prefix + "_2"), val2);
    EXPECT_EQ(ReadAttribute<T>(name_prefix + "_3"), val3);
    EXPECT_EQ(ReadAttribute<T>(name_prefix + "_4"), val4);
  }

  void CheckNumberedDoubleValues(const std::string& name_prefix, double val1,
                                 double val2, double val3, double val4) {
    EXPECT_DOUBLE_EQ(ReadAttribute<double>(name_prefix + "_1"), val1);
    EXPECT_DOUBLE_EQ(ReadAttribute<double>(name_prefix + "_2"), val2);
    EXPECT_DOUBLE_EQ(ReadAttribute<double>(name_prefix + "_3"), val3);
    EXPECT_DOUBLE_EQ(ReadAttribute<double>(name_prefix + "_4"), val4);
  }

  void CheckAlarmFields(const std::string& name_prefix, bool low_warn,
                        bool high_warn, bool low_alarm, bool high_alarm) {
    EXPECT_EQ(ReadAttribute<bool>(name_prefix + "_low_warn"), low_warn);
    EXPECT_EQ(ReadAttribute<bool>(name_prefix + "_high_warn"), high_warn);
    EXPECT_EQ(ReadAttribute<bool>(name_prefix + "_low_alarm"), low_alarm);
    EXPECT_EQ(ReadAttribute<bool>(name_prefix + "_high_alarm"), high_alarm);
  }

  void CheckMediaType(MediaType media_type) {
    auto enum_value_attr = datasource_->GetMediaTypeAttribute()->GetValue();
    auto enum_value_ptr = enum_value_attr.get<const EnumValueDescriptor*>();
    ASSERT_TRUE(enum_value_ptr != nullptr);
    const EnumValueDescriptor* enum_value = *enum_value_ptr;
    ASSERT_TRUE(enum_value != nullptr);
    EXPECT_EQ(enum_value->number(), media_type);
  }

 protected:
  std::shared_ptr<QSFPDataSource> datasource_;
};

std::string MakeGoodQsfpEepromContents() {
  std::string data;
  data.resize(213);
  // Fill status info:
  data.replace(2, 1, 1, 0x00);   // Data NOT ready = 0
  data.replace(3, 1, 1, 0x5a);   // TX LOS = 1,0,1,0; RX LOS = 0,1,0,1
  data.replace(4, 1, 1, 0x5a);   // TX EQ Fault = 1,0,1,0; TX Fault = 0,1,0,1
  data.replace(5, 1, 1, 0x5a);   // TX CDR LOL = 1,0,1,0; RX CDR LOL = 0,1,0,1
  data.replace(6, 1, 1, 0xA0);   // Temp High Alarm and Warning
  data.replace(7, 1, 1, 0x50);   // VCC Low Alarm and Warning
  data.replace(9, 1, 1, 0xA0);   // RX1 Power High Alarm and Warning
  data.replace(10, 1, 1, 0x50);  // RX3 Power Low Alarm and Warning
  data.replace(11, 1, 1, 0x0A);  // TX2 Bias High Alarm and Warning
  data.replace(12, 1, 1, 0x05);  // TX4 Bias Low Alarm and Warning
  data.replace(13, 1, 1, 0x0A);  // TX2 Power High Alarm and Warning
  data.replace(14, 1, 1, 0x05);  // TX4 Power Low Alarm and Warning
  data.replace(22, 1, 1, 0xe7);  // MSB Temp = -25C
  data.replace(23, 1, 1, 0);     // LSB
  data.replace(26, 1, 1, 0x80);  // MSB VCC = 3.3V
  data.replace(27, 1, 1, 0xe8);  // LSB
  const char rx_power[] = {
      0xff, 0xff,  // Ch1 6553.5 uW
      0x7f, 0xff,  // Ch2 3276.7 uW
      0x3f, 0xff,  // Ch3 1638.3 uW
      0x1f, 0xff   // Ch4 819.1 uW
  };
  data.replace(34, sizeof(rx_power), rx_power, sizeof(rx_power));
  const char tx_bias[] = {
      0xff, 0xff,  // Ch1 131.07 mA
      0x7f, 0xff,  // Ch2 65.5 mA
      0x3f, 0xff,  // Ch3 32.7 mA
      0x1f, 0xff   // Ch4 16.3 mA
  };
  data.replace(42, sizeof(tx_bias), tx_bias, sizeof(tx_bias));
  const char tx_power[] = {
      0xff, 0xff,  // Ch1 6553.5 uW
      0x7f, 0xff,  // Ch2 3276.7 uW
      0x3f, 0xff,  // Ch3 1638.3 uW
      0x1f, 0xff   // Ch4 819.1 uW
  };
  data.replace(50, sizeof(tx_power), tx_power, sizeof(tx_power));
  data.replace(86, 1, 1, 0x0a);  // TX Disable = 0,1,0,1
  data.replace(88, 1, 1, 0x1b);  // TX Rate Select = 3,2,1,0
  // Fill hardware info:
  data.replace(127, 1, 1, 0);     // Page 00 mapped
  data.replace(128, 1, 1, 0x0D);  // QSFP+
  data.replace(130, 1, 1, 0x0C);  // MPO
  data.replace(148, 16, "Test-Vendor-----");
  data.replace(168, 16, "Test-Part-Number");
  data.replace(184, 1, 1, 0);  // Revision number 86
  data.replace(185, 1, 1, 86);
  data.replace(196, 16, "Test-Serial-Num-");
  data.replace(212, 6, "100107");
  return data;
}

TEST_F(QSFPDataSourceTest, CannotReadEmptyBuffer) {
  std::string empty = "";
  EXPECT_FALSE(ParseData(empty).ok());
}

TEST_F(QSFPDataSourceTest, CanReadGoodEEPROMContents) {
  std::string data = MakeGoodQsfpEepromContents();
  EXPECT_OK(ParseData(data));
}

TEST_F(QSFPDataSourceTest, GoodEEPROMContentsProduceCorrectValues) {
  std::string data = MakeGoodQsfpEepromContents();
  ASSERT_OK(ParseData(data));
  EXPECT_TRUE(ReadAttribute<bool>("data_ready"));
  CheckNumberedValues<bool>("tx_los", true, false, true, false);
  CheckNumberedValues<bool>("rx_los", false, true, false, true);
  CheckNumberedValues<bool>("tx_eq_fault", true, false, true, false);
  CheckNumberedValues<bool>("tx_fault", false, true, false, true);
  CheckNumberedValues<bool>("tx_cdr_lol", true, false, true, false);
  CheckNumberedValues<bool>("rx_cdr_lol", false, true, false, true);
  CheckAlarmFields("temperature", false, true, false, true);
  CheckAlarmFields("vcc", true, false, true, false);
  CheckAlarmFields("rx_power_1", false, true, false, true);
  CheckAlarmFields("rx_power_2", false, false, false, false);
  CheckAlarmFields("rx_power_3", true, false, true, false);
  CheckAlarmFields("rx_power_4", false, false, false, false);
  CheckAlarmFields("tx_bias_1", false, false, false, false);
  CheckAlarmFields("tx_bias_2", false, true, false, true);
  CheckAlarmFields("tx_bias_3", false, false, false, false);
  CheckAlarmFields("tx_bias_4", true, false, true, false);
  CheckAlarmFields("tx_power_1", false, false, false, false);
  CheckAlarmFields("tx_power_2", false, true, false, true);
  CheckAlarmFields("tx_power_3", false, false, false, false);
  CheckAlarmFields("tx_power_4", true, false, true, false);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("temperature"), -25);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("vcc"), 3.3);
  CheckNumberedDoubleValues("rx_power", 6553.5, 3276.7, 1638.3, 819.1);
  CheckNumberedDoubleValues("tx_bias", 131.07, 65.534, 32.766, 16.382);
  CheckNumberedDoubleValues("tx_power", 6553.5, 3276.7, 1638.3, 819.1);
  CheckNumberedValues<bool>("tx_disable", false, true, false, true);
  CheckNumberedValues<uint32>("tx_rate_select", 3, 2, 1, 0);
  EXPECT_EQ(ReadAttribute<std::string>("vendor_name"), "Test-Vendor-----");
  EXPECT_EQ(ReadAttribute<std::string>("part_number"), "Test-Part-Number");
  EXPECT_EQ(ReadAttribute<uint32>("revision_number"), 86);
  EXPECT_EQ(ReadAttribute<std::string>("serial_number"), "Test-Serial-Num-");
  EXPECT_EQ(ReadAttribute<uint32>("manufacture_date"), 1262822400);
}

TEST_F(QSFPDataSourceTest, CannotReadWhenNotPage0) {
  std::string data = MakeGoodQsfpEepromContents();
  data.replace(127, 1, 1, 2);  // Page 02 mapped
  EXPECT_FALSE(ParseData(data).ok());
}

TEST_F(QSFPDataSourceTest, CanReadMissingConnectorType) {
  std::string data = MakeGoodQsfpEepromContents();
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_UNKNOWN);
}

TEST_F(QSFPDataSourceTest, CanReadEthernetComplianceConnectorType) {
  std::string data = MakeGoodQsfpEepromContents();
  data.replace(131, 1, 1, 0x14);
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_SR4);
}

TEST_F(QSFPDataSourceTest, CanDisambiguateMediaBasedOnConnectorType) {
  std::string data = MakeGoodQsfpEepromContents();
  data.replace(131, 1, 1, 0x02);
  data.replace(130, 1, 1, 0x07);
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_LR4);
  data.replace(130, 1, 1, 0x0c);
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_PSM4);
}

TEST_F(QSFPDataSourceTest, UsesGooglePSM4AtLowRevisionCompliance) {
  std::string data = MakeGoodQsfpEepromContents();
  data.replace(131, 1, 1, 0x80);
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_PSM4);
}

TEST_F(QSFPDataSourceTest, UsesExtendedMediaAtHighRevisionCompliance) {
  std::string data = MakeGoodQsfpEepromContents();
  data.replace(1, 1, 1, 0x05);  // revision compliance >= 0x05
  data.replace(131, 1, 1, 0x80);
  data.replace(192, 1, 1, 0x08);
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_CCR4);
  data.replace(1, 1, 1, 0x06);  // revision compliance >= 0x05
  ASSERT_TRUE(ParseData(data).ok());
  CheckMediaType(MediaType::MEDIA_TYPE_QSFP_CCR4);
}

class SFPDataSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Set the time zone to UTC.
    tzname[0] = tzname[1] = const_cast<char*>("GMT");
    timezone = 0;
    daylight = 0;
    setenv("TZ", "UTC", 1);
  }

  ::util::Status ParseData(const std::string& data) {
    std::unique_ptr<StringSourceInterface> stringsource(
        new FixedStringSource(data));
    datasource_ = SFPDataSource::Make(std::move(stringsource), new NoCache());
    ::util::Status status = datasource_->UpdateValuesAndLock();
    datasource_->Unlock();
    return status;
  }

  template <typename T>
  T ReadAttribute(const std::string& name) {
    return *datasource_->GetAttribute(name).ValueOrDie()->GetValue().get<T>();
  }

  std::string MakeGoodSfpEepromContents() {
    std::string data;
    // Fill hardware info:
    data.resize(111);
    data.replace(0, 1, 1, 0x03);
    data.replace(20, 16, "Test-Vendor-----");
    data.replace(40, 16, "Test-Part-Number");
    data.replace(56, 1, 1, 0);
    data.replace(57, 1, 1, 0);
    data.replace(58, 1, 1, 0);
    data.replace(59, 1, 1, 36);
    data.replace(68, 16, "Test-Serial-Number--");
    data.replace(84, 6, "100107");
    // Fill status info:
    const char fake_sfp_eeprom_diag_data[] = {
        0xe7, 0,     // 96-97: Temperature = -25*C
        0x80, 0xe8,  // 98-99: Vcc = 3.3V
        0xc3, 0x8c,  // 100-101: TX Bias = 100.12mA
        0x04, 0xd2,  // 102-103: TX Power = 123.4 uW
        0xff, 0xff,  // 104-105: RX Power = 6553.5 uW
        0,    0,    0, 0,
        0x96  // 110: Enable the 5 status/control flags.
              // Data_Ready_Bar State (bit 0) is low(0) to indicate data_ready.
    };
    data.insert(96, fake_sfp_eeprom_diag_data,
                sizeof(fake_sfp_eeprom_diag_data));
    return data;
  }

 protected:
  std::shared_ptr<SFPDataSource> datasource_;
};

TEST_F(SFPDataSourceTest, CannotReadEmptyBuffer) {
  std::string empty = "";
  EXPECT_FALSE(ParseData(empty).ok());
}

TEST_F(SFPDataSourceTest, CanReadGoodEEPROMContents) {
  std::string data = MakeGoodSfpEepromContents();
  EXPECT_OK(ParseData(data));
}

TEST_F(SFPDataSourceTest, GoodEEPROMContentsProduceCorrectValues) {
  std::string data = MakeGoodSfpEepromContents();
  ASSERT_OK(ParseData(data));
  EXPECT_EQ(ReadAttribute<std::string>("vendor_name"), "Test-Vendor-----");
  EXPECT_EQ(ReadAttribute<std::string>("part_number"), "Test-Part-Number");
  EXPECT_EQ(ReadAttribute<uint32>("revision_number"), 36);
  EXPECT_EQ(ReadAttribute<std::string>("serial_number"), "Test-Serial-Numb");
  EXPECT_EQ(ReadAttribute<uint32>("manufacture_date"), 1262822400);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("temperature"), -25);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("vcc"), 3.3);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("tx_bias"), 100.12);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("tx_power"), 123.4);
  EXPECT_DOUBLE_EQ(ReadAttribute<double>("rx_power"), 6553.5);
  EXPECT_TRUE(ReadAttribute<bool>("data_ready"));
  EXPECT_TRUE(ReadAttribute<bool>("rx_los"));
  EXPECT_TRUE(ReadAttribute<bool>("tx_fault"));
  EXPECT_EQ(ReadAttribute<uint32>("rate_select"), 1);
  EXPECT_TRUE(ReadAttribute<bool>("tx_disable"));
}

TEST_F(SFPDataSourceTest, CannotReadWrongEEPROMType) {
  std::string data = MakeGoodSfpEepromContents();
  data.replace(0, 1, 1, 0xFF);  // Should be 0x03.
  EXPECT_FALSE(ParseData(data).ok());
}

class LightPeakDataSourceTest : public ::testing::Test {
 public:
  ::util::Status ParseData(const std::string& data) {
    std::unique_ptr<StringSourceInterface> stringsource(
        new FixedStringSource(data));
    datasource_ =
        LightPeakDataSource::Make(std::move(stringsource), new NoCache());
    ::util::Status status = datasource_->UpdateValuesAndLock();
    datasource_->Unlock();
    return status;
  }

  std::string MakeGoodLightPeakEepromContents() {
    std::string data;
    data.resize(22);
    data.replace(6, 1, 1, 0x6A);     // Vendor OUI
    data.replace(7, 1, 1, 0x17);     // Vendor OUI
    data.replace(8, 1, 1, 0x00);     // Vendor OUI
    data.replace(9, 1, 1, 0xFF);     // Partnumber LSB
    data.replace(10, 1, 1, 0x00);    // Partnumber
    data.replace(11, 1, 1, 0x32);    // Partnumber
    data.replace(12, 1, 1, 0x50);    // Partnumber MSB
    data.replace(13, 1, 1, 0x44);    // Vendor Revision
    data.replace(14, 1, 1, 0x01);    // Date Code Year
    data.replace(15, 1, 1, 0x39);    // Date Code Week
    data.replace(18, 1, 1, 0x20);    // Vendor Serial Number LSB
    data.replace(19, 1, 1, 0x30);    // Vendor Serial Number
    data.replace(20, 1, 1, 0x27);    // Vendor Serial Number MSB
    return data;
  }

 protected:
  std::shared_ptr<LightPeakDataSource> datasource_;
};

TEST_F(LightPeakDataSourceTest, CannotReadEmptyBuffer) {
  std::string empty = "";
  EXPECT_FALSE(ParseData(empty).ok());
}

TEST_F(LightPeakDataSourceTest, CanReadGoodEEPROMContents) {
  std::string data = MakeGoodLightPeakEepromContents();
  EXPECT_OK(ParseData(data));
}

TEST_F(LightPeakDataSourceTest, GoodEEPROMContentsProduceCorrectValues) {
  std::string data = MakeGoodLightPeakEepromContents();
  ASSERT_OK(ParseData(data));
  EXPECT_THAT(datasource_->GetManufacturerNameAttribute(),
              ContainsValue<std::string>("Avago"));
  EXPECT_THAT(datasource_->GetPartNumberAttribute(),
              ContainsValue<std::string>("AFBR-75RG52Z"));
  EXPECT_THAT(datasource_->GetSerialNumberAttribute(),
              ContainsValue<std::string>("2568224"));
  uint32 min_date = absl::ToUnixSeconds(
      absl::FromDateTime(2001, 1, 39 * 7, 0, 0, 0, absl::UTCTimeZone()));
  uint32 max_date = absl::ToUnixSeconds(
      absl::FromDateTime(2011, 1, 40 * 7, 0, 0, 0, absl::UTCTimeZone()));
  EXPECT_THAT(datasource_->GetManufactureDateAttribute(),
              ContainsValueInRange<uint32>(min_date, max_date));

  EXPECT_THAT(datasource_->GetAttribute("revision_number"),
              IsOkAndContainsValue<uint32>(0x44));
}

TEST_F(LightPeakDataSourceTest, SaeEepromContentsReadCorrectly) {
  std::string data = MakeGoodLightPeakEepromContents();
  // Replace vendor oui and part number if SAE values.
  data.replace(6, 1, 1, 0x1f);     // Vendor OUI
  data.replace(7, 1, 1, 0x26);     // Vendor OUI
  data.replace(8, 1, 1, 0x00);     // Vendor OUI
  data.replace(9, 1, 1, 0x01);     // Partnumber LSB
  data.replace(10, 1, 1, 0x70);    // Partnumber
  data.replace(11, 1, 1, 0x11);    // Partnumber
  data.replace(12, 1, 1, 0x08);    // Partnumber MSB
  ASSERT_OK(ParseData(data));
  EXPECT_THAT(datasource_->GetManufacturerNameAttribute(),
              ContainsValue<std::string>("SAE"));
  EXPECT_THAT(datasource_->GetPartNumberAttribute(),
              ContainsValue<std::string>("7120-004-01"));
}

TEST_F(LightPeakDataSourceTest, OtherVendorEepromContentsReadCorrectly) {
  std::string data = MakeGoodLightPeakEepromContents();
  // Replace vendor oui and part number if SAE values.
  data.replace(6, 1, 1, 0x11);     // Vendor OUI
  data.replace(7, 1, 1, 0x22);     // Vendor OUI
  data.replace(8, 1, 1, 0x33);     // Vendor OUI
  data.replace(9, 1, 1, 0x44);     // Partnumber LSB
  data.replace(10, 1, 1, 0x55);    // Partnumber
  data.replace(11, 1, 1, 0x66);    // Partnumber
  data.replace(12, 1, 1, 0x77);    // Partnumber MSB
  ASSERT_OK(ParseData(data));
  EXPECT_THAT(datasource_->GetManufacturerNameAttribute(),
              ContainsValue<std::string>("\x33\x22\x11"));
  EXPECT_THAT(datasource_->GetPartNumberAttribute(),
              ContainsValue<std::string>("\x77\x66\x55\x44"));
}

}  // namespace phal
}  // namespace hal
}  // namespace stratum
