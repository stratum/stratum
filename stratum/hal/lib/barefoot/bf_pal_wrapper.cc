// Copyright 2019-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_pal_wrapper.h"

extern "C" {
#include "tofino/bf_pal/bf_pal_port_intf.h"
}

#include <memory>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/barefoot/macros.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/constants.h"

namespace stratum {
namespace hal {
namespace barefoot {

/* static */
constexpr int32 BFPalWrapper::kDefaultMtu;

::util::StatusOr<PortState> BFPalWrapper::PortOperStateGet(int unit,
                                                           uint32 port_id) {
  int state;
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_oper_state_get(static_cast<bf_dev_id_t>(unit),
                                 static_cast<bf_dev_port_t>(port_id), &state));
  return state ? PORT_STATE_UP : PORT_STATE_DOWN;
}

::util::Status BFPalWrapper::PortAllStatsGet(int unit, uint32 port_id,
                                             PortCounters* counters) {
  uint64_t stats[BF_NUM_RMON_COUNTERS];
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_all_stats_get(static_cast<bf_dev_id_t>(unit),
                                static_cast<bf_dev_port_t>(port_id), stats));
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

::util::Status PortStatusChangeCb(int unit, uint32 port_id, bool up,
                                  void* cookie) {
  using PortStatusChangeEvent = BFPalInterface::PortStatusChangeEvent;
  auto* wrapper = static_cast<BFPalWrapper*>(cookie);
  absl::ReaderMutexLock l(&wrapper->port_status_change_event_writer_lock_);
  if (!wrapper->port_status_change_event_writer_) return ::util::OkStatus();
  PortState new_state = up ? PORT_STATE_UP : PORT_STATE_DOWN;
  return wrapper->port_status_change_event_writer_->Write(
      PortStatusChangeEvent{unit, port_id, new_state},
      absl::InfiniteDuration());
}

namespace {

bf_status_t PortStatusChangeCbInternal(bf_dev_id_t dev_id,
                                       bf_dev_port_t dev_port, bool up,
                                       void* cookie) {
  auto status = PortStatusChangeCb(static_cast<int>(dev_id),
                                   static_cast<uint64>(dev_port), up, cookie);
  return status.ok() ? BF_SUCCESS : BF_INTERNAL_ERROR;
}

}  // namespace

::util::Status BFPalWrapper::PortStatusChangeRegisterEventWriter(
    std::unique_ptr<ChannelWriter<PortStatusChangeEvent> > writer) {
  absl::WriterMutexLock l(&port_status_change_event_writer_lock_);
  port_status_change_event_writer_ = std::move(writer);
  RETURN_IF_BFRT_ERROR(
      bf_pal_port_status_notif_reg(PortStatusChangeCbInternal, this));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortStatusChangeUnregisterEventWriter() {
  absl::WriterMutexLock l(&port_status_change_event_writer_lock_);
  port_status_change_event_writer_ = nullptr;
  return ::util::OkStatus();
}

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

::util::Status BFPalWrapper::PortAdd(int unit, uint32 port_id, uint64 speed_bps,
                                     FecMode fec_mode) {
  ASSIGN_OR_RETURN(auto bf_speed, PortSpeedHalToBf(speed_bps));
  ASSIGN_OR_RETURN(auto bf_fec_mode, FecModeHalToBf(fec_mode, speed_bps));
  RETURN_IF_BFRT_ERROR(bf_pal_port_add(static_cast<bf_dev_id_t>(unit),
                                       static_cast<bf_dev_port_t>(port_id),
                                       bf_speed, bf_fec_mode));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortDelete(int unit, uint32 port_id) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_del(static_cast<bf_dev_id_t>(unit),
                                       static_cast<bf_dev_port_t>(port_id)));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortEnable(int unit, uint32 port_id) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_enable(static_cast<bf_dev_id_t>(unit),
                                          static_cast<bf_dev_port_t>(port_id)));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortDisable(int unit, uint32 port_id) {
  RETURN_IF_BFRT_ERROR(bf_pal_port_disable(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id)));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortAutonegPolicySet(int unit, uint32 port_id,
                                                  TriState autoneg) {
  ASSIGN_OR_RETURN(auto autoneg_v, AutonegHalToBf(autoneg));
  RETURN_IF_BFRT_ERROR(bf_pal_port_autoneg_policy_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      autoneg_v));
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortMtuSet(int unit, uint32 port_id, int32 mtu) {
  if (mtu < 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid MTU value.";
  }
  if (mtu == 0) mtu = kDefaultMtu;
  RETURN_IF_BFRT_ERROR(bf_pal_port_mtu_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      static_cast<uint32>(mtu), static_cast<uint32>(mtu)));
  return ::util::OkStatus();
}

bool BFPalWrapper::PortIsValid(int unit, uint32 port_id) {
  return (bf_pal_port_is_valid(unit, port_id) == BF_SUCCESS);
}

::util::Status BFPalWrapper::PortLoopbackModeSet(int unit, uint32 port_id,
                                                 LoopbackState loopback_mode) {
  if (loopback_mode == LOOPBACK_STATE_UNKNOWN) {
    // Do nothing if we try to set loopback mode to the default one (UNKNOWN).
    return ::util::OkStatus();
  }
  ASSIGN_OR_RETURN(bf_loopback_mode_e lp_mode, LoopbackModeToBf(loopback_mode));
  RETURN_IF_BFRT_ERROR(bf_pal_port_loopback_mode_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      lp_mode));

  return ::util::OkStatus();
}

::util::StatusOr<uint32> BFPalWrapper::PortIdFromPortKeyGet(
    int unit, const PortKey& port_key) {
  const int port = port_key.port;
  CHECK_RETURN_IF_FALSE(port >= 0)
      << "Port ID must be non-negative. Attempted to get port " << port
      << " on dev " << unit << ".";

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
      << "Channel must be set for port " << port << " on dev " << unit << ".";

  char port_string[MAX_PORT_HDL_STRING_LEN];
  int r = snprintf(port_string, sizeof(port_string), "%d/%d", port, channel);
  CHECK_RETURN_IF_FALSE(r > 0 && r < sizeof(port_string))
      << "Failed to build port string for port " << port << " channel "
      << channel << " on dev " << unit << ".";

  bf_dev_port_t dev_port;
  RETURN_IF_BFRT_ERROR(bf_pal_port_str_to_dev_port_map(
      static_cast<bf_dev_id_t>(unit), port_string, &dev_port));
  return static_cast<uint32>(dev_port);
}

BFPalWrapper::BFPalWrapper() : port_status_change_event_writer_(nullptr) {}

/* static */
BFPalWrapper* BFPalWrapper::GetSingleton() {
  static BFPalWrapper wrapper;
  return &wrapper;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
