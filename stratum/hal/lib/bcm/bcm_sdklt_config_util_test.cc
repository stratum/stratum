/*
 * Copyright 2019-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <string>
#include "gtest/gtest.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm_sdklt_config_util.h"


namespace stratum {
namespace hal {
namespace bcm {

DECLARE_string(test_tmpdir);

const std::string kBaseConfigPath = "stratum/hal/lib/bcm/testdata/base_chassis_map.pb.txt";
const std::string kTargetConfigPath = "stratum/hal/lib/bcm/testdata/target_chassis_map.pb.txt";

// TODO(Yi): Should compare entire output yaml string, however, we cannot guarantee
//           that the order of port config for each table.
const std::vector<std::string> kExpectedStrings = {
    "PC_PM_ID: 12",
    "PC_PM_ID: 13",
    "PM_OPMODE: [PC_PM_OPMODE_DEFAULT, PC_PM_OPMODE_DEFAULT, PC_PM_OPMODE_DEFAULT, PC_PM_OPMODE_DEFAULT]",
    "SPEED_MAX: [100000, 0, 0, 0]",
    "LANE_MAP: [15, 0, 0, 0]",
    "RX_LANE_MAP: 4131",
    "RX_LANE_MAP: 4866",
    "TX_LANE_MAP: 306",
    "TX_LANE_MAP: 8961",
    "RX_POLARITY_FLIP: 12",
    "RX_POLARITY_FLIP: 13",
    "TX_POLARITY_FLIP: 14",
    "TX_POLARITY_FLIP: 2",
    "CORE_INDEX: 0",
    "PORT_ID: 50",
    "PORT_ID: 54",
    "PC_PHYS_PORT_ID: 49",
    "PC_PHYS_PORT_ID: 53",
    "OPMODE: PC_PORT_OPMODE_100G",
    "ENABLE: 1"
};

class BcmSdkConfigUtilTest : ::testing::Test {
  void SetUp() override {
  }
};

TEST(BcmSdkConfigUtilTest, GenerateYamlTest) {
  ::stratum::hal::BcmChassisMap base_chassis_map;
  ::stratum::hal::BcmChassisMap target_chassis_map;
  ASSERT_OK(ReadProtoFromTextFile(kBaseConfigPath, &base_chassis_map));
  ASSERT_OK(ReadProtoFromTextFile(kTargetConfigPath, &target_chassis_map));

  auto sdklt_config = GenerateBcmSdkltConfig(base_chassis_map, target_chassis_map);
  ASSERT_OK(sdklt_config);

  auto sdklt_config_string = sdklt_config.ValueOrDie();
  for (const auto& expected_string : kExpectedStrings) {
    EXPECT_THAT(sdklt_config_string, ::testing::HasSubstr(expected_string));
  }
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum