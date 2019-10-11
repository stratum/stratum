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

#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace bcm {

::util::StatusOr<std::string> GenerateBcmSdkltConfig(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) {
  std::stringstream buffer;

  // Here we are reusing the BcmPort data structure to store info we need.
  absl::flat_hash_map<int, BcmPort> serdes_core_info;
  for (auto bcm_port : base_bcm_chassis_map.bcm_ports()) {
    int serdes_core_id = bcm_port.serdes_core();

    auto stored_info = serdes_core_info.find(serdes_core_id);
    if(stored_info == serdes_core_info.end()) {
      serdes_core_info.emplace(serdes_core_id, bcm_port);
      continue;
    }

    // Stores the maximum speed
    if (bcm_port.speed_bps() > stored_info->second.speed_bps()) {
      stored_info->second.set_speed_bps(bcm_port.speed_bps());
    }

    // Stores the maximum number of serdes lanes
    if (bcm_port.num_serdes_lanes() > stored_info->second.num_serdes_lanes()) {
      stored_info->second.set_num_serdes_lanes(bcm_port.num_serdes_lanes());
    }
  }

  // TODO(Yi): We use default node Id 0, need to support multiple nodes.
  // PC_PM
  YAML::Emitter pc_pm;
  pc_pm << YAML::BeginDoc;
  pc_pm << YAML::BeginMap;
  pc_pm << YAML::Key << "device";
  pc_pm << YAML::Value << YAML::BeginMap;
  pc_pm << YAML::Key << "0";
  pc_pm << YAML::Value << YAML::BeginMap;
  pc_pm << YAML::Key << "PC_PM";
  pc_pm << YAML::Value << YAML::BeginMap;

  for (auto iter = serdes_core_info.begin();
       iter != serdes_core_info.end(); ++iter) {
    int serdes_core_id = iter->first;
    auto bcm_port = iter->second;

    uint64 speed_bps = bcm_port.speed_bps();
    // Key is a map (PC_PM_ID: xx)
    pc_pm << YAML::Key << YAML::BeginMap
          << YAML::Key << "PC_PM_ID" << YAML::Value << serdes_core_id
          << YAML::EndMap;
    pc_pm << YAML::Value << YAML::BeginMap;

    pc_pm << YAML::Key << "PM_OPMODE";
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq << "PC_PM_OPMODE_DEFAULT" << YAML::EndSeq;

    pc_pm << YAML::Key << "SPEED_MAX";  // TODO(Yi): Support multiple lane speed
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq << speed_bps/1000000 << 0 << 0 << 0 << YAML::EndSeq;

    pc_pm << YAML::Key << "LANE_MAP";  // TODO(Yi): Support multiple lane bits
    pc_pm << YAML::Value << YAML::Flow << YAML::BeginSeq << 0xf << 0 << 0 << 0 << YAML::EndSeq;

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

  for (auto iter = serdes_core_info.begin();
       iter != serdes_core_info.end(); ++iter) {
    int serdes_core_id = iter->first;
    auto bcm_port = iter->second;

    if (!bcm_port.rx_lane_map() && !bcm_port.tx_lane_map() &&
        !bcm_port.rx_polarity_flip() && !bcm_port.tx_polarity_flip()) {
      // Skip this port, since there is not config fot it.
      continue;
    }

    // TODO(Yi): Currently we are using default CORE_INDEX.
    // Key is a map (PC_PM_ID: xx, CORE_INDEX: 0)
    pc_pm_core << YAML::Key << YAML::BeginMap
               << YAML::Key << "PC_PM_ID" << YAML::Value << serdes_core_id
               << YAML::Key << "CORE_INDEX" << YAML::Value << 0
               << YAML::EndMap;

    pc_pm_core << YAML::Value << YAML::BeginMap;
    if (bcm_port.rx_lane_map()) {
      pc_pm_core << YAML::Key << "RX_LANE_MAP";
      pc_pm_core << YAML::Value << bcm_port.rx_lane_map();
    }

    if (bcm_port.tx_lane_map()) {
      pc_pm_core << YAML::Key << "TX_LANE_MAP";
      pc_pm_core << YAML::Value << bcm_port.tx_lane_map();
    }

    if (bcm_port.rx_polarity_flip()) {
      pc_pm_core << YAML::Key << "RX_POLARITY_FLIP";
      pc_pm_core << YAML::Value << bcm_port.rx_polarity_flip();
    }

    if (bcm_port.tx_polarity_flip()) {
      pc_pm_core << YAML::Key << "TX_POLARITY_FLIP";
      pc_pm_core << YAML::Value << bcm_port.tx_polarity_flip();
    }
    pc_pm_core << YAML::EndMap;
  }
  pc_pm_core << YAML::EndMap;  // PC_PM_CORE
  pc_pm_core << YAML::EndMap;  // 0
  pc_pm_core << YAML::EndMap;  // device
  pc_pm_core << YAML::EndDoc;
  buffer << pc_pm_core.c_str() << "\n";

  // PC_PM_LANE
  YAML::Emitter pc_pm_lane;
  pc_pm_lane << YAML::BeginDoc;
  pc_pm_lane << YAML::BeginMap;
  pc_pm_lane << YAML::Key << "device";
  pc_pm_lane << YAML::Value << YAML::BeginMap;
  pc_pm_lane << YAML::Key << "0";
  pc_pm_lane << YAML::Value << YAML::BeginMap;
  pc_pm_lane << YAML::Key << "PC_PM_LANE";
  pc_pm_lane << YAML::Value << YAML::BeginMap;

  for (auto iter = serdes_core_info.begin();
       iter != serdes_core_info.end(); ++iter) {
    int serdes_core_id = iter->first;
    auto bcm_port = iter->second;

    for (int lane_id = 0; lane_id < bcm_port.num_serdes_lanes(); lane_id++) {
      // TODO(Yi): Currently we are using default CORE_INDEX and PORT_OPMODE.
      // Key is a map (PC_PM_ID: xx, CORE_INDEX: 0, CORE_LANE: xx)
      pc_pm_lane << YAML::Key << YAML::BeginMap
                 << YAML::Key << "PC_PM_ID" << YAML::Value << serdes_core_id
                 << YAML::Key << "CORE_INDEX" << YAML::Value << 0
                 << YAML::Key << "CORE_LANE" << YAML::Value << lane_id
                 << YAML::EndMap;

      pc_pm_lane << YAML::Value << YAML::BeginMap;
      pc_pm_lane << YAML::Key << "PORT_OPMODE";
      pc_pm_lane << YAML::Value << YAML::Flow << YAML::BeginSeq << "PC_PORT_OPMODE_ANY" << YAML::EndSeq;
      pc_pm_lane << YAML::EndMap;  // PORT_OPMODE
    }
  }
  pc_pm_lane << YAML::EndMap;  // PC_PM_LANE
  pc_pm_lane << YAML::EndMap;  // 0
  pc_pm_lane << YAML::EndMap;  // device
  pc_pm_lane << YAML::EndDoc;
  buffer << pc_pm_lane.c_str() << "\n";

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

  for (auto bcm_port : target_bcm_chassis_map.bcm_ports()) {
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