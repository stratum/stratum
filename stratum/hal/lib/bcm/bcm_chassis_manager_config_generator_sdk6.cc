// Copyright 2018 Google LLC
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

// This file implements the BcmChassisManager::WriteBcmConfigFile function for
// Broadcom SDK6.

#include <algorithm>
#include <set>
#include <sstream>  // IWYU pragma: keep

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "google/protobuf/message.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/public/lib/error.h"

DECLARE_string(bcm_sdk_config_file);

namespace stratum {
namespace hal {
namespace bcm {

::util::Status BcmChassisManager::WriteBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) const {
  std::stringstream buffer;

  // initialize the port mask. The total number of chips supported comes from
  // base_bcm_chassis_map.
  const size_t max_num_units = base_bcm_chassis_map.bcm_chips_size();
  std::vector<uint64> xe_pbmp_mask0(max_num_units, 0);
  std::vector<uint64> xe_pbmp_mask1(max_num_units, 0);
  std::vector<uint64> xe_pbmp_mask2(max_num_units, 0);
  std::vector<bool> is_chip_oversubscribed(max_num_units, false);

  // Chassis-level SDK properties.
  if (target_bcm_chassis_map.has_bcm_chassis()) {
    const auto& bcm_chassis = target_bcm_chassis_map.bcm_chassis();
    for (const std::string& sdk_property : bcm_chassis.sdk_properties()) {
      buffer << sdk_property << std::endl;
    }
    // In addition to SDK properties in the config, in the sim mode we need to
    // also add properties to disable DMA.
    if (mode_ == OPERATION_MODE_SIM) {
      buffer << "tdma_intr_enable=0" << std::endl;
      buffer << "tslam_dma_enable=0" << std::endl;
      buffer << "table_dma_enable=0" << std::endl;
    }
    buffer << std::endl;
  }

  // Chip-level SDK properties.
  for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
    int unit = bcm_chip.unit();
    if (bcm_chip.sdk_properties_size()) {
      for (const std::string& sdk_property : bcm_chip.sdk_properties()) {
        buffer << sdk_property << std::endl;
      }
      buffer << std::endl;
    }
    if (bcm_chip.is_oversubscribed()) {
      is_chip_oversubscribed[unit] = true;
    }
  }

  // XE port maps.
  // TODO(unknown): See if there is some BCM macros to work with pbmp's.
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    if (bcm_port.type() == BcmPort::XE || bcm_port.type() == BcmPort::CE) {
      int idx = bcm_port.logical_port();
      int unit = bcm_port.unit();
      if (idx < 64) {
        xe_pbmp_mask0[unit] |= static_cast<uint64>(0x01) << idx;
      } else if (idx < 128) {
        xe_pbmp_mask1[unit] |= static_cast<uint64>(0x01) << (idx - 64);
      } else {
        xe_pbmp_mask2[unit] |= static_cast<uint64>(0x01) << (idx - 128);
      }
    }
  }
  for (size_t i = 0; i < max_num_units; ++i) {
    if (xe_pbmp_mask1[i] || xe_pbmp_mask0[i] || xe_pbmp_mask2[i]) {
      std::stringstream mask(std::stringstream::in | std::stringstream::out);
      std::stringstream t0(std::stringstream::in | std::stringstream::out);
      std::stringstream t1(std::stringstream::in | std::stringstream::out);
      if (xe_pbmp_mask2[i]) {
        t0 << std::hex << std::uppercase << xe_pbmp_mask0[i];
        t1 << std::hex << std::uppercase << xe_pbmp_mask1[i];
        mask << std::hex << std::uppercase << xe_pbmp_mask2[i]
             << std::string(2 * sizeof(uint64) - t1.str().length(), '0')
             << t1.str()
             << std::string(2 * sizeof(uint64) - t0.str().length(), '0')
             << t0.str();
      } else if (xe_pbmp_mask1[i]) {
        t0 << std::hex << std::uppercase << xe_pbmp_mask0[i];
        mask << std::hex << std::uppercase << xe_pbmp_mask1[i]
             << std::string(2 * sizeof(uint64) - t0.str().length(), '0')
             << t0.str();
      } else {
        mask << std::hex << std::uppercase << xe_pbmp_mask0[i];
      }
      buffer << "pbmp_xport_xe." << i << "=0x" << mask.str() << std::endl;
      if (is_chip_oversubscribed[i]) {
        buffer << "pbmp_oversubscribe." << i << "=0x" << mask.str()
               << std::endl;
      }
    }
  }
  buffer << std::endl;

  // Port properties. Before that we create a map from chip-type to
  // map of channel to speed_bps for the flex ports.
  std::map<BcmChip::BcmChipType, std::map<int, uint64>>
      flex_chip_to_channel_to_speed = {{BcmChip::TOMAHAWK,
                                        {{1, kHundredGigBps},
                                         {2, kTwentyFiveGigBps},
                                         {3, kFiftyGigBps},
                                         {4, kTwentyFiveGigBps}}},
                                       {BcmChip::TRIDENT2,
                                        {{1, kFortyGigBps},
                                         {2, kTenGigBps},
                                         {3, kTwentyGigBps},
                                         {4, kTenGigBps}}}};
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    uint64 speed_bps = 0;
    if (bcm_port.type() == BcmPort::XE || bcm_port.type() == BcmPort::CE ||
        bcm_port.type() == BcmPort::GE) {
      // Find the type of the chip hosting this port. Then find the speed
      // which we need to set in the config.bcm, which depends on whether
      // the port is flex or not. We dont use GetBcmChip as unit_to_bcm_chip_
      // may not be populated when this function is called.
      BcmChip::BcmChipType chip_type = BcmChip::UNKNOWN;
      for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
        if (bcm_chip.unit() == bcm_port.unit()) {
          chip_type = bcm_chip.type();
          break;
        }
      }
      if (bcm_port.flex_port()) {
        CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK ||
                              chip_type == BcmChip::TRIDENT2)
            << "Un-supported BCM chip type: "
            << BcmChip::BcmChipType_Name(chip_type);
        CHECK_RETURN_IF_FALSE(bcm_port.channel() >= 1 &&
                              bcm_port.channel() <= 4)
            << "Flex-port with no channel: " << bcm_port.ShortDebugString();
        speed_bps =
            flex_chip_to_channel_to_speed[chip_type][bcm_port.channel()];
      } else {
        speed_bps = bcm_port.speed_bps();
      }
    } else if (bcm_port.type() == BcmPort::MGMT) {
      CHECK_RETURN_IF_FALSE(!bcm_port.flex_port())
          << "Mgmt ports cannot be flex.";
      speed_bps = bcm_port.speed_bps();
    } else {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Un-supported BCM port type: " << bcm_port.type() << " in "
             << bcm_port.ShortDebugString();
    }

    // Port speed and diag port setting.
    buffer << "portmap_" << bcm_port.logical_port() << "." << bcm_port.unit()
           << "=" << bcm_port.physical_port() << ":"
           << speed_bps / kBitsPerGigabit;
    if (bcm_port.flex_port() && bcm_port.serdes_lane()) {
      buffer << ":i";
    }
    buffer << std::endl;
    buffer << "dport_map_port_" << bcm_port.logical_port() << "."
           << bcm_port.unit() << "=" << bcm_port.diag_port() << std::endl;
    // Lane remapping handling.
    if (bcm_port.tx_lane_map() > 0) {
      buffer << "xgxs_tx_lane_map_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.tx_lane_map() << std::dec << std::nouppercase
             << std::endl;
    }
    if (bcm_port.rx_lane_map() > 0) {
      buffer << "xgxs_rx_lane_map_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.rx_lane_map() << std::dec << std::nouppercase
             << std::endl;
    }
    // XE ports polarity flip handling for RX and TX.
    if (bcm_port.tx_polarity_flip() > 0) {
      buffer << "phy_xaui_tx_polarity_flip_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.tx_polarity_flip() << std::dec << std::nouppercase
             << std::endl;
    }
    if (bcm_port.rx_polarity_flip() > 0) {
      buffer << "phy_xaui_rx_polarity_flip_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.rx_polarity_flip() << std::dec << std::nouppercase
             << std::endl;
    }
    // Port-level SDK properties.
    if (bcm_port.sdk_properties_size()) {
      for (const std::string& sdk_property : bcm_port.sdk_properties()) {
        buffer << sdk_property << std::endl;
      }
    }
    buffer << std::endl;
  }

  RETURN_IF_ERROR(WriteStringToFile(buffer.str(), FLAGS_bcm_sdk_config_file));

  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
