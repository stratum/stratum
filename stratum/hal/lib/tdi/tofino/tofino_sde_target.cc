// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// Tofino-specific SDE wrapper methods.

#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <ostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#include "absl/cleanup/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi.pb.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"

extern "C" {
#include "bf_switchd/bf_switchd.h"
#include "bf_types/bf_types.h"
#include "lld/lld_sku.h"
#include "tofino/bf_pal/bf_pal_port_intf.h"
#include "tofino/bf_pal/dev_intf.h"
#include "tofino/bf_pal/pltfm_intf.h"
#include "tofino/pdfixed/pd_devport_mgr.h"
#include "tofino/pdfixed/pd_tm.h"
#include "tdi_tofino/tdi_tofino_defs.h"

// Flag to enable detailed logging in the SDE pipe manager.
extern bool stat_mgr_enable_detail_trace;
} // extern "C"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

namespace {

::util::StatusOr<bf_port_speed_t> PortSpeedHalToBf(uint64 speed_bps) {
  switch (speed_bps) {
    case kOneGigBps:
      return BF_SPEED_1G;
    case kTenGigBps:
      return BF_SPEED_10G;
    case kTwentyFiveGigBps:
      return BF_SPEED_25G;
    case kFortyGigBps:
      return BF_SPEED_40G;
    case kFiftyGigBps:
      return BF_SPEED_50G;
    case kHundredGigBps:
      return BF_SPEED_100G;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Unsupported port speed.";
  }
}

::util::StatusOr<int> AutonegHalToBf(TriState autoneg) {
  switch (autoneg) {
    case TRI_STATE_UNKNOWN:
      return 0;
    case TRI_STATE_TRUE:
      return 1;
    case TRI_STATE_FALSE:
      return 2;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid autoneg state.";
  }
}

::util::StatusOr<bf_fec_type_t> FecModeHalToBf(
    FecMode fec_mode, uint64 speed_bps) {
  if (fec_mode == FEC_MODE_UNKNOWN || fec_mode == FEC_MODE_OFF) {
    return BF_FEC_TYP_NONE;
  } else if (fec_mode == FEC_MODE_ON || fec_mode == FEC_MODE_AUTO) {
    // we have to "guess" the FEC type to use based on the port speed.
    switch (speed_bps) {
      case kOneGigBps:
        RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid FEC mode for 1Gbps mode.";
      case kTenGigBps:
      case kFortyGigBps:
        return BF_FEC_TYP_FIRECODE;
      case kTwentyFiveGigBps:
      case kFiftyGigBps:
      case kHundredGigBps:
      case kTwoHundredGigBps:
      case kFourHundredGigBps:
        return BF_FEC_TYP_REED_SOLOMON;
      default:
        RETURN_ERROR(ERR_INVALID_PARAM) << "Unsupported port speed.";
    }
  }
  RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid FEC mode.";
}

::util::StatusOr<bf_loopback_mode_e> LoopbackModeToBf(
    LoopbackState loopback_mode) {
  switch (loopback_mode) {
    case LOOPBACK_STATE_NONE:
      return BF_LPBK_NONE;
    case LOOPBACK_STATE_MAC:
      return BF_LPBK_MAC_NEAR;
    default:
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Unsupported loopback mode: " << LoopbackState_Name(loopback_mode)
          << ".";
  }
}

// A callback function executed in SDE port state change thread context.
bf_status_t sde_port_status_callback(
    bf_dev_id_t device, bf_dev_port_t dev_port, bool up, void* cookie) {
  absl::Time timestamp = absl::Now();
  TdiSdeWrapper* tdi_sde_wrapper = TdiSdeWrapper::GetSingleton();
  if (!tdi_sde_wrapper) {
    LOG(ERROR) << "TdiSdeWrapper singleton instance is not initialized.";
    return BF_INTERNAL_ERROR;
  }
  // Forward the event.
  auto status =
      tdi_sde_wrapper->OnPortStatusEvent(device, dev_port, up, timestamp);

  return status.ok() ? BF_SUCCESS : BF_INTERNAL_ERROR;
}

}  // namespace

::util::StatusOr<PortState> TdiSdeWrapper::GetPortState(int device, int port) {
  int state = 0;
  RETURN_IF_TDI_ERROR(
      bf_pal_port_oper_state_get(static_cast<bf_dev_id_t>(device),
                                 static_cast<bf_dev_port_t>(port), &state));
  return state ? PORT_STATE_UP : PORT_STATE_DOWN;
}

::util::Status TdiSdeWrapper::GetPortCounters(
    int device, int port, PortCounters* counters) {
  uint64_t stats[BF_NUM_RMON_COUNTERS] = {0};
  RETURN_IF_TDI_ERROR(
      bf_pal_port_all_stats_get(static_cast<bf_dev_id_t>(device),
                                static_cast<bf_dev_port_t>(port), stats));
  counters->set_in_octets(stats[bf_mac_stat_OctetsReceived]);
  counters->set_out_octets(stats[bf_mac_stat_OctetsTransmittedTotal]);
  counters->set_in_unicast_pkts(
      stats[bf_mac_stat_FramesReceivedwithUnicastAddresses]);
  counters->set_out_unicast_pkts(stats[bf_mac_stat_FramesTransmittedUnicast]);
  counters->set_in_broadcast_pkts(
      stats[bf_mac_stat_FramesReceivedwithBroadcastAddresses]);
  counters->set_out_broadcast_pkts(
      stats[bf_mac_stat_FramesTransmittedBroadcast]);
  counters->set_in_multicast_pkts(
      stats[bf_mac_stat_FramesReceivedwithMulticastAddresses]);
  counters->set_out_multicast_pkts(
      stats[bf_mac_stat_FramesTransmittedMulticast]);
  counters->set_in_discards(stats[bf_mac_stat_FramesDroppedBufferFull]);
  counters->set_out_discards(0);       // stat not available
  counters->set_in_unknown_protos(0);  // stat not meaningful
  counters->set_in_errors(stats[bf_mac_stat_FrameswithanyError]);
  counters->set_out_errors(stats[bf_mac_stat_FramesTransmittedwithError]);
  counters->set_in_fcs_errors(stats[bf_mac_stat_FramesReceivedwithFCSError]);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::RegisterPortStatusEventWriter(
    std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = std::move(writer);
  RETURN_IF_TDI_ERROR(
      bf_pal_port_status_notif_reg(sde_port_status_callback, nullptr));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::GetPortInfo(
    int device, int port, TargetDatapathId *target_dp_id) {
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::HotplugPort(
    int device, int port, HotplugConfigParams& hotplug_config) {
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::AddPort(
    int device, int port, uint64 speed_bps, FecMode fec_mode) {
  ASSIGN_OR_RETURN(auto bf_speed, PortSpeedHalToBf(speed_bps));
  ASSIGN_OR_RETURN(auto bf_fec_mode, FecModeHalToBf(fec_mode, speed_bps));
  RETURN_IF_TDI_ERROR(bf_pal_port_add(static_cast<bf_dev_id_t>(device),
                                      static_cast<bf_dev_port_t>(port),
                                      bf_speed,
                                      bf_fec_mode));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::AddPort(
    int device, int port, uint64 speed_bps, const PortConfigParams& config,
    FecMode fec_mode) {
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeletePort(int device, int port) {
  RETURN_IF_TDI_ERROR(bf_pal_port_del(static_cast<bf_dev_id_t>(device),
                                      static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::EnablePort(int device, int port) {
  RETURN_IF_TDI_ERROR(bf_pal_port_enable(static_cast<bf_dev_id_t>(device),
                                         static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DisablePort(int device, int port) {
  RETURN_IF_TDI_ERROR(bf_pal_port_disable(static_cast<bf_dev_id_t>(device),
                                          static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetPortShapingRate(
    int device, int port, bool is_in_pps, uint32 burst_size,
    uint64 rate_per_second) {
  if (!is_in_pps) {
    rate_per_second /= 1000;  // The SDE expects the bitrate in kbps.
  }

  RETURN_IF_TDI_ERROR(p4_pd_tm_set_port_shaping_rate(
      device, port, is_in_pps, burst_size, rate_per_second));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::EnablePortShaping(
    int device, int port, TriState enable) {
  if (enable == TriState::TRI_STATE_TRUE) {
    RETURN_IF_TDI_ERROR(p4_pd_tm_enable_port_shaping(device, port));
  } else if (enable == TriState::TRI_STATE_FALSE) {
    RETURN_IF_TDI_ERROR(p4_pd_tm_disable_port_shaping(device, port));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetPortAutonegPolicy(
    int device, int port, TriState autoneg) {
  ASSIGN_OR_RETURN(auto autoneg_v, AutonegHalToBf(autoneg));
  RETURN_IF_TDI_ERROR(bf_pal_port_autoneg_policy_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      autoneg_v));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetPortMtu(int device, int port, int32 mtu) {
  if (mtu < 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid MTU value.";
  }
  if (mtu == 0) mtu = kBfDefaultMtu;
  RETURN_IF_TDI_ERROR(bf_pal_port_mtu_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      static_cast<uint32>(mtu), static_cast<uint32>(mtu)));
  return ::util::OkStatus();
}

bool TdiSdeWrapper::IsValidPort(int device, int port) {
  return bf_pal_port_is_valid(device, port) == BF_SUCCESS;
}

::util::Status TdiSdeWrapper::SetPortLoopbackMode(
    int device, int port, LoopbackState loopback_mode) {
  if (loopback_mode == LOOPBACK_STATE_UNKNOWN) {
    // Do nothing if we try to set loopback mode to the default one (UNKNOWN).
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(bf_loopback_mode_e lp_mode, LoopbackModeToBf(loopback_mode));
  RETURN_IF_TDI_ERROR(
      bf_pal_port_loopback_mode_set(static_cast<bf_dev_id_t>(device),
                                    static_cast<bf_dev_port_t>(port), lp_mode));
  return ::util::OkStatus();
}

::util::StatusOr<bool> TdiSdeWrapper::IsSoftwareModel(int device) {
  bool is_sw_model = true;
  auto bf_status = bf_pal_pltfm_type_get(device, &is_sw_model);
  CHECK_RETURN_IF_FALSE(bf_status == BF_SUCCESS)
      << "Error getting software model status.";
  return is_sw_model;
}

// Helper functions around reading the switch SKU.
namespace {

std::string GetBfChipFamilyAndType(int device) {
  bf_dev_type_t dev_type = lld_sku_get_dev_type(device);
  return pipe_mgr_dev_type2str(dev_type);
}

std::string GetBfChipRevision(int device) {
  bf_sku_chip_part_rev_t revision_number;
  lld_sku_get_chip_part_revision_number(device, &revision_number);
  switch (revision_number) {
    case BF_SKU_CHIP_PART_REV_A0:
      return "A0";
    case BF_SKU_CHIP_PART_REV_B0:
      return "B0";
    default:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

std::string GetBfChipId(int device) {
  uint64 chip_id = 0;
  lld_sku_get_chip_id(device, &chip_id);
  return absl::StrCat("0x", absl::Hex(chip_id));
}

}  // namespace


std::string TdiSdeWrapper::GetChipType(int device) const {
  return absl::StrCat(GetBfChipFamilyAndType(device), ", revision ",
                      GetBfChipRevision(device), ", chip_id ",
                      GetBfChipId(device));
}

// NOTE: This is Tofino-specific.
std::string TdiSdeWrapper::GetSdeVersion() const {
  return "9.11.0";
}

::util::StatusOr<uint32> TdiSdeWrapper::GetPortIdFromPortKey(
    int device, const PortKey& port_key) {
  const int port = port_key.port;
  CHECK_RETURN_IF_FALSE(port >= 0)
      << "Port ID must be non-negative. Attempted to get port " << port
      << " on dev " << device << ".";

  // PortKey uses three possible values for channel:
  //     > 0: port is channelized (first channel is 1)
  //     0: port is not channelized
  //     < 0: port channel is not important (e.g. for port groups)
  // BF SDK expects the first channel to be 0
  //     Convert base-1 channel to base-0 channel if port is channelized
  //     Otherwise, port is already 0 in the non-channelized case
  const int channel =
      (port_key.channel > 0) ? port_key.channel - 1 : port_key.channel;
  CHECK_RETURN_IF_FALSE(channel >= 0)
      << "Channel must be set for port " << port << " on dev " << device << ".";

  char port_string[MAX_PORT_HDL_STRING_LEN];
  int r = snprintf(port_string, sizeof(port_string), "%d/%d", port, channel);
  CHECK_RETURN_IF_FALSE(r > 0 && r < sizeof(port_string))
      << "Failed to build port string for port " << port << " channel "
      << channel << " on dev " << device << ".";

  bf_dev_port_t dev_port;
  RETURN_IF_TDI_ERROR(bf_pal_port_str_to_dev_port_map(
      static_cast<bf_dev_id_t>(device), port_string, &dev_port));
  return static_cast<uint32>(dev_port);
}

::util::StatusOr<int> TdiSdeWrapper::GetPcieCpuPort(int device) {
  int port = p4_devport_mgr_pcie_cpu_port_get(device);
  CHECK_RETURN_IF_FALSE(port != -1);
  return port;
}

::util::Status TdiSdeWrapper::SetTmCpuPort(int device, int port) {
  CHECK_RETURN_IF_FALSE(p4_pd_tm_set_cpuport(device, port) == 0)
      << "Unable to set CPU port " << port << " on device " << device;
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::SetDeflectOnDropDestination(
    int device, int port, int queue) {
  // The DoD destination must be a pipe-local port.
  p4_pd_tm_pipe_t pipe = DEV_PORT_TO_PIPE(port);
  RETURN_IF_TDI_ERROR(
      p4_pd_tm_set_negative_mirror_dest(device, pipe, port, queue));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::InitializeSde(
    const std::string& sde_install_path, const std::string& sde_config_file,
    bool run_in_background) {
  CHECK_RETURN_IF_FALSE(sde_install_path != "")
      << "sde_install_path is required";
  CHECK_RETURN_IF_FALSE(sde_config_file != "") << "sde_config_file is required";

  // Parse bf_switchd arguments.
  auto switchd_main_ctx = absl::make_unique<bf_switchd_context_t>();
  switchd_main_ctx->install_dir = strdup(sde_install_path.c_str());
  switchd_main_ctx->conf_file = strdup(sde_config_file.c_str());
  switchd_main_ctx->skip_p4 = true;
  if (run_in_background) {
    switchd_main_ctx->running_in_background = true;
  } else {
    switchd_main_ctx->shell_set_ucli = true;
  }

  // Determine if kernel mode packet driver is loaded.
  std::string bf_sysfs_fname;
  {
    char buf[128] = {};
    RETURN_IF_TDI_ERROR(switch_pci_sysfs_str_get(buf, sizeof(buf)));
    bf_sysfs_fname = buf;
  }
  absl::StrAppend(&bf_sysfs_fname, "/dev_add");
  LOG(INFO) << "bf_sysfs_fname: " << bf_sysfs_fname;
  if (PathExists(bf_sysfs_fname)) {
    // Override previous parsing if bf_kpkt KLM was loaded.
    LOG(INFO)
        << "kernel mode packet driver present, forcing kernel_pkt option!";
    switchd_main_ctx->kernel_pkt = true;
  }

  RETURN_IF_TDI_ERROR(bf_switchd_lib_init(switchd_main_ctx.get()))
      << "Error when starting switchd.";
  LOG(INFO) << "switchd started successfully";

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::AddDevice(
    int dev_id, const TdiDeviceConfig& device_config) {
  const ::tdi::Device *device = nullptr;
  absl::WriterMutexLock l(&data_lock_);

  CHECK_RETURN_IF_FALSE(device_config.programs_size() > 0);

  tdi_id_mapper_.reset();

  RETURN_IF_TDI_ERROR(bf_pal_device_warm_init_begin(
      dev_id, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
      /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk and build device profile for SDE to load.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_tdi_sde_config_dir));
  // Need to extend the lifetime of the path strings until the SDE reads them.
  std::vector<std::unique_ptr<std::string>> path_strings;
  device_profile.num_p4_programs = device_config.programs_size();
  for (int i = 0; i < device_config.programs_size(); ++i) {
    const auto& program = device_config.programs(i);
    const std::string program_path =
        absl::StrCat(FLAGS_tdi_sde_config_dir, "/", program.name());
    auto tdi_path = absl::make_unique<std::string>(
        absl::StrCat(program_path, "/bfrt.json"));
    RETURN_IF_ERROR(RecursivelyCreateDir(program_path));
    RETURN_IF_ERROR(WriteStringToFile(program.bfrt(), *tdi_path));

    bf_p4_program_t* p4_program = &device_profile.p4_programs[i];
    ::snprintf(p4_program->prog_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
               program.name().c_str());
    p4_program->bfrt_json_file = &(*tdi_path)[0];
    p4_program->num_p4_pipelines = program.pipelines_size();
    path_strings.emplace_back(std::move(tdi_path));
    CHECK_RETURN_IF_FALSE(program.pipelines_size() > 0);
    for (int j = 0; j < program.pipelines_size(); ++j) {
      const auto& pipeline = program.pipelines(j);
      const std::string pipeline_path =
          absl::StrCat(program_path, "/", pipeline.name());
      auto context_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/context.json"));
      auto config_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/tofino.bin"));
      RETURN_IF_ERROR(RecursivelyCreateDir(pipeline_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.context(), *context_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.config(), *config_path));

      bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[j];
      ::snprintf(pipeline_profile->p4_pipeline_name, _PI_UPDATE_MAX_NAME_SIZE,
                 "%s", pipeline.name().c_str());
      pipeline_profile->cfg_file = &(*config_path)[0];
      pipeline_profile->runtime_context_file = &(*context_path)[0];
      path_strings.emplace_back(std::move(config_path));
      path_strings.emplace_back(std::move(context_path));

      CHECK_RETURN_IF_FALSE(pipeline.scope_size() <= MAX_P4_PIPELINES);
      pipeline_profile->num_pipes_in_scope = pipeline.scope_size();
      for (int p = 0; p < pipeline.scope_size(); ++p) {
        const auto& scope = pipeline.scope(p);
        pipeline_profile->pipe_scope[p] = scope;
      }
    }
  }

  // This call re-initializes most SDE components.
  RETURN_IF_TDI_ERROR(bf_pal_device_add(dev_id, &device_profile));
  RETURN_IF_TDI_ERROR(bf_pal_device_warm_init_end(dev_id));

  // Set SDE log levels for modules of interest.
  // TODO(max): create story around SDE logs. How to get them into glog? What
  // levels to enable for which modules?
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_BFRT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PKT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  stat_mgr_enable_detail_trace = false;
  if (VLOG_IS_ON(2)) {
    CHECK_RETURN_IF_FALSE(bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT,
                                               BF_LOG_WARN) == 0);
    stat_mgr_enable_detail_trace = true;
  }

  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  RETURN_IF_TDI_ERROR(device->tdiInfoGet(
       device_config.programs(0).name(), &tdi_info_));

  // FIXME: if all we ever do is create and push, this could be one call.
  tdi_id_mapper_ = TdiIdMapper::CreateInstance();
  RETURN_IF_ERROR(
      tdi_id_mapper_->PushForwardingPipelineConfig(device_config, tdi_info_));

  ASSIGN_OR_RETURN(auto cpu_port, GetPcieCpuPort(dev_id));
  RETURN_IF_ERROR(SetTmCpuPort(dev_id, cpu_port));

  return ::util::OkStatus();
}

//  Packetio

::util::Status TdiSdeWrapper::TxPacket(int device, const std::string& buffer) {
  bf_pkt* pkt = nullptr;
  RETURN_IF_TDI_ERROR(
      bf_pkt_alloc(device, &pkt, buffer.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  auto pkt_cleaner =
      gtl::MakeCleanup([pkt, device]() { bf_pkt_free(device, pkt); });
  RETURN_IF_TDI_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8*>(buffer.data()), buffer.size()));
  RETURN_IF_TDI_ERROR(bf_pkt_tx(device, pkt, BF_PKT_TX_RING_0, pkt));
  pkt_cleaner.release();
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::StartPacketIo(int device) {
  if (!bf_pkt_is_inited(device)) {
    RETURN_IF_TDI_ERROR(bf_pkt_init());
  }

  // type of i should be bf_pkt_tx_ring_t?
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_TDI_ERROR(bf_pkt_tx_done_notif_register(
        device, TdiSdeWrapper::BfPktTxNotifyCallback,
        static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_TDI_ERROR(
        bf_pkt_rx_register(device, TdiSdeWrapper::BfPktRxNotifyCallback,
                           static_cast<bf_pkt_rx_ring_t>(rx_ring), nullptr));
  }
  VLOG(1) << "Registered packetio callbacks on device " << device << ".";
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::StopPacketIo(int device) {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_TDI_ERROR(bf_pkt_tx_done_notif_deregister(
        device, static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_TDI_ERROR(
        bf_pkt_rx_deregister(device, static_cast<bf_pkt_rx_ring_t>(rx_ring)));
  }
  VLOG(1) << "Unregistered packetio callbacks on device " << device << ".";
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::HandlePacketRx(
    bf_dev_id_t device, bf_pkt* pkt, bf_pkt_rx_ring_t rx_ring) {
  absl::ReaderMutexLock l(&packet_rx_callback_lock_);
  auto rx_writer = gtl::FindOrNull(device_to_packet_rx_writer_, device);
  CHECK_RETURN_IF_FALSE(rx_writer)
      << "No Rx callback registered for device id " << device << ".";

  std::string buffer(reinterpret_cast<const char*>(bf_pkt_get_pkt_data(pkt)),
                     bf_pkt_get_pkt_size(pkt));
  if (!(*rx_writer)->TryWrite(buffer).ok()) {
    LOG_EVERY_N(INFO, 500) << "Dropped packet received from CPU.";
  }
  VLOG(1) << "Received " << buffer.size() << " byte packet from CPU "
          << StringToHex(buffer);
  return ::util::OkStatus();
}

bf_status_t TdiSdeWrapper::BfPktTxNotifyCallback(
    bf_dev_id_t device, bf_pkt_tx_ring_t tx_ring, uint64 tx_cookie,
    uint32 status) {
  VLOG(1) << "Tx done notification for device: " << device
          << " tx ring: " << tx_ring << " tx cookie: " << tx_cookie
          << " status: " << status;

  bf_pkt* pkt = reinterpret_cast<bf_pkt*>(tx_cookie);
  return bf_pkt_free(device, pkt);
}

bf_status_t TdiSdeWrapper::BfPktRxNotifyCallback(
    bf_dev_id_t device, bf_pkt* pkt, void* cookie, bf_pkt_rx_ring_t rx_ring) {
  TdiSdeWrapper* tdi_sde_wrapper = TdiSdeWrapper::GetSingleton();
  // TODO(max): Handle error
  tdi_sde_wrapper->HandlePacketRx(device, pkt, rx_ring);
  return bf_pkt_free(device, pkt);
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
