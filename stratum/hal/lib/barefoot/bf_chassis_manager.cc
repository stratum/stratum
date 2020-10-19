// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include <map>
#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/barefoot/bf_pal_interface.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace barefoot {

using PortStatusChangeEvent = BFPalInterface::PortStatusChangeEvent;
using TransceiverEvent = PhalInterface::TransceiverEvent;

ABSL_CONST_INIT absl::Mutex chassis_lock(absl::kConstInit);

/* static */
constexpr int BFChassisManager::kMaxPortStatusChangeEventDepth;
/* static */
constexpr int BFChassisManager::kMaxXcvrEventDepth;

BFChassisManager::BFChassisManager(PhalInterface* phal_interface,
                                   BFPalInterface* bf_pal_interface)
    : initialized_(false),
      port_status_change_event_channel_(nullptr),
      xcvr_event_writer_id_(kInvalidWriterId),
      phal_interface_(phal_interface),
      bf_pal_interface_(bf_pal_interface),
      unit_to_node_id_(),
      node_id_to_unit_(),
      node_id_to_port_id_to_port_state_() {}

BFChassisManager::~BFChassisManager() = default;

::util::Status BFChassisManager::AddPortHelper(
    uint64 node_id, int unit, uint32 port_id,
    const SingletonPort& singleton_port /* desired config */,
    /* out */ PortConfig* config /* new config */) {
  config->admin_state = ADMIN_STATE_UNKNOWN;

  const auto& config_params = singleton_port.config_params();
  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Invalid admin state for port " << port_id << " in node " << node_id;
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "Unsupported 'diags' admin state for port " << port_id << " in node "
        << node_id;
  }

  LOG(INFO) << "Adding port " << port_id << " in node " << node_id << ".";
  RETURN_IF_ERROR(bf_pal_interface_->PortAdd(
      unit, port_id, singleton_port.speed_bps(), config_params.fec_mode()));
  config->speed_bps = singleton_port.speed_bps();
  config->admin_state = ADMIN_STATE_DISABLED;
  config->fec_mode = config_params.fec_mode();

  if (config_params.mtu() != 0) {
    RETURN_IF_ERROR(
        bf_pal_interface_->PortMtuSet(unit, port_id, config_params.mtu()));
  }
  config->mtu = config_params.mtu();
  if (config_params.autoneg() != TRI_STATE_UNKNOWN) {
    RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(
        unit, port_id, config_params.autoneg()));
  }
  config->autoneg = config_params.autoneg();

  if (config_params.loopback_mode() != LOOPBACK_STATE_UNKNOWN) {
    LOG(INFO) << "Setting port " << port_id << " to loopback mode "
              << config_params.loopback_mode() << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortLoopbackModeSet(
        unit, port_id, config_params.loopback_mode()));
  }
  config->loopback_mode = config_params.loopback_mode();

  if (config_params.admin_state() == ADMIN_STATE_ENABLED) {
    LOG(INFO) << "Enabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
  }

  return ::util::OkStatus();
}

::util::Status BFChassisManager::UpdatePortHelper(
    uint64 node_id, int unit, uint32 port_id,
    const SingletonPort& singleton_port /* desired config */,
    const PortConfig& config_old /* current config */,
    /* out */ PortConfig* config /* new config */) {
  *config = config_old;

  if (!bf_pal_interface_->PortIsValid(unit, port_id)) {
    config->admin_state = ADMIN_STATE_UNKNOWN;
    config->speed_bps.reset();
    config->fec_mode.reset();
    RETURN_ERROR(ERR_INTERNAL)
        << "Port " << port_id << " in node " << node_id << " is not valid.";
  }

  const auto& config_params = singleton_port.config_params();
  if (singleton_port.speed_bps() != config_old.speed_bps) {
    RETURN_IF_ERROR(bf_pal_interface_->PortDisable(unit, port_id));
    RETURN_IF_ERROR(bf_pal_interface_->PortDelete(unit, port_id));

    ::util::Status status =
        AddPortHelper(node_id, unit, port_id, singleton_port, config);
    if (status.ok()) {
      return ::util::OkStatus();
    } else {
      // Revert to the old port configuration
      //   -- make a singleton_port from config_old
      //   -- call AddPortHelper with "old" singleton_port
      SingletonPort port_old =
          BuildSingletonPort(singleton_port.slot(), singleton_port.port(),
                             singleton_port.channel(), *config_old.speed_bps);
      port_old.mutable_config_params()->set_admin_state(config_old.admin_state);
      if (config_old.autoneg)
        port_old.mutable_config_params()->set_autoneg(*config_old.autoneg);
      if (config_old.mtu)
        port_old.mutable_config_params()->set_mtu(*config_old.mtu);
      if (config_old.fec_mode)
        port_old.mutable_config_params()->set_fec_mode(*config_old.fec_mode);
      AddPortHelper(node_id, unit, port_id, port_old, config);
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Could not add port " << port_id << " with new speed "
          << singleton_port.speed_bps() << " to BF SDE.";
    }
  }
  // same for FEC mode
  if (config_params.fec_mode() != config_old.fec_mode) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "The FEC mode for port " << port_id << " in node " << node_id
        << " has changed; you need to delete the port and add it again.";
  }

  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Invalid admin state for port " << port_id << " in node " << node_id;
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    RETURN_ERROR(ERR_UNIMPLEMENTED)
        << "Unsupported 'diags' admin state for port " << port_id << " in node "
        << node_id;
  }

  bool config_changed = false;

  if (config_params.mtu() != config_old.mtu) {
    VLOG(1) << "Mtu for port " << port_id << " in node " << node_id
            << " changed.";
    config->mtu.reset();
    RETURN_IF_ERROR(
        bf_pal_interface_->PortMtuSet(unit, port_id, config_params.mtu()));
    config->mtu = config_params.mtu();
    config_changed = true;
  }
  if (config_params.autoneg() != config_old.autoneg) {
    VLOG(1) << "Autoneg policy for port " << port_id << " in node " << node_id
            << " changed.";
    config->autoneg.reset();
    RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(
        unit, port_id, config_params.autoneg()));
    config->autoneg = config_params.autoneg();
    config_changed = true;
  }
  if (config_params.loopback_mode() != config_old.loopback_mode) {
    config->loopback_mode.reset();
    RETURN_IF_ERROR(bf_pal_interface_->PortLoopbackModeSet(
        unit, port_id, config_params.loopback_mode()));
    config->loopback_mode = config_params.loopback_mode();
    config_changed = true;
  }

  bool need_disable = false, need_enable = false;
  if (config_params.admin_state() == ADMIN_STATE_DISABLED) {
    // if the new admin state is disabled, we need to disable the port if it was
    // previously enabled.
    need_disable = (config_old.admin_state != ADMIN_STATE_DISABLED);
  } else if (config_params.admin_state() == ADMIN_STATE_ENABLED) {
    // if the new admin state is enabled, we need to:
    //  * disable the port if there is a config chaned and the port was
    //    previously enabled
    //  * enable the port if it needs to be disabled first because of a config
    //    change if it is currently disabled
    need_disable =
        config_changed && (config_old.admin_state != ADMIN_STATE_DISABLED);
    need_enable =
        need_disable || (config_old.admin_state == ADMIN_STATE_DISABLED);
  }

  if (need_disable) {
    LOG(INFO) << "Disabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortDisable(unit, port_id));
    config->admin_state = ADMIN_STATE_DISABLED;
  }
  if (need_enable) {
    LOG(INFO) << "Enabling port " << port_id << " in node " << node_id << ".";
    RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
  }

  return ::util::OkStatus();
}

::util::Status BFChassisManager::PushChassisConfig(
    const ChassisConfig& config) {
  if (!initialized_) RETURN_IF_ERROR(RegisterEventWriters());

  // new maps
  std::map<int, uint64> unit_to_node_id;
  std::map<uint64, int> node_id_to_unit;
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state;
  std::map<uint64, std::map<uint32, PortConfig>>
      node_id_to_port_id_to_port_config;
  std::map<uint64, std::map<uint32, PortKey>>
      node_id_to_port_id_to_singleton_port_key;
  std::map<PortKey, HwState> xcvr_port_key_to_xcvr_state;

  int unit(0);
  for (auto& node : config.nodes()) {
    unit_to_node_id[unit] = node.id();
    node_id_to_unit[node.id()] = unit;
    unit++;
  }

  for (auto singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();

    auto* unit = gtl::FindOrNull(node_id_to_unit, node_id);
    if (unit == nullptr) {
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Invalid ChassisConfig, unknown node id " << node_id
          << " for port " << port_id << ".";
    }
    node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    node_id_to_port_id_to_port_config[node_id][port_id] = PortConfig();
    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    node_id_to_port_id_to_singleton_port_key[node_id][port_id] =
        singleton_port_key;
    PortKey port_group_key(singleton_port.slot(), singleton_port.port());
    xcvr_port_key_to_xcvr_state[port_group_key] = HW_STATE_UNKNOWN;
  }

  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  for (auto singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();
    // we checked that node_id was valid in the previous loop
    auto unit = node_id_to_unit[node_id];

    // TODO(antonin): we currently ignore slot, port and channel (note that
    // Stratum requires slot and port to be set). We require id to be set to the
    // Tofino device port.

    const PortConfig* config_old = nullptr;
    const auto* port_id_to_port_config_old =
        gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
    if (port_id_to_port_config_old != nullptr) {
      config_old = gtl::FindOrNull(*port_id_to_port_config_old, port_id);
    }

    auto& config = node_id_to_port_id_to_port_config[node_id][port_id];
    if (config_old == nullptr) {  // new port
      // if anything fails, config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      APPEND_STATUS_IF_ERROR(status, AddPortHelper(node_id, unit, port_id,
                                                   singleton_port, &config));
    } else {  // port already exists, config may have changed
      if (config_old->admin_state == ADMIN_STATE_UNKNOWN) {
        // something is wrong with the port, we make sure the port is deleted
        // first (and ignore the error status if there is one), then add the
        // port again.
        if (bf_pal_interface_->PortIsValid(unit, port_id)) {
          bf_pal_interface_->PortDelete(unit, port_id);
        }
        APPEND_STATUS_IF_ERROR(status, AddPortHelper(node_id, unit, port_id,
                                                     singleton_port, &config));
        continue;
      }

      // diff configs and apply necessary changes

      // sanity-check: if admin_state is not ADMIN_STATE_UNKNOWN, then the port
      // was added and the speed_bps was set.
      if (!config_old->speed_bps) {
        RETURN_ERROR(ERR_INTERNAL)
            << "Invalid internal state in BFChassisManager, "
            << "speed_bps field should contain a value";
      }

      // if anything fails, config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      APPEND_STATUS_IF_ERROR(
          status, UpdatePortHelper(node_id, unit, port_id, singleton_port,
                                   *config_old, &config));
    }
  }

  for (const auto& node_ports_old : node_id_to_port_id_to_port_config_) {
    auto node_id = node_ports_old.first;
    for (const auto& port_old : node_ports_old.second) {
      auto port_id = port_old.first;
      if (node_id_to_port_id_to_port_config.count(node_id) > 0 &&
          node_id_to_port_id_to_port_config[node_id].count(port_id) > 0) {
        continue;
      }
      auto unit = node_id_to_unit_[node_id];
      // remove ports which are no longer present in the ChassisConfig
      LOG(INFO) << "Deleting port " << port_id << " in node " << node_id << ".";
      APPEND_STATUS_IF_ERROR(status,
                             bf_pal_interface_->PortDelete(unit, port_id));
    }
  }

  unit_to_node_id_ = unit_to_node_id;
  node_id_to_unit_ = node_id_to_unit;
  node_id_to_port_id_to_port_state_ = node_id_to_port_id_to_port_state;
  node_id_to_port_id_to_port_config_ = node_id_to_port_id_to_port_config;
  node_id_to_port_id_to_singleton_port_key_ =
      node_id_to_port_id_to_singleton_port_key;
  xcvr_port_key_to_xcvr_state_ = xcvr_port_key_to_xcvr_state;
  initialized_ = true;

  return status;
}

::util::Status BFChassisManager::VerifyChassisConfig(
    const ChassisConfig& config) {
  if (config.trunk_ports_size()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Trunk ports are not supported on Tofino.";
  }
  if (config.port_groups_size()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Port groups are not supported on Tofino.";
  }

  // If the class is initialized, we also need to check if the new config will
  // require a change in the port layout. If so, report reboot required.
  if (initialized_) {
    std::map<uint64, std::map<uint32, PortKey>>
        node_id_to_port_id_to_singleton_port_key;

    for (const auto& singleton_port : config.singleton_ports()) {
      uint32 port_id = singleton_port.id();
      uint64 node_id = singleton_port.node();

      node_id_to_port_id_to_singleton_port_key[node_id][port_id] =
          PortKey(singleton_port.slot(), singleton_port.port(),
                  singleton_port.channel());
    }

    if (node_id_to_port_id_to_singleton_port_key !=
        node_id_to_port_id_to_singleton_port_key_) {
      return MAKE_ERROR(ERR_REBOOT_REQUIRED)
             << "The switch is already initialized, but we detected the "
             << "newly pushed config requires a change in the port layout. "
             << "The stack needs to be rebooted to finish config push.";
    }
  }
  return ::util::OkStatus();
}

::util::Status BFChassisManager::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BFChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::StatusOr<const BFChassisManager::PortConfig*>
BFChassisManager::GetPortConfig(uint64 node_id, uint32 port_id) const {
  auto* port_id_to_config =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_config != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortConfig* config = gtl::FindOrNull(*port_id_to_config, port_id);
  CHECK_RETURN_IF_FALSE(config != nullptr)
      << "Port " << port_id << " is not configured or not known for node "
      << node_id << ".";
  return config;
}

::util::StatusOr<DataResponse> BFChassisManager::GetPortData(
    const DataRequest::Request& request) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  DataResponse resp;
  using Request = DataRequest::Request;
  switch (request.request_case()) {
    case Request::kOperStatus: {
      ASSIGN_OR_RETURN(auto port_state,
                       GetPortState(request.oper_status().node_id(),
                                    request.oper_status().port_id()));
      resp.mutable_oper_status()->set_state(port_state);
      break;
    }
    case Request::kAdminStatus: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.admin_status().node_id(),
                                     request.admin_status().port_id()));
      resp.mutable_admin_status()->set_state(config->admin_state);
      break;
    }
    case Request::kPortSpeed: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.port_speed().node_id(),
                                     request.port_speed().port_id()));
      if (config->speed_bps)
        resp.mutable_port_speed()->set_speed_bps(*config->speed_bps);
      break;
    }
    case Request::kNegotiatedPortSpeed: {
      ASSIGN_OR_RETURN(
          auto* config,
          GetPortConfig(request.negotiated_port_speed().node_id(),
                        request.negotiated_port_speed().port_id()));
      if (!config->speed_bps) break;
      ASSIGN_OR_RETURN(auto port_state,
                       GetPortState(request.negotiated_port_speed().node_id(),
                                    request.negotiated_port_speed().port_id()));
      if (port_state != PORT_STATE_UP) break;
      resp.mutable_negotiated_port_speed()->set_speed_bps(*config->speed_bps);
      break;
    }
    case Request::kPortCounters: {
      RETURN_IF_ERROR(GetPortCounters(request.port_counters().node_id(),
                                      request.port_counters().port_id(),
                                      resp.mutable_port_counters()));
      break;
    }
    case Request::kAutonegStatus: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.autoneg_status().node_id(),
                                     request.autoneg_status().port_id()));
      if (config->autoneg)
        resp.mutable_autoneg_status()->set_state(*config->autoneg);
      break;
    }
    case Request::kFrontPanelPortInfo: {
      RETURN_IF_ERROR(
          GetFrontPanelPortInfo(request.front_panel_port_info().node_id(),
                                request.front_panel_port_info().port_id(),
                                resp.mutable_front_panel_port_info()));
      break;
    }
    case Request::kFecStatus: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.fec_status().node_id(),
                                     request.fec_status().port_id()));
      if (config->fec_mode)
        resp.mutable_fec_status()->set_mode(*config->fec_mode);
      break;
    }
    case Request::kLoopbackStatus: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.loopback_status().node_id(),
                                     request.loopback_status().port_id()));
      if (config->loopback_mode)
        resp.mutable_loopback_status()->set_state(*config->loopback_mode);
      break;
    }
    default:
      RETURN_ERROR(ERR_INTERNAL) << "Not supported yet";
  }
  return resp;
}

::util::StatusOr<PortState> BFChassisManager::GetPortState(uint64 node_id,
                                                           uint32 port_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto unit, GetUnitFromNodeId(node_id));

  auto* port_id_to_port_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortState* port_state_ptr =
      gtl::FindOrNull(*port_id_to_port_state, port_id);
  // TODO(antonin): Once we implement PushChassisConfig, port_state_ptr should
  // never be NULL
  if (port_state_ptr != nullptr && *port_state_ptr != PORT_STATE_UNKNOWN) {
    return *port_state_ptr;
  }

  // If state is unknown, query the state
  LOG(INFO) << "Querying state of port " << port_id << " in node " << node_id
            << ".";
  ASSIGN_OR_RETURN(auto port_state,
                   bf_pal_interface_->PortOperStateGet(unit, port_id));
  LOG(INFO) << "State of port " << port_id << " in node " << node_id << ": "
            << PrintPortState(port_state);
  return port_state;
}

::util::Status BFChassisManager::GetPortCounters(uint64 node_id, uint32 port_id,
                                                 PortCounters* counters) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto unit, GetUnitFromNodeId(node_id));
  return bf_pal_interface_->PortAllStatsGet(unit, port_id, counters);
  return ::util::OkStatus();
}

::util::StatusOr<std::map<uint64, int>> BFChassisManager::GetNodeIdToUnitMap()
    const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  return node_id_to_unit_;
}

::util::Status BFChassisManager::ReplayPortsConfig(uint64 node_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto unit, GetUnitFromNodeId(node_id));

  for (auto& p : node_id_to_port_id_to_port_state_[node_id])
    p.second = PORT_STATE_UNKNOWN;

  LOG(INFO) << "Replaying ports for node " << node_id << ".";

  auto replay_one_port = [node_id, unit, this](
                             uint32 port_id, const PortConfig& config,
                             PortConfig* config_new) -> ::util::Status {
    VLOG(1) << "Replaying port " << port_id << " in node " << node_id << ".";

    if (config.admin_state == ADMIN_STATE_UNKNOWN) {
      LOG(WARNING) << "Port " << port_id << " in node " << node_id
                   << " was not configured properly, so skipping replay.";
      return ::util::OkStatus();
    }

    if (!config.speed_bps) {
      RETURN_ERROR(ERR_INTERNAL)
          << "Invalid internal state in BFChassisManager, "
          << "speed_bps field should contain a value";
    }
    if (!config.fec_mode) {
      RETURN_ERROR(ERR_INTERNAL)
          << "Invalid internal state in BFChassisManager, "
          << "fec_mode field should contain a value";
    }

    RETURN_IF_ERROR(bf_pal_interface_->PortAdd(unit, port_id, *config.speed_bps,
                                               *config.fec_mode));
    config_new->speed_bps = *config.speed_bps;
    config_new->admin_state = ADMIN_STATE_DISABLED;
    config_new->fec_mode = *config.fec_mode;

    if (config.mtu) {
      RETURN_IF_ERROR(
          bf_pal_interface_->PortMtuSet(unit, port_id, *config.mtu));
      config_new->mtu = *config.mtu;
    }
    if (config.autoneg) {
      RETURN_IF_ERROR(bf_pal_interface_->PortAutonegPolicySet(unit, port_id,
                                                              *config.autoneg));
      config_new->autoneg = *config.autoneg;
    }
    if (config.loopback_mode) {
      RETURN_IF_ERROR(bf_pal_interface_->PortLoopbackModeSet(
          unit, port_id, *config.loopback_mode));
      config_new->loopback_mode = *config.loopback_mode;
    }

    if (config.admin_state == ADMIN_STATE_ENABLED) {
      VLOG(1) << "Enabling port " << port_id << " in node " << node_id << ".";
      RETURN_IF_ERROR(bf_pal_interface_->PortEnable(unit, port_id));
      config_new->admin_state = ADMIN_STATE_ENABLED;
    }

    return ::util::OkStatus();
  };

  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  for (auto& p : node_id_to_port_id_to_port_config_[node_id]) {
    PortConfig config_new;
    APPEND_STATUS_IF_ERROR(status,
                           replay_one_port(p.first, p.second, &config_new));
    p.second = config_new;
  }

  return status;
}

::util::Status BFChassisManager::ResetPortsConfig(uint64 node_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  auto* port_id_to_config =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_config != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  auto* port_id_to_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_state != nullptr)
      << "Node " << node_id << " has a configuration mismatch.";
  for (auto& p : *port_id_to_config) p.second = PortConfig();
  for (auto& p : *port_id_to_state) p.second = PORT_STATE_UNKNOWN;
  return ::util::OkStatus();
}

::util::Status BFChassisManager::GetFrontPanelPortInfo(
    uint64 node_id, uint32 port_id, FrontPanelPortInfo* fp_port_info) {
  auto* port_id_to_port_key =
      gtl::FindOrNull(node_id_to_port_id_to_singleton_port_key_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_key != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  auto* port_key = gtl::FindOrNull(*port_id_to_port_key, port_id);
  CHECK_RETURN_IF_FALSE(port_key != nullptr)
      << "Node " << node_id << ", port " << port_id
      << " is not configured or not known.";
  return phal_interface_->GetFrontPanelPortInfo(port_key->slot, port_key->port,
                                                fp_port_info);
}

std::unique_ptr<BFChassisManager> BFChassisManager::CreateInstance(
    PhalInterface* phal_interface, BFPalInterface* bf_pal_interface) {
  return absl::WrapUnique(
      new BFChassisManager(phal_interface, bf_pal_interface));
}

void BFChassisManager::SendPortOperStateGnmiEvent(uint64 node_id,
                                                  uint32 port_id,
                                                  PortState new_state) {
  absl::ReaderMutexLock l(&gnmi_event_lock_);
  if (!gnmi_event_writer_) return;
  // Allocate and initialize a PortOperStateChangedEvent event and pass it to
  // the gNMI publisher using the gNMI event notification channel.
  // The GnmiEventPtr is a smart pointer (shared_ptr<>) and it takes care of
  // the memory allocated to this event object once the event is handled by
  // the GnmiPublisher.
  if (!gnmi_event_writer_->Write(GnmiEventPtr(
          new PortOperStateChangedEvent(node_id, port_id, new_state)))) {
    // Remove WriterInterface if it is no longer operational.
    gnmi_event_writer_.reset();
  }
}

void BFChassisManager::ReadPortStatusChangeEvents() {
  PortStatusChangeEvent event;
  while (true) {
    // port_status_change_event_reader_ does not need to be protected by a mutex
    // because this thread is the only one accessing it. It is assigned in
    // RegisterEventWriters and then left untouched until UnregisterEventWriters
    // is called. UnregisterEventWriters joins this thread before resetting the
    // reader.
    int code =
        port_status_change_event_reader_->Read(&event, absl::InfiniteDuration())
            .error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle received message.
    {
      absl::WriterMutexLock l(&chassis_lock);
      const uint64* node_id = gtl::FindOrNull(unit_to_node_id_, event.unit);
      if (node_id == nullptr) {
        LOG(ERROR) << "Unknown unit / device id " << event.unit << ".";
        continue;
      }
      auto* state = gtl::FindOrNull(node_id_to_port_id_to_port_state_[*node_id],
                                    event.port_id);
      if (state == nullptr) {
        // We get a notification for all ports, even ports that were not added,
        // when doing a Fast Refresh, which can be confusing, so we use VLOG
        // instead.
        // LOG(ERROR) << "Unknown port " << event.port_id << " in node "
        //            << *node_id << ".";
        VLOG(1) << "Unknown port " << event.port_id << " in node " << *node_id
                << ".";
        continue;
      }
      LOG(INFO) << "State of port " << event.port_id << " in node " << *node_id
                << ": " << PrintPortState(event.state) << ".";
      *state = event.state;
      SendPortOperStateGnmiEvent(*node_id, event.port_id, event.state);
    }
  }
}

void BFChassisManager::ReadTransceiverEvents() {
  TransceiverEvent event;
  while (true) {
    // xcvr_event_reader_ does not need to be protected by a mutex because this
    // thread is the only one accessing it. It is assigned in
    // RegisterEventWriters and then left untouched until UnregisterEventWriters
    // is called. UnregisterEventWriters joins this thread before resetting the
    // reader.
    int code =
        xcvr_event_reader_->Read(&event, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle received message.
    TransceiverEventHandler(event.slot, event.port, event.state);
  }
}

void BFChassisManager::TransceiverEventHandler(int slot, int port,
                                               HwState new_state) {
  absl::WriterMutexLock l(&chassis_lock);

  PortKey xcvr_port_key(slot, port);
  LOG(INFO) << "Transceiver event for port " << xcvr_port_key.ToString() << ": "
            << HwState_Name(new_state) << ".";

  // See if we know about this transceiver module. Find a mutable state pointer
  // so we can override it later.
  HwState* mutable_state =
      gtl::FindOrNull(xcvr_port_key_to_xcvr_state_, xcvr_port_key);
  if (mutable_state == nullptr) {
    LOG(ERROR) << "Detected unknown " << xcvr_port_key.ToString()
               << " in TransceiverEventHandler. This should not happen!";
    return;
  }
  HwState old_state = *mutable_state;

  // This handler is supposed to return present or non present for the state of
  // the transceiver modules. Other values do no make sense.
  if (new_state != HW_STATE_PRESENT && new_state != HW_STATE_NOT_PRESENT) {
    LOG(ERROR) << "Invalid state for transceiver " << xcvr_port_key.ToString()
               << " in TransceiverEventHandler: " << HwState_Name(new_state)
               << ".";
    return;
  }

  // Discard some invalid situations and report the error. Then save the new
  // state
  if (old_state == HW_STATE_READY && new_state == HW_STATE_PRESENT) {
    LOG(ERROR) << "Got present for a ready transceiver "
               << xcvr_port_key.ToString() << " in TransceiverEventHandler.";
    return;
  }
  if (old_state == HW_STATE_UNKNOWN && new_state == HW_STATE_NOT_PRESENT) {
    LOG(ERROR) << "Got not-present for an unknown transceiver "
               << xcvr_port_key.ToString() << " in TransceiverEventHandler.";
    return;
  }
  *mutable_state = new_state;

  // TODO(antonin): set autoneg based on media type...
  FrontPanelPortInfo fp_port_info;
  auto status =
      phal_interface_->GetFrontPanelPortInfo(slot, port, &fp_port_info);
  if (!status.ok()) {
    LOG(ERROR) << "Failure in TransceiverEventHandler: " << status;
    return;
  }

  // Finally, before we exit we make sure if the port was HW_STATE_PRESENT,
  // it is set to HW_STATE_READY to show it has been configured and ready.
  if (*mutable_state == HW_STATE_PRESENT) {
    LOG(INFO) << "Transceiver " << xcvr_port_key.ToString() << " is ready.";
    *mutable_state = HW_STATE_READY;
  }
}

::util::Status BFChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }

  {
    port_status_change_event_channel_ =
        Channel<PortStatusChangeEvent>::Create(kMaxPortStatusChangeEventDepth);
    // Create and hand-off Writer to the BFPalInterface.
    auto writer = ChannelWriter<PortStatusChangeEvent>::Create(
        port_status_change_event_channel_);
    RETURN_IF_ERROR(bf_pal_interface_->PortStatusChangeRegisterEventWriter(
        std::move(writer)));
    LOG(INFO) << "Port status notification callback registered successfully";

    port_status_change_event_reader_ =
        ChannelReader<PortStatusChangeEvent>::Create(
            port_status_change_event_channel_);
    port_status_change_event_thread_ =
        std::thread([this]() { this->ReadPortStatusChangeEvents(); });
  }

  if (xcvr_event_writer_id_ == kInvalidWriterId) {
    xcvr_event_channel_ = Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);
    auto writer = ChannelWriter<TransceiverEvent>::Create(xcvr_event_channel_);
    int priority = PhalInterface::kTransceiverEventWriterPriorityHigh;
    ASSIGN_OR_RETURN(xcvr_event_writer_id_,
                     phal_interface_->RegisterTransceiverEventWriter(
                         std::move(writer), priority));

    xcvr_event_reader_ =
        ChannelReader<TransceiverEvent>::Create(xcvr_event_channel_);
    xcvr_event_thread_ =
        std::thread([this]() { this->ReadTransceiverEvents(); });
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Transceiver event handler already registered.";
  }

  return ::util::OkStatus();
}

::util::Status BFChassisManager::UnregisterEventWriters() {
  absl::WriterMutexLock l(&chassis_lock);
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(
      status, bf_pal_interface_->PortStatusChangeUnregisterEventWriter());
  if (!port_status_change_event_channel_->Close()) {
    APPEND_ERROR(status)
        << "Error when closing port status change event channel.";
  }
  if (xcvr_event_writer_id_ != kInvalidWriterId) {
    APPEND_STATUS_IF_ERROR(status,
                           phal_interface_->UnregisterTransceiverEventWriter(
                               xcvr_event_writer_id_));
    xcvr_event_writer_id_ = kInvalidWriterId;
    if (!xcvr_event_channel_->Close()) {
      APPEND_ERROR(status) << "Error when closing transceiver event channel.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Transceiver event handler not registered.";
  }

  port_status_change_event_thread_.join();
  // Once the thread is joined, it is safe to reset these pointers.
  port_status_change_event_reader_ = nullptr;
  port_status_change_event_channel_ = nullptr;

  xcvr_event_thread_.join();
  xcvr_event_reader_ = nullptr;
  xcvr_event_channel_ = nullptr;
  return status;
}

::util::StatusOr<int> BFChassisManager::GetUnitFromNodeId(
    uint64 node_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const int* unit = gtl::FindOrNull(node_id_to_unit_, node_id);
  CHECK_RETURN_IF_FALSE(unit != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  return *unit;
}

void BFChassisManager::CleanupInternalState() {
  unit_to_node_id_.clear();
  node_id_to_unit_.clear();
  node_id_to_port_id_to_port_state_.clear();
  node_id_to_port_id_to_port_config_.clear();
  node_id_to_port_id_to_singleton_port_key_.clear();
  xcvr_port_key_to_xcvr_state_.clear();
}

::util::Status BFChassisManager::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  {
    absl::ReaderMutexLock l(&chassis_lock);
    if (!initialized_) return status;
  }
  // It is fine to release the chassis lock here (it is actually needed to call
  // UnregisterEventWriters or there would be a deadlock). Because initialized_
  // is set to true, RegisterEventWriters cannot be called.
  APPEND_STATUS_IF_ERROR(status, UnregisterEventWriters());
  {
    absl::WriterMutexLock l(&chassis_lock);
    initialized_ = false;
    CleanupInternalState();
  }
  return status;
}

}  // namespace barefoot
}  // namespace hal
}  // namespace stratum
