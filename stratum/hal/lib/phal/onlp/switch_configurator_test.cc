// Copyright 2019 Dell EMC
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

#include "stratum/hal/lib/phal/onlp/switch_configurator.h"

#include <fstream>
#include <iostream>
#include <string>

#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/onlp/onlpphal_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"

// Testing header files
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

int main(int argc, char** argv) {
  ::google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  InitGoogle(argv[0], &argc, &argv, true);
  return RUN_ALL_TESTS();
}

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using test_utils::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::Return;
using ::testing::StrictMock;

class OnlpSwitchConfiguratorTest : public ::testing::Test {
 public:
  void SetUp() override {
    root_group_ = AttributeGroup::From(PhalDB::descriptor());
    onlpphal_.InitializeOnlpInterface();
    onlp_interface_ = onlpphal_.GetOnlpInterface();
    ASSERT_OK_AND_ASSIGN(configurator_, OnlpSwitchConfigurator::Make(
                                            &onlpphal_, onlp_interface_));
  }
  std::unique_ptr<AttributeGroup> root_group_;
  OnlpPhalMock onlpphal_;
  MockOnlpWrapper* onlp_interface_;
  PhalInitConfig config_;
  std::unique_ptr<OnlpSwitchConfigurator> configurator_;
  string text_config_path = "phal_init_config.pb.txt";
};

namespace {

// Create a dummy PhalDB protobuf
::util::Status TestPhalInitConfig(PhalInitConfig* config) {
  // Create Card
  auto card = config->add_cards();

  // Ports
  int num_ports = 32;
  for (int i = 0; i < num_ports; i++) {
    auto port = card->add_ports();
    port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
    // Note: Transceiver added at runtime
  }

  // Create Fan Tray
  auto fantray = config->add_fan_trays();

  // Fans
  int num_fans = 10;
  for (int i = 0; i < num_fans; i++) {
    auto fan = fantray->add_fans();
    fan->set_id(i + 1);
    auto mutable_cache = fan->mutable_cache_policy();
    mutable_cache->set_type(FETCH_ONCE);
  }

  // Create Psu Tray
  auto psutray = config->add_psu_trays();

  // Fans
  int num_psus = 2;
  for (int i = 0; i < num_psus; i++) {
    auto psu = psutray->add_psus();
    psu->set_id(i + 1);
    auto mutable_cache = psu->mutable_cache_policy();
    mutable_cache->set_type(NO_CACHE);
    mutable_cache->set_timed_value(10);
  }

  // Create Led Group
  auto led_group = config->add_led_groups();

  // Leds
  int num_leds = 4;
  for (int i = 0; i < num_leds; i++) {
    auto led = led_group->add_leds();
    led->set_id(i + 1);
    auto mutable_cache = led->mutable_cache_policy();
    mutable_cache->set_type(NO_CACHE);
  }

  // Create Led Group
  auto thermal_group = config->add_thermal_groups();

  // Thermals
  int num_thermals = 10;
  for (int i = 0; i < num_thermals; i++) {
    auto thermal = thermal_group->add_thermals();
    // We'll use the default id generated
    // thermal->set_id(i+1);
    auto mutable_cache = thermal->mutable_cache_policy();
    mutable_cache->set_type(TIMED_CACHE);
    mutable_cache->set_timed_value(2);
  }

  return ::util::OkStatus();
}

TEST_F(OnlpSwitchConfiguratorTest, CanSaveConfigToTextFile) {
  ASSERT_OK(TestPhalInitConfig(&config_));
  ASSERT_OK(WriteProtoToTextFile(config_, text_config_path));
}

TEST_F(OnlpSwitchConfiguratorTest, CanLoadConfigFromTextFile) {
  PhalInitConfig config2;
  ASSERT_OK(TestPhalInitConfig(&config_));
  ASSERT_OK(ReadProtoFromTextFile(text_config_path, &config2));
  ASSERT_TRUE(
      google::protobuf::util::MessageDifferencer::Equals(config_, config2));
}

TEST_F(OnlpSwitchConfiguratorTest, CanConfigurePhalDB) {
  ASSERT_OK(TestPhalInitConfig(&config_));

  // GetOidInfo mock return
  onlp_oid_hdr_t mock_oid_info;
  mock_oid_info.status = ONLP_OID_STATUS_FLAG_PRESENT;

  // Add Mock Sfp Port GetOidInfo calls
  onlp_sfp_info_t mock_sfp_info;
  mock_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info.type = ONLP_SFP_TYPE_SFP;

  // SFF Diag info
  mock_sfp_info.dom.voltage = 12;
  mock_sfp_info.dom.nchannels = SFF_DOM_CHANNEL_COUNT_MAX;
  for (int i = 0; i < mock_sfp_info.dom.nchannels; i++) {
    mock_sfp_info.dom.channels[i].fields =
        (SFF_DOM_FIELD_FLAG_RX_POWER | SFF_DOM_FIELD_FLAG_TX_POWER |
         SFF_DOM_FIELD_FLAG_RX_POWER_OMA | SFF_DOM_FIELD_FLAG_VOLTAGE);
    mock_sfp_info.dom.channels[i].bias_cur = 2;
    mock_sfp_info.dom.channels[i].rx_power = 10;
    mock_sfp_info.dom.channels[i].rx_power_oma = 10;
    mock_sfp_info.dom.channels[i].tx_power = 10;
  }

  for (const PhalCardConfig& card : config_.cards()) {
    for (int i = 0; i < card.ports_size(); i++) {
      // SFP ports get added later dynamically
      switch (card.ports(i).physical_port_type()) {
        case PHYSICAL_PORT_TYPE_SFP_CAGE:
        case PHYSICAL_PORT_TYPE_QSFP_CAGE:
          EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_SFP_ID_CREATE(i + 1)))
              .Times(2)
              .WillRepeatedly(Return(OidInfo(mock_oid_info)));
          EXPECT_CALL(*onlp_interface_, GetSfpInfo(ONLP_SFP_ID_CREATE(i + 1)))
              .Times(2)
              .WillRepeatedly(Return(SfpInfo(mock_sfp_info)));
          break;

        // don't worry about other port types
        default:
          break;
      }
    }
  }

  // Add Mock Fan GetOidInfo calls
  onlp_fan_info_t mock_fan_info;
  mock_fan_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalFanTrayConfig& fan_tray : config_.fan_trays()) {
    for (int i = 0; i < fan_tray.fans_size(); i++) {
      EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_FAN_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_interface_, GetFanInfo(ONLP_FAN_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(FanInfo(mock_fan_info)));
    }
  }

  // Add Mock PSU GetOidInfo calls
  onlp_psu_info_t mock_psu_info;
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalPsuTrayConfig& psu_tray : config_.psu_trays()) {
    for (int i = 0; i < psu_tray.psus_size(); i++) {
      EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_PSU_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_interface_, GetPsuInfo(ONLP_PSU_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(PsuInfo(mock_psu_info)));
    }
  }

  // Add Mock LED GetOidInfo calls
  onlp_led_info_t mock_led_info;
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalLedGroupConfig& led_group : config_.led_groups()) {
    for (int i = 0; i < led_group.leds_size(); i++) {
      EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_LED_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_interface_, GetLedInfo(ONLP_LED_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(LedInfo(mock_led_info)));
    }
  }

  // Add Mock Thermal GetOidInfo calls
  onlp_thermal_info_t mock_thermal_info;
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalThermalGroupConfig& thermal_group : config_.thermal_groups()) {
    for (int i = 0; i < thermal_group.thermals_size(); i++) {
      EXPECT_CALL(*onlp_interface_, GetOidInfo(ONLP_THERMAL_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_interface_,
                  GetThermalInfo(ONLP_THERMAL_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));
    }
  }

  ASSERT_OK(configurator_->ConfigurePhalDB(
      config_,
      (AttributeGroup*)root_group_.get()));  // NOLINT
}

}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
