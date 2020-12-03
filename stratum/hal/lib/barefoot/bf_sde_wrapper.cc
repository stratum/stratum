// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_sde_wrapper.h"

#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "bf_rt/bf_rt_table_operations.hpp"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/barefoot/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/utils.h"

extern "C" {
#include "tofino/bf_pal/bf_pal_port_intf.h"
#include "tofino/bf_pal/dev_intf.h"
#include "tofino/bf_pal/pltfm_intf.h"
#include "tofino/pdfixed/pd_devport_mgr.h"
#include "tofino/pdfixed/pd_tm.h"
// Flag to enable detailed logging in the SDE pipe manager.
extern bool stat_mgr_enable_detail_trace;
}

DECLARE_string(bfrt_sde_config_dir);

namespace stratum {
namespace hal {
namespace barefoot {

constexpr absl::Duration BfSdeWrapper::kWriteTimeout;
constexpr int32 BfSdeWrapper::kBfDefaultMtu;
// TODO(max): move into SdeWrapper?
constexpr int _PI_UPDATE_MAX_NAME_SIZE = 100;

namespace {

// A callback function executed in SDE port state change thread context.
bf_status_t sde_port_status_callback(bf_dev_id_t dev_id, bf_dev_port_t dev_port,
                                     bool up, void* cookie) {
  BfSdeWrapper* bf_sde_wrapper = BfSdeWrapper::GetSingleton();
  if (!bf_sde_wrapper) {
    LOG(ERROR) << "BfSdeWrapper singleton instance is not initialized.";
    return BF_INTERNAL_ERROR;
  }
  // Forward the event.
  auto status = bf_sde_wrapper->OnPortStatusEvent(dev_id, dev_port, up);

  return status.ok() ? BF_SUCCESS : BF_INTERNAL_ERROR;
}

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

::util::StatusOr<bf_fec_type_t> FecModeHalToBf(FecMode fec_mode,
                                               uint64 speed_bps) {
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

}  // namespace

BfSdeWrapper* BfSdeWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BfSdeWrapper::init_lock_(absl::kConstInit);

BfSdeWrapper::BfSdeWrapper() : port_status_event_writer_(nullptr) {}

::util::StatusOr<PortState> BfSdeWrapper::GetPortState(int device, int port) {
  int state;
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_oper_state_get(static_cast<bf_dev_id_t>(device),
                                 static_cast<bf_dev_port_t>(port), &state));
  return state ? PORT_STATE_UP : PORT_STATE_DOWN;
}

::util::Status BfSdeWrapper::GetPortCounters(int device, int port,
                                             PortCounters* counters) {
  uint64_t stats[BF_NUM_RMON_COUNTERS];
  RETURN_IF_BFRT_ERROR(
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

::util::Status BfSdeWrapper::OnPortStatusEvent(int device, int port, bool up) {
  // Create PortStatusEvent message.
  PortState state = up ? PORT_STATE_UP : PORT_STATE_DOWN;
  PortStatusEvent event = {device, port, state};

  {
    absl::ReaderMutexLock l(&port_status_event_writer_lock_);
    if (!port_status_event_writer_) {
      return ::util::OkStatus();
    }
    return port_status_event_writer_->Write(event, kWriteTimeout);
  }
}

::util::Status BfSdeWrapper::RegisterPortStatusEventWriter(
    std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = std::move(writer);
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_status_notif_reg(sde_port_status_callback, nullptr));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::UnregisterPortStatusEventWriter() {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::AddPort(int device, int port, uint64 speed_bps,
                                     FecMode fec_mode) {
  ASSIGN_OR_RETURN(auto bf_speed, PortSpeedHalToBf(speed_bps));
  ASSIGN_OR_RETURN(auto bf_fec_mode, FecModeHalToBf(fec_mode, speed_bps));
  RETURN_IF_BFRT_ERROR(bf_pal_port_add(static_cast<bf_dev_id_t>(device),
                                       static_cast<bf_dev_port_t>(port),
                                       bf_speed, bf_fec_mode));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::DeletePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_del(static_cast<bf_dev_id_t>(device),
                                       static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::EnablePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_enable(static_cast<bf_dev_id_t>(device),
                                          static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::DisablePort(int device, int port) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_disable(static_cast<bf_dev_id_t>(device),
                                           static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetPortAutonegPolicy(int device, int port,
                                                  TriState autoneg) {
  ASSIGN_OR_RETURN(auto autoneg_v, AutonegHalToBf(autoneg));
  RETURN_IF_BFRT_ERROR(bf_pal_port_autoneg_policy_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      autoneg_v));
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::SetPortMtu(int device, int port, int32 mtu) {
  if (mtu < 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid MTU value.";
  }
  if (mtu == 0) mtu = kBfDefaultMtu;
  RETURN_IF_BFRT_ERROR(bf_pal_port_mtu_set(
      static_cast<bf_dev_id_t>(device), static_cast<bf_dev_port_t>(port),
      static_cast<uint32>(mtu), static_cast<uint32>(mtu)));
  return ::util::OkStatus();
}

bool BfSdeWrapper::IsValidPort(int device, int port) {
  return (bf_pal_port_is_valid(device, port) == BF_SUCCESS);
}

::util::Status BfSdeWrapper::SetPortLoopbackMode(int device, int port,
                                                 LoopbackState loopback_mode) {
  if (loopback_mode == LOOPBACK_STATE_UNKNOWN) {
    // Do nothing if we try to set loopback mode to the default one (UNKNOWN).
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(bf_loopback_mode_e lp_mode, LoopbackModeToBf(loopback_mode));
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_loopback_mode_set(static_cast<bf_dev_id_t>(device),
                                    static_cast<bf_dev_port_t>(port), lp_mode));

  return ::util::OkStatus();
}

::util::StatusOr<bool> BfSdeWrapper::IsSoftwareModel(int device) {
  bool is_sw_model;
  auto bf_status = bf_pal_pltfm_type_get(device, &is_sw_model);
  CHECK_RETURN_IF_FALSE(bf_status == BF_SUCCESS)
      << "Error getting software model status.";

  return is_sw_model;
}

::util::StatusOr<uint32> BfSdeWrapper::GetPortIdFromPortKey(
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
  RETURN_IF_BFRT_ERROR(bf_pal_port_str_to_dev_port_map(
      static_cast<bf_dev_id_t>(device), port_string, &dev_port));
  return static_cast<uint32>(dev_port);
}

::util::StatusOr<int> BfSdeWrapper::GetPcieCpuPort(int device) {
  int port = p4_devport_mgr_pcie_cpu_port_get(device);
  CHECK_RETURN_IF_FALSE(port != -1);
  return port;
}

::util::Status BfSdeWrapper::SetTmCpuPort(int device, int port) {
  CHECK_RETURN_IF_FALSE(p4_pd_tm_set_cpuport(device, port) == 0)
      << "Unable to set CPU port " << port << " on device " << device;
  return ::util::OkStatus();
}

// BFRT

::util::Status BfSdeWrapper::AddDevice(int device,
                                       const BfrtDeviceConfig& device_config) {
  absl::WriterMutexLock l(&data_lock_);

  // CHECK_RETURN_IF_FALSE(initialized_) << "Not initialized";
  CHECK_RETURN_IF_FALSE(device_config.programs_size() > 0);

  // if (pipeline_initialized_) {
  // RETURN_IF_BFRT_ERROR(bf_device_remove(device));
  // }

  bfrt_device_manager_ = &bfrt::BfRtDevMgr::getInstance();
  bfrt_id_mapper_.reset();

  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_begin(
      device, BF_DEV_WARM_INIT_FAST_RECFG, BF_DEV_SERDES_UPD_NONE,
      /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk and build device profile for SDE to load.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_bfrt_sde_config_dir));
  // Need to extend the lifetime of the path strings until the SDE read them.
  std::vector<std::unique_ptr<std::string>> path_strings;
  device_profile.num_p4_programs = device_config.programs_size();
  for (int i = 0; i < device_config.programs_size(); ++i) {
    const auto& program = device_config.programs(i);
    const std::string program_path =
        absl::StrCat(FLAGS_bfrt_sde_config_dir, "/", program.name());
    auto bfrt_path = absl::make_unique<std::string>(
        absl::StrCat(program_path, "/bfrt.json"));
    RETURN_IF_ERROR(RecursivelyCreateDir(program_path));
    RETURN_IF_ERROR(WriteStringToFile(program.bfrt(), *bfrt_path));

    bf_p4_program_t* p4_program = &device_profile.p4_programs[i];
    ::snprintf(p4_program->prog_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
               program.name().c_str());
    p4_program->bfrt_json_file = &(*bfrt_path)[0];
    p4_program->num_p4_pipelines = program.pipelines_size();
    path_strings.emplace_back(std::move(bfrt_path));
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

  // bf_device_add?
  // This call re-initializes most SDE components.
  RETURN_IF_BFRT_ERROR(bf_pal_device_add(device, &device_profile));
  RETURN_IF_BFRT_ERROR(bf_pal_device_warm_init_end(device));

  // Set SDE log levels for modules of interest.
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_BFRT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  CHECK_RETURN_IF_FALSE(
      bf_sys_log_level_set(BF_MOD_PKT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  if (VLOG_IS_ON(2)) {
    CHECK_RETURN_IF_FALSE(bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT,
                                               BF_LOG_INFO) == 0);
    stat_mgr_enable_detail_trace = true;
  }

  RETURN_IF_BFRT_ERROR(bfrt_device_manager_->bfRtInfoGet(
      device, device_config.programs(0).name(), &bfrt_info_));

  // FIXME: if all we ever do is create and push, this could be one call.
  bfrt_id_mapper_ = BfrtIdMapper::CreateInstance(device);
  RETURN_IF_ERROR(
      bfrt_id_mapper_->PushForwardingPipelineConfig(device_config, bfrt_info_));

  return ::util::OkStatus();
}

// Create and start an new session.
::util::StatusOr<std::shared_ptr<BfSdeInterface::SessionInterface>>
BfSdeWrapper::CreateSession() {
  return Session::CreateSession();
}

//  Packetio

::util::Status BfSdeWrapper::TxPacket(int device, const std::string& buffer) {
  bf_pkt* pkt = nullptr;
  RETURN_IF_BFRT_ERROR(
      bf_pkt_alloc(device, &pkt, buffer.size(), BF_DMA_CPU_PKT_TRANSMIT_0));
  auto pkt_cleaner =
      gtl::MakeCleanup([pkt, device]() { bf_pkt_free(device, pkt); });
  RETURN_IF_BFRT_ERROR(bf_pkt_data_copy(
      pkt, reinterpret_cast<const uint8*>(buffer.data()), buffer.size()));
  RETURN_IF_BFRT_ERROR(bf_pkt_tx(device, pkt, BF_PKT_TX_RING_0, pkt));
  pkt_cleaner.release();

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::StartPacketIo(int device) {
  // Maybe move to InitSde function?
  if (!bf_pkt_is_inited(device)) {
    RETURN_IF_BFRT_ERROR(bf_pkt_init());
  }

  // type of i should be bf_pkt_tx_ring_t?
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_register(
        device, BfSdeWrapper::BfPktTxNotifyCallback,
        static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(
        bf_pkt_rx_register(device, BfSdeWrapper::BfPktRxNotifyCallback,
                           static_cast<bf_pkt_rx_ring_t>(rx_ring), nullptr));
  }
  VLOG(1) << "Registered packetio callbacks on device " << device << ".";

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::StopPacketIo(int device) {
  for (int tx_ring = BF_PKT_TX_RING_0; tx_ring < BF_PKT_TX_RING_MAX;
       ++tx_ring) {
    RETURN_IF_BFRT_ERROR(bf_pkt_tx_done_notif_deregister(
        device, static_cast<bf_pkt_tx_ring_t>(tx_ring)));
  }

  for (int rx_ring = BF_PKT_RX_RING_0; rx_ring < BF_PKT_RX_RING_MAX;
       ++rx_ring) {
    RETURN_IF_BFRT_ERROR(
        bf_pkt_rx_deregister(device, static_cast<bf_pkt_rx_ring_t>(rx_ring)));
  }
  VLOG(1) << "Unregistered packetio callbacks on device " << device << ".";

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::RegisterPacketReceiveWriter(
    int device, std::unique_ptr<ChannelWriter<std::string>> writer) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_[device] = std::move(writer);
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::UnregisterPacketReceiveWriter(int device) {
  absl::WriterMutexLock l(&packet_rx_callback_lock_);
  device_to_packet_rx_writer_.erase(device);
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::HandlePacketRx(bf_dev_id_t device, bf_pkt* pkt,
                                            bf_pkt_rx_ring_t rx_ring) {
  absl::ReaderMutexLock l(&packet_rx_callback_lock_);
  auto rx_writer = gtl::FindOrNull(device_to_packet_rx_writer_, device);
  CHECK_RETURN_IF_FALSE(rx_writer)
      << "No Rx callback registered for device id " << device << ".";

  std::string buffer(reinterpret_cast<const char*>(bf_pkt_get_pkt_data(pkt)),
                     bf_pkt_get_pkt_size(pkt));
  if (!(*rx_writer)->TryWrite(buffer).ok()) {
    LOG_EVERY_N(INFO, 500) << "Dropped packet received from CPU.";
  }

  VLOG(1) << "Received packet from CPU " << buffer.size() << " bytes "
          << StringToHex(buffer);

  return ::util::OkStatus();
}

bf_status_t BfSdeWrapper::BfPktTxNotifyCallback(bf_dev_id_t dev_id,
                                                bf_pkt_tx_ring_t tx_ring,
                                                uint64 tx_cookie,
                                                uint32 status) {
  VLOG(1) << "Tx done notification for device: " << dev_id
          << " tx ring: " << tx_ring << " tx cookie: " << tx_cookie
          << " status: " << status;

  bf_pkt* pkt = reinterpret_cast<bf_pkt*>(tx_cookie);
  return bf_pkt_free(dev_id, pkt);
}

bf_status_t BfSdeWrapper::BfPktRxNotifyCallback(bf_dev_id_t dev_id, bf_pkt* pkt,
                                                void* cookie,
                                                bf_pkt_rx_ring_t rx_ring) {
  BfSdeWrapper* bf_sde_wrapper = BfSdeWrapper::GetSingleton();
  // TODO: Handle error
  bf_sde_wrapper->HandlePacketRx(dev_id, pkt, rx_ring);
  // static_cast<BfSdeWrapper*>(cookie)->HandlePacketRx(dev_id, pkt, rx_ring);
  return bf_pkt_free(dev_id, pkt);
}

bf_rt_target_t BfSdeWrapper::GetDeviceTarget(int device) const {
  bf_rt_target_t dev_tgt = {};
  dev_tgt.dev_id = device;
  dev_tgt.pipe_id = BF_DEV_PIPE_ALL;
  return dev_tgt;
}

// PRE
namespace {
::util::Status PrintMcGroupEntry(const bfrt::BfRtTable* table,
                                 const bfrt::BfRtTableKey* table_key,
                                 const bfrt::BfRtTableData* table_data) {
  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  uint64 multicast_group_id;

  // Key: $MGID
  RETURN_IF_ERROR(GetField(*table_key, kMgid, &multicast_group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1XidValid, &l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeL1Xid, &l1_xid_list));

  LOG(INFO) << "Multicast group id " << multicast_group_id << " has "
            << mc_node_list.size() << " nodes.";
  for (const auto& node : mc_node_list) {
    LOG(INFO) << "\tnode id " << node;
  }

  return ::util::OkStatus();
}

::util::Status PrintMcNodeEntry(const bfrt::BfRtTable* table,
                                const bfrt::BfRtTableKey* table_key,
                                const bfrt::BfRtTableData* table_data) {
  // Key: $MULTICAST_NODE_ID (24 bit)
  uint64 node_id;
  RETURN_IF_ERROR(GetField(*table_key, kMcNodeId, &node_id));
  // Data: $MULTICAST_RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  // Data: $DEV_PORT
  std::vector<uint32> ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &ports));

  std::string ports_str = " ports [ ";
  for (const auto& port : ports) {
    ports_str += std::to_string(port) + " ";
  }
  ports_str += "]";
  LOG(INFO) << "Node id " << node_id << ": rid " << rid << ports_str;

  return ::util::OkStatus();
}
}  // namespace

::util::Status BfSdeWrapper::DumpPreState(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session) {
  if (VLOG_IS_ON(2)) {
    auto real_session = std::dynamic_pointer_cast<Session>(session);
    CHECK_RETURN_IF_FALSE(real_session);

    auto bf_dev_tgt = GetDeviceTarget(device);
    const bfrt::BfRtTable* table;

    // Dump group table
    LOG(INFO) << "#### $pre.mgid ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
    std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcGroupEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";

    // Dump node table
    LOG(INFO) << "#### $pre.node ####";
    RETURN_IF_BFRT_ERROR(
        bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
    for (size_t i = 0; i < keys.size(); ++i) {
      const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
      const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
      PrintMcNodeEntry(table, table_key.get(), table_data.get());
    }
    LOG(INFO) << "###################";
  }
  return ::util::OkStatus();
}

::util::StatusOr<uint32> BfSdeWrapper::GetFreeMulticastNodeId(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  size_t table_size;
  RETURN_IF_BFRT_ERROR(table->tableSizeGet(&table_size));
  uint32 usage;
  RETURN_IF_BFRT_ERROR(table->tableUsageGet(
      *real_session->bfrt_session_, bf_dev_tgt,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, &usage));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  uint32 id = usage;
  for (size_t _ = 0; _ < table_size; ++_) {
    // Key: $MULTICAST_NODE_ID
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, id));
    bf_status_t status = table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key,
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get());
    if (status == BF_OBJECT_NOT_FOUND) {
      return id;
    } else if (status == BF_SUCCESS) {
      id++;
      continue;
    } else {
      RETURN_IF_BFRT_ERROR(status);
    }
  }

  RETURN_ERROR(ERR_TABLE_FULL) << "Could not find free multicast node id.";
}

::util::StatusOr<uint32> BfSdeWrapper::CreateMulticastNode(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    int mc_replication_id, const std::vector<uint32>& mc_lag_ids,
    const std::vector<uint32> ports) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  auto bf_dev_tgt = GetDeviceTarget(device);

  ASSIGN_OR_RETURN(uint64 mc_node_id, GetFreeMulticastNodeId(device, session));

  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
  // Data: $MULTICAST_RID (16 bit)
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcReplicationId, mc_replication_id));
  // Data: $MULTICAST_LAG_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeLagId, mc_lag_ids));
  // Data: $DEV_PORT
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeDevPort, ports));

  RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  return mc_node_id;
}

::util::StatusOr<std::vector<uint32>> BfSdeWrapper::GetNodesInMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);

  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $MULTICAST_NODE_ID
  std::vector<uint32> mc_node_list;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));

  return mc_node_list;
}

::util::Status BfSdeWrapper::DeleteMulticastNodes(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  // TODO(max): handle partial delete failures
  for (const auto& mc_node_id : mc_node_ids) {
    std::unique_ptr<bfrt::BfRtTableKey> table_key;
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
    RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                              bf_dev_tgt, *table_key));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetMulticastNode(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 mc_node_id, int* replication_id, std::vector<uint32>* lag_ids,
    std::vector<uint32>* ports) {
  CHECK_RETURN_IF_FALSE(replication_id);
  CHECK_RETURN_IF_FALSE(lag_ids);
  CHECK_RETURN_IF_FALSE(ports);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE node table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreNodeTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));
  // Key: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_key.get(), kMcNodeId, mc_node_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));
  // Data: $DEV_PORT
  std::vector<uint32> dev_ports;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeDevPort, &dev_ports));
  *ports = dev_ports;
  // Data: $RID (16 bit)
  uint64 rid;
  RETURN_IF_ERROR(GetField(*table_data, kMcReplicationId, &rid));
  *replication_id = rid;
  // Data: $MULTICAST_LAG_ID
  std::vector<uint32> lags;
  RETURN_IF_ERROR(GetField(*table_data, kMcNodeLagId, &lags));
  *lag_ids = lags;

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;  // PRE MGID table.
  bf_rt_id_t table_id;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  RETURN_IF_BFRT_ERROR(table->tableIdGet(&table_id));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  std::vector<uint32> mc_node_list;
  std::vector<bool> l1_xid_valid_list;
  std::vector<uint32> l1_xid_list;
  for (const auto& mc_node_id : mc_node_ids) {
    mc_node_list.push_back(mc_node_id);
    // TODO(Yi): P4Runtime doesn't support XID, set invalid for now.
    l1_xid_valid_list.push_back(false);
    l1_xid_list.push_back(0);
  }
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  // Data: $MULTICAST_NODE_ID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeId, mc_node_list));
  // Data: $MULTICAST_NODE_L1_XID_VALID
  RETURN_IF_ERROR(
      SetField(table_data.get(), kMcNodeL1XidValid, l1_xid_valid_list));
  // Data: $MULTICAST_NODE_L1_XID
  RETURN_IF_ERROR(SetField(table_data.get(), kMcNodeL1Xid, l1_xid_list));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(device, session, group_id, mc_node_ids, true);
}
::util::Status BfSdeWrapper::ModifyMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, const std::vector<uint32>& mc_node_ids) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteMulticastGroup(device, session, group_id, mc_node_ids, false);
}

::util::Status BfSdeWrapper::DeleteMulticastGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $MGID
  RETURN_IF_ERROR(SetField(table_key.get(), kMgid, group_id));
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetMulticastGroups(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 group_id, std::vector<uint32>* group_ids,
    std::vector<std::vector<uint32>>* mc_node_ids) {
  CHECK_RETURN_IF_FALSE(group_ids);
  CHECK_RETURN_IF_FALSE(mc_node_ids);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;  // PRE MGID table.
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet(kPreMgidTable, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $MGID
    RETURN_IF_ERROR(SetField(keys[0].get(), kMgid, group_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  group_ids->resize(0);
  mc_node_ids->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    ::p4::v1::MulticastGroupEntry result;
    // Key: $MGID
    uint64 group_id;
    RETURN_IF_ERROR(GetField(*table_key, kMgid, &group_id));
    group_ids->push_back(group_id);
    // Data: $MULTICAST_NODE_ID
    std::vector<uint32> mc_node_list;
    RETURN_IF_ERROR(GetField(*table_data, kMcNodeId, &mc_node_list));
    mc_node_ids->push_back(mc_node_list);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(mc_node_ids->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));

  // Key: $sid
  RETURN_IF_ERROR(SetField(table_key.get(), "$sid", session_id));
  // Data: $direction
  RETURN_IF_ERROR(SetField(table_data.get(), "$direction", "BOTH"));
  // Data: $session_enable
  RETURN_IF_ERROR(SetFieldBool(table_data.get(), "$session_enable", true));
  // Data: $ucast_egress_port
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$ucast_egress_port", egress_port));
  // Data: $ucast_egress_port_valid
  RETURN_IF_ERROR(
      SetFieldBool(table_data.get(), "$ucast_egress_port_valid", true));
  // Data: $ingress_cos
  RETURN_IF_ERROR(SetField(table_data.get(), "$ingress_cos", cos));
  // Data: $max_pkt_len
  RETURN_IF_ERROR(SetField(table_data.get(), "$max_pkt_len", max_pkt_len));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(device, session, session_id, egress_port, cos,
                           max_pkt_len, true);
}

::util::Status BfSdeWrapper::ModifyCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, int egress_port, int cos, int max_pkt_len) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteCloneSession(device, session, session_id, egress_port, cos,
                           max_pkt_len, false);
}

::util::Status BfSdeWrapper::DeleteCloneSession(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &table_data));
  // Key: $sid
  RETURN_IF_ERROR(SetField(table_key.get(), "$sid", session_id));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetCloneSessions(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 session_id, std::vector<uint32>* session_ids,
    std::vector<int>* egress_ports, std::vector<int>* coss,
    std::vector<int>* max_pkt_lens) {
  CHECK_RETURN_IF_FALSE(session_ids);
  CHECK_RETURN_IF_FALSE(egress_ports);
  CHECK_RETURN_IF_FALSE(coss);
  CHECK_RETURN_IF_FALSE(max_pkt_lens);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromNameGet("$mirror.cfg", &table));
  bf_rt_id_t action_id;
  RETURN_IF_BFRT_ERROR(table->actionIdGet("$normal", &action_id));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (session_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(action_id, &datums[0]));
    // Key: $sid
    RETURN_IF_ERROR(SetField(keys[0].get(), "$sid", session_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  session_ids->resize(0);
  egress_ports->resize(0);
  coss->resize(0);
  max_pkt_lens->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $sid
    uint64 session_id;
    RETURN_IF_ERROR(GetField(*table_key, "$sid", &session_id));
    session_ids->push_back(session_id);
    // Data: $ingress_cos
    uint64 ingress_cos;
    RETURN_IF_ERROR(GetField(*table_data, "$ingress_cos", &ingress_cos));
    coss->push_back(ingress_cos);
    // Data: $max_pkt_len
    uint64 pkt_len;
    RETURN_IF_ERROR(GetField(*table_data, "$max_pkt_len", &pkt_len));
    max_pkt_lens->push_back(pkt_len);
    // Data: $ucast_egress_port
    uint64 port;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port", &port));
    egress_ports->push_back(port);
    // Data: $session_enable
    bool session_enable;
    RETURN_IF_ERROR(GetField(*table_data, "$session_enable", &session_enable));
    CHECK_RETURN_IF_FALSE(session_enable)
        << "Found a session that is not enabled.";
    // Data: $ucast_egress_port_valid
    bool ucast_egress_port_valid;
    RETURN_IF_ERROR(GetField(*table_data, "$ucast_egress_port_valid",
                             &ucast_egress_port_valid));
    CHECK_RETURN_IF_FALSE(ucast_egress_port_valid)
        << "Found a unicase egress port that is not set valid.";
  }

  CHECK_EQ(session_ids->size(), keys.size());
  CHECK_EQ(egress_ports->size(), keys.size());
  CHECK_EQ(coss->size(), keys.size());
  CHECK_EQ(max_pkt_lens->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteIndirectCounter(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 counter_id, int counter_index, absl::optional<uint64> byte_count,
    absl::optional<uint64> packet_count) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(counter_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Counter key: $COUNTER_INDEX
  RETURN_IF_ERROR(SetField(table_key.get(), "$COUNTER_INDEX", counter_index));

  // Counter data: $COUNTER_SPEC_BYTES
  if (byte_count.has_value()) {
    bf_rt_id_t field_id;
    auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
    if (bf_status == BF_SUCCESS) {
      RETURN_IF_BFRT_ERROR(table_data->setValue(field_id, byte_count.value()));
    }
  }
  // Counter data: $COUNTER_SPEC_PKTS
  if (packet_count.has_value()) {
    bf_rt_id_t field_id;
    auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
    if (bf_status == BF_SUCCESS) {
      RETURN_IF_BFRT_ERROR(
          table_data->setValue(field_id, packet_count.value()));
    }
  }
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryMod(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ReadIndirectCounter(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 counter_id, int counter_index, absl::optional<uint64>* byte_count,
    absl::optional<uint64>* packet_count, absl::Duration timeout) {
  CHECK_RETURN_IF_FALSE(byte_count);
  CHECK_RETURN_IF_FALSE(packet_count);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(counter_id, &table));

  RETURN_IF_ERROR(SynchronizeCounters(device, session, counter_id, timeout));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Counter key: $COUNTER_INDEX
  RETURN_IF_ERROR(SetField(table_key.get(), "$COUNTER_INDEX", counter_index));

  // Read the counter data.
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryGet(
      *real_session->bfrt_session_, bf_dev_tgt, *table_key,
      bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, table_data.get()));

  byte_count->reset();
  packet_count->reset();
  // Counter data: $COUNTER_SPEC_BYTES
  bf_rt_id_t field_id;
  auto bf_status = table->dataFieldIdGet("$COUNTER_SPEC_BYTES", &field_id);
  if (bf_status == BF_SUCCESS) {
    uint64 counter_data;
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
    *byte_count = counter_data;
  }

  // Counter data: $COUNTER_SPEC_PKTS
  bf_status = table->dataFieldIdGet("$COUNTER_SPEC_PKTS", &field_id);
  if (bf_status == BF_SUCCESS) {
    uint64 counter_data;
    RETURN_IF_BFRT_ERROR(table_data->getValue(field_id, &counter_data));
    *packet_count = counter_data;
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, int action_id,
    const BfActionData& action_data, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$ACTION_MEMBER_ID", member_id));

  // Action data
  RETURN_IF_BFRT_ERROR(table->dataReset(action_id, table_data.get()));
  for (const auto& param : action_data.params()) {
    const size_t size = param.value().size();
    const uint8* val = reinterpret_cast<const uint8*>(param.value().data());
    RETURN_IF_BFRT_ERROR(table_data->setValue(param.id(), val, size));
  }

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, int action_id,
    const BfActionData& action_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(device, session, table_id, member_id,
                                  action_id, action_data, true);
  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::ModifyActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, int action_id,
    const BfActionData& action_data) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileMember(device, session, table_id, member_id,
                                  action_id, action_data, false);
}

::util::Status BfSdeWrapper::DeleteActionProfileMember(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));

  // Key: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$ACTION_MEMBER_ID", member_id));

  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetActionProfileMembers(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int member_id, std::vector<int>* member_ids,
    std::vector<int>* action_ids, std::vector<BfActionData>* action_datas) {
  CHECK_RETURN_IF_FALSE(member_ids);
  CHECK_RETURN_IF_FALSE(action_ids);
  CHECK_RETURN_IF_FALSE(action_datas);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (member_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $ACTION_MEMBER_ID
    RETURN_IF_ERROR(SetField(keys[0].get(), "$ACTION_MEMBER_ID", member_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  member_ids->resize(0);
  action_ids->resize(0);
  action_datas->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $sid
    uint64 member_id;
    RETURN_IF_ERROR(GetField(*table_key, "$ACTION_MEMBER_ID", &member_id));
    member_ids->push_back(member_id);

    // Data: action id
    bf_rt_id_t action_id;
    RETURN_IF_BFRT_ERROR(table_data->actionIdGet(&action_id));
    action_ids->push_back(action_id);

    // Data: action params
    BfActionData action_data;
    std::vector<bf_rt_id_t> field_id_list;
    RETURN_IF_BFRT_ERROR(table->dataFieldIdListGet(action_id, &field_id_list));
    for (const auto& field_id : field_id_list) {
      size_t field_size;
      RETURN_IF_BFRT_ERROR(
          table->dataFieldSizeGet(field_id, action_id, &field_size));
      // "field_size" describes how many "bits" is this field, need to convert
      // to bytes with padding.
      field_size = (field_size + 7) / 8;
      uint8 field_data[field_size];
      RETURN_IF_BFRT_ERROR(
          table_data->getValue(field_id, field_size, field_data));
      const void* param_val = reinterpret_cast<const void*>(field_data);
      auto* param = action_data.add_params();
      param->set_id(field_id);
      param->set_value(param_val, field_size);
    }
    action_datas->push_back(action_data);
  }

  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(action_ids->size(), keys.size());
  CHECK_EQ(action_datas->size(), keys.size());

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::WriteActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status, bool insert) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  std::unique_ptr<bfrt::BfRtTableData> table_data;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  RETURN_IF_BFRT_ERROR(table->dataAllocate(&table_data));

  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$SELECTOR_GROUP_ID", group_id));
  // Data: $ACTION_MEMBER_ID
  RETURN_IF_ERROR(SetField(table_data.get(), "$ACTION_MEMBER_ID", member_ids));
  // Data: $ACTION_MEMBER_STATUS
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$ACTION_MEMBER_STATUS", member_status));
  // Data: $MAX_GROUP_SIZE
  RETURN_IF_ERROR(
      SetField(table_data.get(), "$MAX_GROUP_SIZE", max_group_size));

  auto bf_dev_tgt = GetDeviceTarget(device);
  if (insert) {
    RETURN_IF_BFRT_ERROR(table->tableEntryAdd(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  } else {
    RETURN_IF_BFRT_ERROR(table->tableEntryMod(
        *real_session->bfrt_session_, bf_dev_tgt, *table_key, *table_data));
  }

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::InsertActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(device, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 true);
}

::util::Status BfSdeWrapper::ModifyActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, int max_group_size,
    const std::vector<uint32>& member_ids,
    const std::vector<bool>& member_status) {
  ::absl::ReaderMutexLock l(&data_lock_);
  return WriteActionProfileGroup(device, session, table_id, group_id,
                                 max_group_size, member_ids, member_status,
                                 false);
}

::util::Status BfSdeWrapper::DeleteActionProfileGroup(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id) {
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::unique_ptr<bfrt::BfRtTableKey> table_key;
  RETURN_IF_BFRT_ERROR(table->keyAllocate(&table_key));
  // Key: $SELECTOR_GROUP_ID
  RETURN_IF_ERROR(SetField(table_key.get(), "$SELECTOR_GROUP_ID", group_id));
  auto bf_dev_tgt = GetDeviceTarget(device);
  RETURN_IF_BFRT_ERROR(table->tableEntryDel(*real_session->bfrt_session_,
                                            bf_dev_tgt, *table_key));

  return ::util::OkStatus();
}

::util::Status BfSdeWrapper::GetActionProfileGroups(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, int group_id, std::vector<int>* group_ids,
    std::vector<int>* max_group_sizes,
    std::vector<std::vector<uint32>>* member_ids,
    std::vector<std::vector<bool>>* member_status) {
  CHECK_RETURN_IF_FALSE(group_ids);
  CHECK_RETURN_IF_FALSE(max_group_sizes);
  CHECK_RETURN_IF_FALSE(member_ids);
  CHECK_RETURN_IF_FALSE(member_status);
  ::absl::ReaderMutexLock l(&data_lock_);
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  auto bf_dev_tgt = GetDeviceTarget(device);
  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));
  std::vector<std::unique_ptr<bfrt::BfRtTableKey>> keys;
  std::vector<std::unique_ptr<bfrt::BfRtTableData>> datums;
  // Is this a wildcard read?
  if (group_id != 0) {
    keys.resize(1);
    datums.resize(1);
    RETURN_IF_BFRT_ERROR(table->keyAllocate(&keys[0]));
    RETURN_IF_BFRT_ERROR(table->dataAllocate(&datums[0]));
    // Key: $SELECTOR_GROUP_ID
    RETURN_IF_ERROR(SetField(keys[0].get(), "$SELECTOR_GROUP_ID", group_id));
    RETURN_IF_BFRT_ERROR(table->tableEntryGet(
        *real_session->bfrt_session_, bf_dev_tgt, *keys[0],
        bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_SW, datums[0].get()));
  } else {
    RETURN_IF_ERROR(GetAllEntries(real_session->bfrt_session_, bf_dev_tgt,
                                  table, &keys, &datums));
  }

  group_ids->resize(0);
  max_group_sizes->resize(0);
  member_ids->resize(0);
  member_status->resize(0);
  for (size_t i = 0; i < keys.size(); ++i) {
    const std::unique_ptr<bfrt::BfRtTableData>& table_data = datums[i];
    const std::unique_ptr<bfrt::BfRtTableKey>& table_key = keys[i];
    // Key: $SELECTOR_GROUP_ID
    uint64 group_id;
    RETURN_IF_ERROR(GetField(*table_key, "$SELECTOR_GROUP_ID", &group_id));
    group_ids->push_back(group_id);

    // Data: $MAX_GROUP_SIZE
    uint64 max_group_size;
    RETURN_IF_ERROR(GetField(*table_data, "$MAX_GROUP_SIZE", &max_group_size));
    max_group_sizes->push_back(max_group_size);

    // Data: $ACTION_MEMBER_ID
    std::vector<uint32> members;
    RETURN_IF_ERROR(GetField(*table_data, "$ACTION_MEMBER_ID", &members));
    member_ids->push_back(members);

    // Data: $ACTION_MEMBER_STATUS
    std::vector<bool> member_enabled;
    RETURN_IF_ERROR(
        GetField(*table_data, "$ACTION_MEMBER_STATUS", &member_enabled));
    member_status->push_back(member_enabled);
  }

  CHECK_EQ(group_ids->size(), keys.size());
  CHECK_EQ(max_group_sizes->size(), keys.size());
  CHECK_EQ(member_ids->size(), keys.size());
  CHECK_EQ(member_status->size(), keys.size());

  return ::util::OkStatus();
}

::util::StatusOr<uint32> BfSdeWrapper::GetBfRtId(uint32 p4info_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetBfRtId(p4info_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetP4InfoId(uint32 bfrt_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetP4InfoId(bfrt_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetActionSelectorBfRtId(
    uint32 action_profile_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetActionSelectorBfRtId(action_profile_id);
}

::util::StatusOr<uint32> BfSdeWrapper::GetActionProfileBfRtId(
    uint32 action_selector_id) const {
  ::absl::ReaderMutexLock l(&data_lock_);
  return bfrt_id_mapper_->GetActionProfileBfRtId(action_selector_id);
}

::util::Status BfSdeWrapper::SynchronizeCounters(
    int device, std::shared_ptr<BfSdeInterface::SessionInterface> session,
    uint32 table_id, absl::Duration timeout) {
  auto real_session = std::dynamic_pointer_cast<Session>(session);
  CHECK_RETURN_IF_FALSE(real_session);

  const bfrt::BfRtTable* table;
  RETURN_IF_BFRT_ERROR(bfrt_info_->bfrtTableFromIdGet(table_id, &table));

  auto bf_dev_tgt = GetDeviceTarget(device);
  // Sync table counter
  std::set<bfrt::TableOperationsType> supported_ops;
  RETURN_IF_BFRT_ERROR(table->tableOperationsSupported(&supported_ops));
  if (supported_ops.count(bfrt::TableOperationsType::COUNTER_SYNC)) {
    auto sync_notifier = std::make_shared<absl::Notification>();
    std::weak_ptr<absl::Notification> weak_ref(sync_notifier);
    std::unique_ptr<bfrt::BfRtTableOperations> table_op;
    RETURN_IF_BFRT_ERROR(table->operationsAllocate(
        bfrt::TableOperationsType::COUNTER_SYNC, &table_op));
    RETURN_IF_BFRT_ERROR(table_op->counterSyncSet(
        *real_session->bfrt_session_, bf_dev_tgt,
        [table_id, weak_ref](const bf_rt_target_t& dev_tgt, void* cookie) {
          if (auto notifier = weak_ref.lock()) {
            VLOG(1) << "Table counter for table " << table_id << " synced.";
            notifier->Notify();
          } else {
            VLOG(1) << "Notifier expired before table " << table_id
                    << " could be synced.";
          }
        },
        nullptr));
    RETURN_IF_BFRT_ERROR(table->tableOperationsExecute(*table_op.get()));
    // Wait until sync done or timeout.
    if (!sync_notifier->WaitForNotificationWithTimeout(timeout)) {
      return MAKE_ERROR(ERR_OPER_TIMEOUT)
             << "Timeout while syncing (indirect) table counters of table "
             << table_id << ".";
    }
  }

  return ::util::OkStatus();
}

BfSdeWrapper* BfSdeWrapper::CreateSingleton() {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BfSdeWrapper();
  }

  return singleton_;
}

BfSdeWrapper* BfSdeWrapper::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
