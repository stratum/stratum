// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"

#include "gflags/gflags.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/integral_types.h"
#include "absl/memory/memory.h"

DEFINE_string(bcm_serdes_db_proto_file,
              "/etc/stratum/dummy_serdes_db.pb.txt",
              "Path to the location of file containing BcmSerdesDb proto in "
              "binary format can be found.");

namespace stratum {
namespace hal {
namespace bcm {

namespace {

// A helper that returns true if a given front panel port info and port speed
// matches a given BcmSerdesDbEntry.
static bool PortMatch(const FrontPanelPortInfo& fp_port_info, uint64 speed_bps,
                      const BcmSerdesDbEntry& serdes_db_entry) {
  if (serdes_db_entry.media_type() == fp_port_info.media_type() &&
      serdes_db_entry.vendor_name() == fp_port_info.vendor_name() &&
      serdes_db_entry.speed_bps() == speed_bps) {
    // See if we match any of the part_numbers in BcmSerdesDbEntry matches the
    // one in FrontPanelPortInfo. If there is no part_number, we make sure
    // BcmSerdesDbEntry does not have any part number either. An example of
    // such case is backplane ports in superchassis like BG16.
    if (fp_port_info.part_number().empty() &&
        serdes_db_entry.part_numbers_size() == 0) {
      return true;
    }
    for (const auto& part_number : serdes_db_entry.part_numbers()) {
      if (fp_port_info.part_number() == part_number) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

BcmSerdesDbManager::BcmSerdesDbManager() {}

BcmSerdesDbManager::~BcmSerdesDbManager() {}

::util::Status BcmSerdesDbManager::Load() {
  RETURN_IF_ERROR(
      ReadProtoFromBinFile(FLAGS_bcm_serdes_db_proto_file, &bcm_serdes_db_));
  return ::util::OkStatus();
}

::util::Status BcmSerdesDbManager::LookupSerdesConfigForPort(
    const BcmPort& bcm_port, const FrontPanelPortInfo& fp_port_info,
    BcmSerdesLaneConfig* bcm_serdes_lane_config) const {
  for (const auto& e : bcm_serdes_db_.bcm_serdes_db_entries()) {
    if (PortMatch(fp_port_info, bcm_port.speed_bps(), e)) {
      const auto& serdes_chip_configs =
          e.bcm_serdes_board_config().bcm_serdes_chip_configs();
      auto i = serdes_chip_configs.find(bcm_port.unit());
      CHECK_RETURN_IF_FALSE(i != serdes_chip_configs.end())
          << "Unit " << bcm_port.unit() << " not found in serdes DB for "
          << PrintBcmPort(bcm_port) << " with following front panel port info: "
          << fp_port_info.ShortDebugString();
      const auto& serdes_core_configs = i->second.bcm_serdes_core_configs();
      auto j = serdes_core_configs.find(bcm_port.serdes_core());
      CHECK_RETURN_IF_FALSE(j != serdes_core_configs.end())
          << "Serdes core " << bcm_port.serdes_core() << " not found in serdes "
          << "DB for " << PrintBcmPort(bcm_port) << " with following front "
          << "panel port info: " << fp_port_info.ShortDebugString();
      const auto& serdes_lane_configs = j->second.bcm_serdes_lane_configs();
      auto k = serdes_lane_configs.find(bcm_port.serdes_lane());
      CHECK_RETURN_IF_FALSE(k != serdes_lane_configs.end())
          << "Serdes lane " << bcm_port.serdes_lane() << " not found in "
          << "serdes DB for " << PrintBcmPort(bcm_port) << " with following "
          << "front panel port info: " << fp_port_info.ShortDebugString();
      *bcm_serdes_lane_config = k->second;
      for (int l = 1; l < bcm_port.num_serdes_lanes(); ++l) {
        auto k = serdes_lane_configs.find(bcm_port.serdes_lane() + l);
        CHECK_RETURN_IF_FALSE(k != serdes_lane_configs.end())
            << "Serdes lane " << bcm_port.serdes_lane() + l << " not found in "
            << "serdes DB for " << PrintBcmPort(bcm_port) << " with following "
            << "front panel port info: " << fp_port_info.ShortDebugString();
        CHECK_RETURN_IF_FALSE(ProtoEqual(*bcm_serdes_lane_config, k->second))
            << "Serdes lane configs found for " << PrintBcmPort(bcm_port)
            << " do not have the same value for all the lanes: "
            << j->second.ShortDebugString();
      }
      return ::util::OkStatus();
    }
  }

  return MAKE_ERROR(ERR_INTERNAL)
         << "Could not find serdes lane info for " << PrintBcmPort(bcm_port)
         << " with following front panel port info: "
         << fp_port_info.ShortDebugString();
}

std::unique_ptr<BcmSerdesDbManager> BcmSerdesDbManager::CreateInstance() {
  return absl::WrapUnique(new BcmSerdesDbManager());
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
