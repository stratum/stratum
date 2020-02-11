// Copyright 2019 Dell EMC
// Copyright 2020 Open Networking Foundation
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

#include "stratum/hal/lib/phal/onlp/onlp_switch_configurator.h"

#include <fstream>
#include <iostream>
#include <string>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/onlp/onlp_phal_mock.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {
namespace {

using test_utils::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::Return;
using ::testing::StrictMock;

class OnlpSwitchConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_group_ = AttributeGroup::From(PhalDB::descriptor());
    onlp_wrapper_mock_ = absl::make_unique<OnlpWrapperMock>();
    onlp_phal_mock_ = absl::make_unique<OnlpPhalMock>();
    ASSERT_OK_AND_ASSIGN(
        configurator_, OnlpSwitchConfigurator::Make(onlp_phal_mock_.get(),
                                                    onlp_wrapper_mock_.get()));
  }

  std::unique_ptr<AttributeGroup> root_group_;
  std::unique_ptr<OnlpWrapperMock> onlp_wrapper_mock_;
  std::unique_ptr<OnlpPhalMock> onlp_phal_mock_;
  std::unique_ptr<OnlpSwitchConfigurator> configurator_;
  PhalInitConfig config_;

  ::util::Status PopulatePhalInitConfig(PhalInitConfig* config) {
    RETURN_IF_ERROR(ParseProtoFromString(kPhalInitConfig, config));
    return ::util::OkStatus();
  }

  static constexpr char kPhalInitConfig[] = R"PROTO(
    cards {
      slot: 1
      ports { port: 1 physical_port_type: PHYSICAL_PORT_TYPE_SFP_CAGE }
    }
    fan_trays {
      slot: 1
      fans {
        slot: 1
        cache_policy { type: FETCH_ONCE }
      }
    }
    psu_trays {
      psus {
        slot: 1
        cache_policy { type: NO_CACHE }
      }
    }
    led_groups {
      leds {
        led_index: 1
        cache_policy { type: NO_CACHE }
      }
    }
    thermal_groups {
      thermals {
        thermal_index: 1
        cache_policy { type: TIMED_CACHE timed_value: 2 }
      }
    }
  )PROTO";
};

constexpr char OnlpSwitchConfiguratorTest::kPhalInitConfig[];

TEST_F(OnlpSwitchConfiguratorTest, CanConfigurePhalDB) {
  PhalInitConfig config;
  ASSERT_OK(PopulatePhalInitConfig(&config));

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
          EXPECT_CALL(*onlp_wrapper_mock_,
                      GetOidInfo(ONLP_SFP_ID_CREATE(i + 1)))
              .Times(2)
              .WillRepeatedly(Return(OidInfo(mock_oid_info)));
          EXPECT_CALL(*onlp_wrapper_mock_,
                      GetSfpInfo(ONLP_SFP_ID_CREATE(i + 1)))
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
      EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(ONLP_FAN_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_wrapper_mock_, GetFanInfo(ONLP_FAN_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(FanInfo(mock_fan_info)));
    }
  }

  // Add Mock PSU GetOidInfo calls
  onlp_psu_info_t mock_psu_info;
  mock_psu_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalPsuTrayConfig& psu_tray : config_.psu_trays()) {
    for (int i = 0; i < psu_tray.psus_size(); i++) {
      EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(ONLP_PSU_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_wrapper_mock_, GetPsuInfo(ONLP_PSU_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(PsuInfo(mock_psu_info)));
    }
  }

  // Add Mock LED GetOidInfo calls
  onlp_led_info_t mock_led_info;
  mock_led_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalLedGroupConfig& led_group : config_.led_groups()) {
    for (int i = 0; i < led_group.leds_size(); i++) {
      EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(ONLP_LED_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_wrapper_mock_, GetLedInfo(ONLP_LED_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(LedInfo(mock_led_info)));
    }
  }

  // Add Mock Thermal GetOidInfo calls
  onlp_thermal_info_t mock_thermal_info;
  mock_thermal_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  for (const PhalThermalGroupConfig& thermal_group : config_.thermal_groups()) {
    for (int i = 0; i < thermal_group.thermals_size(); i++) {
      EXPECT_CALL(*onlp_wrapper_mock_,
                  GetOidInfo(ONLP_THERMAL_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(OidInfo(mock_oid_info)));
      EXPECT_CALL(*onlp_wrapper_mock_,
                  GetThermalInfo(ONLP_THERMAL_ID_CREATE(i + 1)))
          .Times(2)
          .WillRepeatedly(Return(ThermalInfo(mock_thermal_info)));
    }
  }

  ASSERT_OK(configurator_->ConfigurePhalDB(
      &config_,
      (AttributeGroup*)root_group_.get()));  // NOLINT
}
}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
