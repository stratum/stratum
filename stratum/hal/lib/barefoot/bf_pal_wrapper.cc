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
  auto bf_status =
      bf_pal_port_oper_state_get(static_cast<bf_dev_id_t>(unit),
                                 static_cast<bf_dev_port_t>(port_id), &state);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when querying port oper status";
  }
  return state ? PORT_STATE_UP : PORT_STATE_DOWN;
}

::util::Status BFPalWrapper::PortAllStatsGet(int unit, uint32 port_id,
                                             PortCounters* counters) {
  uint64_t stats[BF_NUM_RMON_COUNTERS];
  auto bf_status =
      bf_pal_port_all_stats_get(static_cast<bf_dev_id_t>(unit),
                                static_cast<bf_dev_port_t>(port_id), stats);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when querying counters for port "
                                    << port_id << " in unit " << unit;
  }
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
  auto bf_status =
      bf_pal_port_status_notif_reg(PortStatusChangeCbInternal, this);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Error when registering port status notification callback";
  }
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
  auto bf_status = bf_pal_port_add(static_cast<bf_dev_id_t>(unit),
                                   static_cast<bf_dev_port_t>(port_id),
                                   bf_speed, bf_fec_mode);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when adding port with BF_PAL.";
  }
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortDelete(int unit, uint32 port_id) {
  auto bf_status = bf_pal_port_del(static_cast<bf_dev_id_t>(unit),
                                   static_cast<bf_dev_port_t>(port_id));
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when deleting port with BF_PAL.";
  }
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortEnable(int unit, uint32 port_id) {
  auto bf_status = bf_pal_port_enable(static_cast<bf_dev_id_t>(unit),
                                      static_cast<bf_dev_port_t>(port_id));
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when enabling port with BF_PAL.";
  }
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortDisable(int unit, uint32 port_id) {
  auto bf_status = bf_pal_port_disable(static_cast<bf_dev_id_t>(unit),
                                       static_cast<bf_dev_port_t>(port_id));
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL) << "Error when disabling port with BF_PAL.";
  }
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortAutonegPolicySet(int unit, uint32 port_id,
                                                  TriState autoneg) {
  ASSIGN_OR_RETURN(auto autoneg_v, AutonegHalToBf(autoneg));
  auto bf_status = bf_pal_port_autoneg_policy_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      autoneg_v);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Error when setting autoneg policy with BF_PAL.";
  }
  return ::util::OkStatus();
}

::util::Status BFPalWrapper::PortMtuSet(int unit, uint32 port_id, int32 mtu) {
  if (mtu < 0) {
    RETURN_ERROR(ERR_INVALID_PARAM) << "Invalid MTU value.";
  }
  if (mtu == 0) mtu = kDefaultMtu;
  auto bf_status = bf_pal_port_mtu_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      static_cast<uint32>(mtu), static_cast<uint32>(mtu));
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Error when setting port MTU with BF_PAL.";
  }
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
  auto bf_status = bf_pal_port_loopback_mode_set(
      static_cast<bf_dev_id_t>(unit), static_cast<bf_dev_port_t>(port_id),
      lp_mode);
  if (bf_status != BF_SUCCESS) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Error when setting loopback mode on dev " << unit << " port "
           << port_id << ".";
  }
  return ::util::OkStatus();
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
