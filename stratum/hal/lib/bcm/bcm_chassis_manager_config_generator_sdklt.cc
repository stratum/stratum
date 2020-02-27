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

#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

#include <algorithm>
#include <set>
#include <sstream>  // IWYU pragma: keep

#include "gflags/gflags.h"
#include "google/protobuf/message.h"
#include "stratum/glue/integral_types.h"
#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "yaml-cpp/yaml.h"

DECLARE_string(bcm_sdk_config_file);

namespace stratum {
namespace hal {
namespace bcm {
::util::Status BcmChassisManager::WriteBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) const {
  std::stringstream buffer;
  const size_t max_num_units = base_bcm_chassis_map.bcm_chips_size();

  // PC_PM Table
  YAML::Emitter pc_pm;
  pc_pm << YAML::BeginDoc;
  pc_pm << YAML::BeginMap;
  pc_pm << YAML::Key << "device";
  pc_pm << YAML::Value << YAML::BeginMap;
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_pm << YAML::Key << unit;
    pc_pm << YAML::Value << YAML::BeginMap;
    pc_pm << YAML::Key << "PC_PM";
    pc_pm << YAML::Value << YAML::BeginMap;
    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      // Key is a map (PC_PM_ID: serdes_core)
      pc_pm << YAML::Key << YAML::BeginMap << YAML::Key << "PC_PM_ID"
            << YAML::Value << bcm_port.serdes_core() << YAML::EndMap;

      pc_pm << YAML::Value << YAML::BeginMap;

      pc_pm << YAML::Key << "PM_OPMODE" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << "PC_PM_OPMODE_DEFAULT" << YAML::EndSeq;

      // TODO(max): SPEED_MAX has to be set to the highest supported value, else
      // speed changes are not possible at runtime. We set it to 100G for now.
      pc_pm << YAML::Key << "SPEED_MAX" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << 100000 << 0 << 0 << 0 << YAML::EndSeq;

      pc_pm << YAML::Key << "LANE_MAP" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << YAML::Hex << 0xf << 0 << 0 << 0 << YAML::Dec
            << YAML::EndSeq;

      pc_pm << YAML::EndMap;  // PC_PM_ID
    }
    pc_pm << YAML::EndMap;  // PC_PM
    pc_pm << YAML::EndMap;  // <unit>
  }
  pc_pm << YAML::EndMap;  // device
  pc_pm << YAML::EndDoc;
  buffer << pc_pm.c_str() << "\n";

  // PC_PM_CORE
  YAML::Emitter pc_pm_core;
  pc_pm_core << YAML::BeginDoc;
  pc_pm_core << YAML::BeginMap;
  pc_pm_core << YAML::Key << "device";
  pc_pm_core << YAML::Value << YAML::BeginMap;
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_pm_core << YAML::Key << unit;
    pc_pm_core << YAML::Value << YAML::BeginMap;
    pc_pm_core << YAML::Key << "PC_PM_CORE";
    pc_pm_core << YAML::Value << YAML::BeginMap;
    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      if (bcm_port.tx_lane_map() || bcm_port.rx_lane_map() ||
          bcm_port.tx_polarity_flip() || bcm_port.rx_polarity_flip()) {
        // Key is a map (PC_PM_ID: serdes_core, CORE_INDEX: unit)
        pc_pm_core << YAML::Key << YAML::BeginMap << YAML::Key << "PC_PM_ID"
                   << YAML::Value << bcm_port.serdes_core() << YAML::Key
                   << "CORE_INDEX" << YAML::Value << bcm_port.unit()
                   << YAML::EndMap;

        pc_pm_core << YAML::Value << YAML::BeginMap;

        if (bcm_port.tx_lane_map()) {
          pc_pm_core << YAML::Key << "TX_LANE_MAP" << YAML::Value
                     << bcm_port.tx_lane_map();
        }

        if (bcm_port.rx_lane_map()) {
          pc_pm_core << YAML::Key << "RX_LANE_MAP" << YAML::Value
                     << bcm_port.rx_lane_map();
        }

        if (bcm_port.tx_polarity_flip()) {
          pc_pm_core << YAML::Key << "TX_POLARITY_FLIP" << YAML::Value
                     << bcm_port.tx_polarity_flip();
        }

        if (bcm_port.tx_polarity_flip()) {
          pc_pm_core << YAML::Key << "RX_POLARITY_FLIP" << YAML::Value
                     << bcm_port.rx_polarity_flip();
        }

        pc_pm_core << YAML::EndMap;
      }
    }
    pc_pm_core << YAML::EndMap;  // PC_PM_CORE
    pc_pm_core << YAML::EndMap;  // <unit>
  }
  pc_pm_core << YAML::EndMap;  // device
  pc_pm_core << YAML::EndDoc;
  buffer << pc_pm_core.c_str() << "\n";

  // TODO(Yi): PC_PM_TX_LANE_PROFILE from serdes db.
  //  Note: PC_PM_LANE depends on PC_PM_TX_LANE_PROFILE
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
  //    for (int lane_id = 0; lane_id < bcm_port.num_serdes_lanes(); lane_id++)
  //    {
  //
  //      // Key is a map (PC_PM_ID: xx, CORE_INDEX: unit, CORE_LANE: xx)
  //      pc_pm_lane << YAML::Key << YAML::BeginMap
  //                 << YAML::Key << "PC_PM_ID" << YAML::Value << pc_pm_id
  //                 << YAML::Key << "CORE_INDEX"
  //                 << YAML::Value << bcm_port.unit()
  //                 << YAML::Key << "CORE_LANE" << YAML::Value << lane_id
  //                 << YAML::EndMap;
  //
  //      // TODO(Yi): Support multiple op mode and profile
  //      pc_pm_lane << YAML::Value << YAML::BeginMap;
  //      pc_pm_lane << YAML::Key << "PORT_OPMODE";
  //      pc_pm_lane << YAML::Value << YAML::Flow
  //                 << YAML::BeginSeq << "PC_PORT_OPMODE_ANY" << YAML::EndSeq;
  //      pc_pm_lane << YAML::Key << "PC_PM_TX_LANE_PROFILE_ID";
  //      pc_pm_lane << YAML::Value << YAML::Flow
  //                 << YAML::BeginSeq << tx_lane_profile_id << YAML::EndSeq;
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
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_port << YAML::Key << unit;
    pc_port << YAML::Value << YAML::BeginMap;
    pc_port << YAML::Key << "PC_PORT";
    pc_port << YAML::Value << YAML::BeginMap;

    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      // Key is a map (PORT_ID: logical_port)
      pc_port << YAML::Key << YAML::BeginMap << YAML::Key << "PORT_ID"
              << YAML::Value << bcm_port.logical_port() << YAML::EndMap;
      pc_port << YAML::Value << YAML::BeginMap << YAML::Key << "PC_PHYS_PORT_ID"
              << YAML::Value << bcm_port.physical_port() << YAML::Key
              << "ENABLE" << YAML::Value << 1 << YAML::Key << "OPMODE"
              << YAML::Value
              << absl::StrCat("PC_PORT_OPMODE_",
                              bcm_port.speed_bps() / kBitsPerGigabit, "G")
              << YAML::EndMap;  // PORT_ID
    }
    pc_port << YAML::EndMap;  // PC_PORT
    pc_port << YAML::EndMap;  // <unit>
  }
  pc_port << YAML::EndMap;  // device
  pc_port << YAML::EndDoc;
  buffer << pc_port.c_str() << "\n";

  return WriteStringToFile(buffer.str(), FLAGS_bcm_sdk_config_file);
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
