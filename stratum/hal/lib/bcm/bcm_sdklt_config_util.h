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

#ifndef STRATUM_HAL_LIB_BCM_BCM_SDK_CONFIG_UTIL_H_
#define STRATUM_HAL_LIB_BCM_BCM_SDK_CONFIG_UTIL_H_

#include <string>

#include "yaml-cpp/yaml.h"
#include "absl/container/flat_hash_map.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/glue/integral_types.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/glue/status/status_macros.h"

namespace stratum {
namespace hal {
namespace bcm {

constexpr int kMaxSerdesLaneNumber = 15;

// Data structure to hold lane settings for BCM port macro
struct SerdesLaneSetting {
  uint32 speed_mbps[16];
  uint32 lane_map[16];
  uint64 tx_lane_map;
  uint64 rx_lane_map;
  uint64 rx_polarity_flip;
  uint64 tx_polarity_flip;
  BcmPort_OpMode op_mode[4];
};

const std::string kSdkltOpModeDefault = "PC_PM_OPMODE_DEFAULT";
const std::string kSdkltOpModeQsgmii = "PC_PM_OPMODE_QSGMII";
const std::string kSdkltOpModeGphy = "PC_PM_OPMODE_GPHY";

::util::StatusOr<std::string> toBcmSdkltOpModeStr(BcmPort_OpMode op_mode) {
  switch (op_mode) {
    case BcmPort_OpMode_OPMODE_DEFAULT:
      return kSdkltOpModeDefault;
    case BcmPort_OpMode_OPMODE_QSGMII:
      return kSdkltOpModeQsgmii;
    case BcmPort_OpMode_OPMODE_GPHY:
      return kSdkltOpModeGphy;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Unknown operation mode " << op_mode;
  }
};

::util::StatusOr<std::string> GenerateBcmSdkltConfig(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) {
  std::stringstream buffer;

  absl::flat_hash_map<int, SerdesLaneSetting*> serdes_lane_settings;
  for(const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    int serdes_core_id = bcm_port.serdes_core();

    if (!serdes_lane_settings.count(serdes_core_id)) {
      serdes_lane_settings.emplace(serdes_core_id, new SerdesLaneSetting());
    }

    SerdesLaneSetting* serdes_lane_setting = serdes_lane_settings[serdes_core_id];

    if (!serdes_lane_setting) {
      // This should not happened
      RETURN_ERROR() << "Serdes core id " << serdes_core_id << " not found.";
    }

    serdes_lane_setting->rx_lane_map = bcm_port.rx_lane_map();
    serdes_lane_setting->tx_lane_map = bcm_port.tx_lane_map();
    serdes_lane_setting->rx_polarity_flip = bcm_port.rx_polarity_flip();
    serdes_lane_setting->tx_polarity_flip = bcm_port.tx_polarity_flip();

    int serdes_lane = bcm_port.serdes_lane();
    CHECK_RETURN_IF_FALSE(serdes_lane >= 0 && serdes_lane < kMaxSerdesLaneNumber)
        << "Invalid serdes lane number";

    serdes_lane_setting->speed_mbps[serdes_lane] = bcm_port.speed_bps() / kBitsPerMegabit;
    serdes_lane_setting->lane_map[serdes_lane] = bcm_port.lane_map();
    serdes_lane_setting->op_mode[serdes_lane] = bcm_port.op_mode();
  }

  // TODO(Yi): We use default node Id 0, need to support multiple nodes.
  // PC_PM Table
  YAML::Emitter pc_pm;
  pc_pm << YAML::BeginDoc;
  pc_pm << YAML::BeginMap;
  pc_pm << YAML::Key << "device";
  pc_pm << YAML::Value << YAML::BeginMap;
  pc_pm << YAML::Key << "0";
  pc_pm << YAML::Value << YAML::BeginMap;
  pc_pm << YAML::Key << "PC_PM";
  pc_pm << YAML::Value << YAML::BeginMap;

  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    int pc_pm_id = bcm_port.serdes_core();
    SerdesLaneSetting* serdes_lane_setting = serdes_lane_settings[pc_pm_id];

    // Key is a map (PC_PM_ID: xx)
    pc_pm << YAML::Key << YAML::BeginMap
          << YAML::Key << "PC_PM_ID" << YAML::Value << pc_pm_id
          << YAML::EndMap;
    pc_pm << YAML::Value << YAML::BeginMap;

    pc_pm << YAML::Key << "PM_OPMODE";
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq;

    for (int i = 0; i < bcm_port.num_serdes_lanes(); i++) {
      ASSIGN_OR_RETURN(auto op_mode_str, toBcmSdkltOpModeStr(serdes_lane_setting->op_mode[i]))
      pc_pm << op_mode_str;
    }
    pc_pm << YAML::EndSeq;

    pc_pm << YAML::Key << "SPEED_MAX";
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < bcm_port.num_serdes_lanes(); i++) {
      pc_pm << serdes_lane_setting->speed_mbps[i];
    }
    pc_pm << YAML::EndSeq;

    pc_pm << YAML::Key << "LANE_MAP";
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < bcm_port.num_serdes_lanes(); i++) {
      pc_pm << serdes_lane_setting->lane_map[i];
    }
    pc_pm << YAML::EndSeq;
    pc_pm << YAML::EndMap;  // PC_PM_ID
  }
  pc_pm << YAML::EndMap;  // PC_PM
  pc_pm << YAML::EndMap;  // 0
  pc_pm << YAML::EndMap;  // device
  pc_pm << YAML::EndDoc;
  buffer << pc_pm.c_str() << "\n";


  // PC_PM_CORE
  YAML::Emitter pc_pm_core;
  pc_pm_core << YAML::BeginDoc;
  pc_pm_core << YAML::BeginMap;
  pc_pm_core << YAML::Key << "device";
  pc_pm_core << YAML::Value << YAML::BeginMap;
  pc_pm_core << YAML::Key << "0";
  pc_pm_core << YAML::Value << YAML::BeginMap;
  pc_pm_core << YAML::Key << "PC_PM_CORE";
  pc_pm_core << YAML::Value << YAML::BeginMap;

  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    int pc_pm_id = bcm_port.serdes_core();
    SerdesLaneSetting* serdes_lane_setting = serdes_lane_settings[pc_pm_id];

    // Key is a map (PC_PM_ID: xx, CORE_INDEX: unit)
    pc_pm_core << YAML::Key << YAML::BeginMap
               << YAML::Key << "PC_PM_ID" << YAML::Value << pc_pm_id
               << YAML::Key << "CORE_INDEX" << YAML::Value << bcm_port.unit()
               << YAML::EndMap;

    pc_pm_core << YAML::Value << YAML::BeginMap;
    pc_pm_core << YAML::Key << "RX_LANE_MAP";
    pc_pm_core << YAML::Value << serdes_lane_setting->rx_lane_map;

    pc_pm_core << YAML::Key << "TX_LANE_MAP";
    pc_pm_core << YAML::Value << serdes_lane_setting->tx_lane_map;

    pc_pm_core << YAML::Key << "RX_POLARITY_FLIP";
    pc_pm_core << YAML::Value << serdes_lane_setting->rx_polarity_flip;

    pc_pm_core << YAML::Key << "TX_POLARITY_FLIP";
    pc_pm_core << YAML::Value << serdes_lane_setting->tx_polarity_flip;
    pc_pm_core << YAML::EndMap;
  }
  pc_pm_core << YAML::EndMap;  // PC_PM_CORE
  pc_pm_core << YAML::EndMap;  // 0
  pc_pm_core << YAML::EndMap;  // device
  pc_pm_core << YAML::EndDoc;
  buffer << pc_pm_core.c_str() << "\n";

  // TODO(Yi): PC_PM_TX_LANE_PROFILE from serdes db or something else.
  // TODO(Yi) PC_PM_LANE, this depends on PC_PM_TX_LANE_PROFILE
//  YAML::Emitter pc_pm_lane;
//  pc_pm_lane << YAML::BeginDoc;
//  pc_pm_lane << YAML::BeginMap;
//  pc_pm_lane << YAML::Key << "device";
//  pc_pm_lane << YAML::Value << YAML::BeginMap;
//  pc_pm_lane << YAML::Key << "0";
//  pc_pm_lane << YAML::Value << YAML::BeginMap;
//  pc_pm_lane << YAML::Key << "PC_PM_LANE";
//  pc_pm_lane << YAML::Value << YAML::BeginMap;
//
//  for (auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
//    int pc_pm_id = bcm_port.serdes_core();
//
//    for (int lane_id = 0; lane_id < bcm_port.num_serdes_lanes(); lane_id++) {
//
//      // Key is a map (PC_PM_ID: xx, CORE_INDEX: unit, CORE_LANE: xx)
//      pc_pm_lane << YAML::Key << YAML::BeginMap
//                 << YAML::Key << "PC_PM_ID" << YAML::Value << pc_pm_id
//                 << YAML::Key << "CORE_INDEX" << YAML::Value << bcm_port.unit()
//                 << YAML::Key << "CORE_LANE" << YAML::Value << lane_id
//                 << YAML::EndMap;
//
//      // TODO(Yi): Support multiple op mode and profile
//      pc_pm_lane << YAML::Value << YAML::BeginMap;
//      pc_pm_lane << YAML::Key << "PORT_OPMODE";
//      pc_pm_lane << YAML::Value << YAML::Flow << YAML::BeginSeq << "PC_PORT_OPMODE_ANY" << YAML::EndSeq;
////      pc_pm_lane << YAML::Key << "PC_PM_TX_LANE_PROFILE_ID";
////      pc_pm_lane << YAML::Value << YAML::Flow << YAML::BeginSeq << tx_lane_profile_id << YAML::EndSeq;
//      pc_pm_lane << YAML::EndMap;  // PORT_OPMODE
//    }
//  }
//  pc_pm_lane << YAML::EndMap;  // PC_PM_LANE
//  pc_pm_lane << YAML::EndMap;  // 0
//  pc_pm_lane << YAML::EndMap;  // device
//  pc_pm_lane << YAML::EndDoc;
//  buffer << pc_pm_lane.c_str() << "\n";

  // PC_PORT
  YAML::Emitter pc_port;
  pc_port << YAML::BeginDoc;
  pc_port << YAML::BeginMap;
  pc_port << YAML::Key << "device";
  pc_port << YAML::Value << YAML::BeginMap;
  pc_port << YAML::Key << "0";
  pc_port << YAML::Value << YAML::BeginMap;
  pc_port << YAML::Key << "PC_PORT";
  pc_port << YAML::Value << YAML::BeginMap;

  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    // Key is a map (PORT_ID: xx)
    pc_port << YAML::Key << YAML::BeginMap
            << YAML::Key << "PORT_ID" << YAML::Value << bcm_port.logical_port()
            << YAML::EndMap;

    pc_port << YAML::Value << YAML::BeginMap;
    pc_port << YAML::Key << "PC_PHYS_PORT_ID";
    pc_port << YAML::Value << bcm_port.physical_port();
    pc_port << YAML::Key << "ENABLE";
    pc_port << YAML::Value << 1;
    pc_port << YAML::Key << "OPMODE";
    pc_port << YAML::Value << SpeedBpsToBcmPortSpeedStr(bcm_port.speed_bps());

    pc_port << YAML::EndMap;  // PORT_ID
  }
  pc_port << YAML::EndMap;  // PC_PORT
  pc_port << YAML::EndMap;  // 0
  pc_port << YAML::EndMap;  // device
  pc_port << YAML::EndDoc;
  buffer << pc_port.c_str() << "\n";

  return buffer.str();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SDK_CONFIG_UTIL_H_