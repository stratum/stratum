// Copyright 2018-present Barefoot Networks, Inc.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/barefoot/bf_chassis_manager.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "stratum/glue/integral_types.h"
#include "stratum/hal/lib/barefoot/bfrt_constants.h"
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

using PortStatusEvent = BfSdeInterface::PortStatusEvent;
using TransceiverEvent = PhalInterface::TransceiverEvent;

/* static */
constexpr int BfChassisManager::kMaxPortStatusEventDepth;
/* static */
constexpr int BfChassisManager::kMaxXcvrEventDepth;

BfChassisManager::BfChassisManager(OperationMode mode,
                                   PhalInterface* phal_interface,
                                   BfSdeInterface* bf_sde_interface)
    : mode_(mode),
      initialized_(false),
      port_status_event_channel_(nullptr),
      xcvr_event_writer_id_(kInvalidWriterId),
      xcvr_event_channel_(nullptr),
      gnmi_event_writer_(nullptr),
      device_to_node_id_(),
      node_id_to_device_(),
      node_id_to_port_id_to_port_state_(),
      node_id_to_port_id_to_time_last_changed_(),
      node_id_to_port_id_to_port_config_(),
      node_id_to_port_id_to_singleton_port_key_(),
      node_id_to_port_id_to_sdk_port_id_(),
      node_id_to_sdk_port_id_to_port_id_(),
      node_id_to_deflect_on_drop_config_(),
      node_id_to_qos_config_(),
      xcvr_port_key_to_xcvr_state_(),
      phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      bf_sde_interface_(ABSL_DIE_IF_NULL(bf_sde_interface)) {}

BfChassisManager::BfChassisManager()
    : mode_(OPERATION_MODE_STANDALONE),
      initialized_(false),
      port_status_event_channel_(nullptr),
      xcvr_event_writer_id_(kInvalidWriterId),
      xcvr_event_channel_(nullptr),
      gnmi_event_writer_(nullptr),
      device_to_node_id_(),
      node_id_to_device_(),
      node_id_to_port_id_to_port_state_(),
      node_id_to_port_id_to_time_last_changed_(),
      node_id_to_port_id_to_port_config_(),
      node_id_to_port_id_to_singleton_port_key_(),
      node_id_to_port_id_to_sdk_port_id_(),
      node_id_to_sdk_port_id_to_port_id_(),
      node_id_to_deflect_on_drop_config_(),
      node_id_to_qos_config_(),
      xcvr_port_key_to_xcvr_state_(),
      phal_interface_(nullptr),
      bf_sde_interface_(nullptr) {}

BfChassisManager::~BfChassisManager() {
  // NOTE: We should not detach any device or unregister any handler in the
  // deconstructor as phal_interface_ or bf_sde_interface_ can be deleted before
  // this class. Make sure you call Shutdown() before deleting the class
  // instance.
  if (initialized_) {
    LOG(ERROR) << "Deleting BfChassisManager while initialized_ is still "
               << "true. You did not call Shutdown() before deleting the class "
               << "instance. This can lead to unexpected behavior.";
  }
  CleanupInternalState();
}

::util::Status BfChassisManager::AddPortHelper(
    uint64 node_id, int device, uint32 sdk_port_id,
    const SingletonPort& singleton_port /* desired config */,
    /* out */ PortConfig* config /* new config */) {
  config->admin_state = ADMIN_STATE_UNKNOWN;
  // SingletonPort ID is the SDN/Stratum port ID
  uint32 port_id = singleton_port.id();

  const auto& config_params = singleton_port.config_params();
  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid admin state for port " << port_id << " in node "
           << node_id << " (SDK Port " << sdk_port_id << ").";
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    return MAKE_ERROR(ERR_UNIMPLEMENTED)
           << "Unsupported 'diags' admin state for port " << port_id
           << " in node " << node_id << " (SDK Port " << sdk_port_id << ").";
  }

  RETURN_IF_ERROR(bf_sde_interface_->AddPort(device, sdk_port_id,
                                             singleton_port.speed_bps(),
                                             config_params.fec_mode()));
  LOG(INFO) << "Added port " << port_id << " in node " << node_id
            << " (SDK Port " << sdk_port_id << ").";
  config->speed_bps = singleton_port.speed_bps();
  config->admin_state = ADMIN_STATE_DISABLED;
  config->fec_mode = config_params.fec_mode();

  if (config_params.mtu() != 0) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortMtu(device, sdk_port_id,
                                                  config_params.mtu()));
    VLOG(1) << "Set MTU " << config_params.mtu() << " for port " << port_id
            << " in node " << node_id << " (SDK Port " << sdk_port_id << ").";
  }
  config->mtu = config_params.mtu();

  if (config_params.autoneg() != TRI_STATE_UNKNOWN) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortAutonegPolicy(
        device, sdk_port_id, config_params.autoneg()));
    VLOG(1) << "Set autoneg policy " << TriState_Name(config_params.autoneg())
            << " for port " << port_id << " in node " << node_id
            << " (SDK Port " << sdk_port_id << ").";
  }
  config->autoneg = config_params.autoneg();

  if (config_params.loopback_mode() != LOOPBACK_STATE_UNKNOWN) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortLoopbackMode(
        device, sdk_port_id, config_params.loopback_mode()));
    VLOG(1) << "Set loopback mode "
            << LoopbackState_Name(config_params.loopback_mode()) << " for port "
            << port_id << " in node " << node_id << " (SDK Port " << sdk_port_id
            << ").";
  }
  config->loopback_mode = config_params.loopback_mode();

  if (config_params.admin_state() == ADMIN_STATE_ENABLED) {
    RETURN_IF_ERROR(bf_sde_interface_->EnablePort(device, sdk_port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
    LOG(INFO) << "Enabled port " << port_id << " in node " << node_id
              << " (SDK Port " << sdk_port_id << ").";
  }

  RETURN_IF_ERROR(bf_sde_interface_->EnablePortShaping(device, sdk_port_id,
                                                       TRI_STATE_FALSE));
  config->shaping_config.reset();

  return ::util::OkStatus();
}

::util::Status BfChassisManager::UpdatePortHelper(
    uint64 node_id, int device, uint32 sdk_port_id,
    const SingletonPort& singleton_port /* desired config */,
    const PortConfig& config_old /* current config */,
    /* out */ PortConfig* config /* new config */) {
  *config = config_old;
  // SingletonPort ID is the SDN/Stratum port ID
  uint32 port_id = singleton_port.id();

  if (!bf_sde_interface_->IsValidPort(device, sdk_port_id)) {
    config->admin_state = ADMIN_STATE_UNKNOWN;
    config->speed_bps.reset();
    config->fec_mode.reset();
    return MAKE_ERROR(ERR_INTERNAL)
           << "Port " << port_id << " in node " << node_id << " is not valid"
           << " (SDK Port " << sdk_port_id << ").";
  }

  const auto& config_params = singleton_port.config_params();
  if (singleton_port.speed_bps() != config_old.speed_bps) {
    RETURN_IF_ERROR(bf_sde_interface_->DisablePort(device, sdk_port_id));
    RETURN_IF_ERROR(bf_sde_interface_->DeletePort(device, sdk_port_id));

    ::util::Status status =
        AddPortHelper(node_id, device, sdk_port_id, singleton_port, config);
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
      AddPortHelper(node_id, device, sdk_port_id, port_old, config);
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Could not add port " << port_id << " with new speed "
             << singleton_port.speed_bps() << " to BF SDE"
             << " (SDK Port " << sdk_port_id << ").";
    }
  }
  // same for FEC mode
  if (config_params.fec_mode() != config_old.fec_mode) {
    return MAKE_ERROR(ERR_UNIMPLEMENTED)
           << "The FEC mode for port " << port_id << " in node " << node_id
           << " has changed; you need to delete the port and add it again"
           << " (SDK Port " << sdk_port_id << ").";
  }

  if (config_params.admin_state() == ADMIN_STATE_UNKNOWN) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid admin state for port " << port_id << " in node "
           << node_id << " (SDK Port " << sdk_port_id << ").";
  }
  if (config_params.admin_state() == ADMIN_STATE_DIAG) {
    return MAKE_ERROR(ERR_UNIMPLEMENTED)
           << "Unsupported 'diags' admin state for port " << port_id
           << " in node " << node_id << " (SDK Port " << sdk_port_id << ").";
  }

  bool config_changed = false;

  if (config_params.mtu() != config_old.mtu) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortMtu(device, sdk_port_id,
                                                  config_params.mtu()));
    config->mtu = config_params.mtu();
    config_changed = true;
    VLOG(1) << "Set MTU " << config_params.mtu() << " for port " << port_id
            << " in node " << node_id << " (SDK Port " << sdk_port_id << ").";
  }
  if (config_params.autoneg() != config_old.autoneg) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortAutonegPolicy(
        device, sdk_port_id, config_params.autoneg()));
    config->autoneg = config_params.autoneg();
    config_changed = true;
    VLOG(1) << "Set autoneg policy " << TriState_Name(config_params.autoneg())
            << " for port " << port_id << " in node " << node_id
            << " (SDK Port " << sdk_port_id << ").";
  }
  if (config_params.loopback_mode() != config_old.loopback_mode) {
    RETURN_IF_ERROR(bf_sde_interface_->SetPortLoopbackMode(
        device, sdk_port_id, config_params.loopback_mode()));
    config->loopback_mode = config_params.loopback_mode();
    config_changed = true;
    VLOG(1) << "Set loopback mode "
            << LoopbackState_Name(config_params.loopback_mode()) << " for port "
            << port_id << " in node " << node_id << " (SDK Port " << sdk_port_id
            << ").";
  }
  // Due to lack of information about the new shaping config here, we always
  // disable it. If required, it will be configured later.
  config->shaping_config.reset();
  RETURN_IF_ERROR(bf_sde_interface_->EnablePortShaping(device, sdk_port_id,
                                                       TRI_STATE_FALSE));

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
    RETURN_IF_ERROR(bf_sde_interface_->DisablePort(device, sdk_port_id));
    config->admin_state = ADMIN_STATE_DISABLED;
    LOG(INFO) << "Disabled port " << port_id << " in node " << node_id
              << " (SDK Port " << sdk_port_id << ").";
  }
  if (need_enable) {
    RETURN_IF_ERROR(bf_sde_interface_->EnablePort(device, sdk_port_id));
    config->admin_state = ADMIN_STATE_ENABLED;
    LOG(INFO) << "Enabled port " << port_id << " in node " << node_id
              << " (SDK Port " << sdk_port_id << ").";
  }

  return ::util::OkStatus();
}

::util::Status BfChassisManager::PushChassisConfig(
    const ChassisConfig& config) {
  if (!initialized_) RETURN_IF_ERROR(RegisterEventWriters());

  // new maps
  std::map<int, uint64> device_to_node_id;
  std::map<uint64, int> node_id_to_device;
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state;
  std::map<uint64, std::map<uint32, absl::Time>>
      node_id_to_port_id_to_time_last_changed;
  std::map<uint64, std::map<uint32, PortConfig>>
      node_id_to_port_id_to_port_config;
  std::map<uint64, std::map<uint32, PortKey>>
      node_id_to_port_id_to_singleton_port_key;
  std::map<uint64, std::map<uint32, uint32>> node_id_to_port_id_to_sdk_port_id;
  std::map<uint64, std::map<uint32, uint32>> node_id_to_sdk_port_id_to_port_id;
  std::map<uint64, TofinoConfig::DeflectOnPacketDropConfig>
      node_id_to_deflect_on_drop_config;
  std::map<uint64, TofinoConfig::TofinoQosConfig> node_id_to_qos_config;
  std::map<PortKey, HwState> xcvr_port_key_to_xcvr_state;

  {
    int device = 0;
    for (const auto& node : config.nodes()) {
      device_to_node_id[device] = node.id();
      node_id_to_device[node.id()] = device;
      device++;
    }
  }

  for (const auto& singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();

    auto* device = gtl::FindOrNull(node_id_to_device, node_id);
    if (device == nullptr) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid ChassisConfig, unknown node id " << node_id
             << " for port " << port_id << ".";
    }
    // Reset port state to unknown, we'll update it on the first port status
    // event or when requested.
    node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    // If (node_id, port_id) already exists as a key in any of
    // node_id_to_port_id_to_{time_last_changed,port_state}_, we keep the last
    // known value. Otherwise, we assume this is the first time we are
    // seeing this port and set the state to unknown or zero.
    // TODO(max): Check if we can retain more state. PushChassisConfig should
    // not clear the entire state if not necessary. Only pipeline pushes reset
    // the ASIC state, requiring a full replay.
    if (const absl::Time* time_last_changed = gtl::FindOrNull(
            node_id_to_port_id_to_time_last_changed_[node_id], port_id)) {
      node_id_to_port_id_to_time_last_changed[node_id][port_id] =
          *time_last_changed;
    } else {
      node_id_to_port_id_to_time_last_changed[node_id][port_id] =
          absl::UnixEpoch();
    }
    if (const PortState* port_state = gtl::FindOrNull(
            node_id_to_port_id_to_port_state_[node_id], port_id)) {
      node_id_to_port_id_to_port_state[node_id][port_id] = *port_state;
    } else {
      node_id_to_port_id_to_port_state[node_id][port_id] = PORT_STATE_UNKNOWN;
    }
    // Create a new empty port config.
    node_id_to_port_id_to_port_config[node_id][port_id] = PortConfig();
    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    node_id_to_port_id_to_singleton_port_key[node_id][port_id] =
        singleton_port_key;

    // Translate the logical SDN port to SDK port (BF device port ID)
    ASSIGN_OR_RETURN(uint32 sdk_port, bf_sde_interface_->GetPortIdFromPortKey(
                                          *device, singleton_port_key));
    node_id_to_port_id_to_sdk_port_id[node_id][port_id] = sdk_port;
    node_id_to_sdk_port_id_to_port_id[node_id][sdk_port] = port_id;

    PortKey port_group_key(singleton_port.slot(), singleton_port.port());
    xcvr_port_key_to_xcvr_state[port_group_key] = HW_STATE_UNKNOWN;
  }

  for (const auto& singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();
    // we checked that node_id was valid in the previous loop
    auto device = node_id_to_device[node_id];

    // TODO(antonin): we currently ignore slot
    // Stratum requires slot and port to be set. We use port and channel to
    // get Tofino device port (called SDK port ID).

    const PortConfig* old_port_config = nullptr;
    if (const auto* port_id_to_port_config_old =
            gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id)) {
      old_port_config = gtl::FindOrNull(*port_id_to_port_config_old, port_id);
    }

    auto& port_config = node_id_to_port_id_to_port_config[node_id][port_id];
    uint32 sdk_port_id = node_id_to_port_id_to_sdk_port_id[node_id][port_id];
    if (old_port_config == nullptr) {  // new port
      // if anything fails, port_config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      RETURN_IF_ERROR(AddPortHelper(node_id, device, sdk_port_id,
                                    singleton_port, &port_config));
    } else {  // port already exists, config may have changed
      if (old_port_config->admin_state == ADMIN_STATE_UNKNOWN) {
        // something is wrong with the port, we make sure the port is deleted
        // first (and ignore the error status if there is one), then add the
        // port again.
        if (bf_sde_interface_->IsValidPort(device, sdk_port_id)) {
          bf_sde_interface_->DeletePort(device, sdk_port_id).IgnoreError();
        }
        RETURN_IF_ERROR(AddPortHelper(node_id, device, sdk_port_id,
                                      singleton_port, &port_config));
        continue;
      }

      // diff configs and apply necessary changes

      // sanity-check: if admin_state is not ADMIN_STATE_UNKNOWN, then the port
      // was added and the speed_bps was set.
      if (!old_port_config->speed_bps) {
        return MAKE_ERROR(ERR_INTERNAL)
               << "Invalid internal state in BfChassisManager, speed_bps field "
                  "should contain a value";
      }

      // if anything fails, port_config.admin_state will be set to
      // ADMIN_STATE_UNKNOWN (invalid)
      RETURN_IF_ERROR(UpdatePortHelper(node_id, device, sdk_port_id,
                                       singleton_port, *old_port_config,
                                       &port_config));
    }
  }

  if (config.has_vendor_config() &&
      config.vendor_config().has_tofino_config()) {
    // Handle port shaping.
    const auto& node_id_to_port_shaping_config =
        config.vendor_config().tofino_config().node_id_to_port_shaping_config();
    for (const auto& key : node_id_to_port_shaping_config) {
      const uint64 node_id = key.first;
      const TofinoConfig::BfPortShapingConfig& port_id_to_shaping_config =
          key.second;
      RET_CHECK(node_id_to_port_id_to_sdk_port_id.count(node_id));
      RET_CHECK(node_id_to_device.count(node_id));
      int device = node_id_to_device[node_id];
      for (const auto& e :
           port_id_to_shaping_config.per_port_shaping_configs()) {
        const uint32 port_id = e.first;
        const TofinoConfig::BfPortShapingConfig::BfPerPortShapingConfig&
            shaping_config = e.second;
        RET_CHECK(node_id_to_port_id_to_sdk_port_id[node_id].count(port_id));
        const uint32 sdk_port_id =
            node_id_to_port_id_to_sdk_port_id[node_id][port_id];
        RETURN_IF_ERROR(ApplyPortShapingConfig(node_id, device, sdk_port_id,
                                               shaping_config));
        node_id_to_port_id_to_port_config[node_id][port_id].shaping_config =
            shaping_config;
      }
    }

    // Handle deflect-on-drop config.
    const auto& node_id_to_deflect_on_drop_configs =
        config.vendor_config()
            .tofino_config()
            .node_id_to_deflect_on_drop_configs();
    for (const auto& key : node_id_to_deflect_on_drop_configs) {
      const uint64 node_id = key.first;
      const auto& deflect_config = key.second;
      for (const auto& drop_target : deflect_config.drop_targets()) {
        RET_CHECK(node_id_to_port_id_to_sdk_port_id.count(node_id));
        RET_CHECK(node_id_to_device.count(node_id));
        const int device = node_id_to_device[node_id];
        uint32 sdk_port_id;
        switch (drop_target.port_type_case()) {
          case TofinoConfig::DeflectOnPacketDropConfig::DropTarget::kPort: {
            const uint32 port_id = drop_target.port();
            RET_CHECK(
                node_id_to_port_id_to_sdk_port_id[node_id].count(port_id));
            sdk_port_id = node_id_to_port_id_to_sdk_port_id[node_id][port_id];
            break;
          }
          case TofinoConfig::DeflectOnPacketDropConfig::DropTarget::kSdkPort: {
            sdk_port_id = drop_target.sdk_port();
            break;
          }
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Unsupported port type in DropTarget "
                   << drop_target.ShortDebugString();
        }
        RETURN_IF_ERROR(bf_sde_interface_->SetDeflectOnDropDestination(
            device, sdk_port_id, drop_target.queue()));
        LOG(INFO) << "Configured deflect-on-drop to SDK port " << sdk_port_id
                  << " in node " << node_id << ".";
      }
      RET_CHECK(gtl::InsertIfNotPresent(&node_id_to_deflect_on_drop_config,
                                        node_id, deflect_config));
    }

    // Handle QoS configuration.
    const auto node_id_to_qos_configs =
        config.vendor_config().tofino_config().node_id_to_qos_config();
    for (const auto& key : node_id_to_qos_configs) {
      const uint64 node_id = key.first;
      // As the SDK Wrapper does not know anything about singleton ports, we
      // need to convert all such port IDs to sdk ports here.
      auto qos_config = key.second;
      for (auto& ppg_config : *qos_config.mutable_ppg_configs()) {
        switch (ppg_config.port_type_case()) {
          case TofinoConfig::TofinoQosConfig::PpgConfig::kSdkPort:
            break;
          case TofinoConfig::TofinoQosConfig::PpgConfig::kPort: {
            RET_CHECK(node_id_to_port_id_to_sdk_port_id.count(node_id));
            RET_CHECK(node_id_to_port_id_to_sdk_port_id[node_id].count(
                ppg_config.port()))
                << "Invalid singleton port " << ppg_config.port()
                << " in PpgConfig " << ppg_config.ShortDebugString() << ".";
            ppg_config.set_sdk_port(
                node_id_to_port_id_to_sdk_port_id[node_id][ppg_config.port()]);
            break;
          }
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Unsupported port type in PpgConfig "
                   << ppg_config.ShortDebugString() << ".";
        }
      }
      for (auto& queue_config : *qos_config.mutable_queue_configs()) {
        switch (queue_config.port_type_case()) {
          case TofinoConfig::TofinoQosConfig::QueueConfig::kSdkPort:
            break;
          case TofinoConfig::TofinoQosConfig::QueueConfig::kPort: {
            RET_CHECK(node_id_to_port_id_to_sdk_port_id.count(node_id));
            RET_CHECK(node_id_to_port_id_to_sdk_port_id[node_id].count(
                queue_config.port()))
                << "Invalid singleton port " << queue_config.port()
                << " in QueueConfig " << queue_config.ShortDebugString() << ".";
            queue_config.set_sdk_port(
                node_id_to_port_id_to_sdk_port_id[node_id]
                                                 [queue_config.port()]);
            break;
          }
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Unsupported port type in QueueConfig "
                   << queue_config.ShortDebugString() << ".";
        }
      }
      const int device = node_id_to_device[node_id];
      RETURN_IF_ERROR(bf_sde_interface_->ConfigureQos(device, qos_config));
      RET_CHECK(
          gtl::InsertIfNotPresent(&node_id_to_qos_config, node_id, qos_config));
    }
  }

  // Remove ports which are no longer present in the ChassisConfig.
  // Currently this code path is never hit, as we do not allow changes to the
  // port layout (adds or deletes) at runtime.
  for (const auto& node_ports_old : node_id_to_port_id_to_port_config_) {
    auto node_id = node_ports_old.first;
    for (const auto& port_old : node_ports_old.second) {
      auto port_id = port_old.first;
      auto device = node_id_to_device_[node_id];
      uint32 sdk_port_id = node_id_to_port_id_to_sdk_port_id_[node_id][port_id];
      if (node_id_to_port_id_to_port_config.count(node_id) > 0 &&
          node_id_to_port_id_to_port_config[node_id].count(port_id) > 0) {
        // Disable port shaping if not specified anymore.
        if (!node_id_to_port_id_to_port_config[node_id][port_id]
                 .shaping_config) {
          RETURN_IF_ERROR(bf_sde_interface_->EnablePortShaping(
              device, sdk_port_id, TRI_STATE_FALSE));
        }
        continue;
      }
      // TODO(bocon): Collect these errors and keep trying to remove old ports
      RETURN_IF_ERROR(bf_sde_interface_->DeletePort(device, sdk_port_id));
      LOG(INFO) << "Deleted port " << port_id << " in node " << node_id
                << " (SDK port " << sdk_port_id << ").";
    }
  }

  device_to_node_id_ = device_to_node_id;
  node_id_to_device_ = node_id_to_device;
  node_id_to_port_id_to_port_state_ = node_id_to_port_id_to_port_state;
  node_id_to_port_id_to_time_last_changed_ =
      node_id_to_port_id_to_time_last_changed;
  node_id_to_port_id_to_port_config_ = node_id_to_port_id_to_port_config;
  node_id_to_port_id_to_singleton_port_key_ =
      node_id_to_port_id_to_singleton_port_key;
  node_id_to_port_id_to_sdk_port_id_ = node_id_to_port_id_to_sdk_port_id;
  node_id_to_sdk_port_id_to_port_id_ = node_id_to_sdk_port_id_to_port_id;
  node_id_to_deflect_on_drop_config_ = node_id_to_deflect_on_drop_config;
  node_id_to_qos_config_ = node_id_to_qos_config;
  xcvr_port_key_to_xcvr_state_ = xcvr_port_key_to_xcvr_state;
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status BfChassisManager::ApplyPortShapingConfig(
    uint64 node_id, int device, uint32 sdk_port_id,
    const TofinoConfig::BfPortShapingConfig::BfPerPortShapingConfig&
        shaping_config) {
  switch (shaping_config.shaping_case()) {
    case TofinoConfig::BfPortShapingConfig::BfPerPortShapingConfig::
        kPacketShaping:
      RETURN_IF_ERROR(bf_sde_interface_->SetPortShapingRate(
          device, sdk_port_id, true,
          shaping_config.packet_shaping().burst_packets(),
          shaping_config.packet_shaping().rate_pps()));
      break;
    case TofinoConfig::BfPortShapingConfig::BfPerPortShapingConfig::
        kByteShaping:
      RETURN_IF_ERROR(bf_sde_interface_->SetPortShapingRate(
          device, sdk_port_id, false,
          shaping_config.byte_shaping().burst_bytes(),
          shaping_config.byte_shaping().rate_bps()));
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid port shaping config "
             << shaping_config.ShortDebugString() << ".";
  }
  RETURN_IF_ERROR(bf_sde_interface_->EnablePortShaping(device, sdk_port_id,
                                                       TRI_STATE_TRUE));
  LOG(INFO) << "Configured port shaping on SDK port " << sdk_port_id
            << " in node " << node_id << ": "
            << shaping_config.ShortDebugString() << ".";

  return ::util::OkStatus();
}

::util::Status BfChassisManager::VerifyChassisConfig(
    const ChassisConfig& config) {
  RET_CHECK(config.trunk_ports_size() == 0)
      << "Trunk ports are not supported on Tofino.";
  RET_CHECK(config.port_groups_size() == 0)
      << "Port groups are not supported on Tofino.";
  RET_CHECK(config.nodes_size() > 0)
      << "The config must contain at least one node.";

  // Find the supported Tofino chip types based on the given platform.
  RET_CHECK(config.has_chassis() && config.chassis().platform())
      << "Config needs a Chassis message with correct platform.";
  switch (config.chassis().platform()) {
    case PLT_GENERIC_BAREFOOT_TOFINO:
    case PLT_GENERIC_BAREFOOT_TOFINO2:
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Unsupported platform: "
             << Platform_Name(config.chassis().platform());
  }

  // Validate Node messages. Make sure there is no two nodes with the same id.
  std::map<uint64, int> node_id_to_device;
  std::map<int, uint64> device_to_node_id;
  for (const auto& node : config.nodes()) {
    RET_CHECK(node.slot() > 0)
        << "No positive slot in " << node.ShortDebugString();
    RET_CHECK(node.id() > 0) << "No positive ID in " << node.ShortDebugString();
    RET_CHECK(gtl::InsertIfNotPresent(&node_id_to_device, node.id(), -1))
        << "The id for Node " << PrintNode(node) << " was already recorded "
        << "for another Node in the config.";
  }
  {
    int device = 0;
    for (const auto& node : config.nodes()) {
      device_to_node_id[device] = node.id();
      node_id_to_device[node.id()] = device;
      ++device;
    }
  }

  // Go over all the singleton ports in the config:
  // 1- Validate the basic singleton port properties.
  // 2- Make sure there is no two ports with the same (slot, port, channel).
  // 3- Make sure for each (slot, port) pair, the channels of all the ports
  //    are valid. This depends on the port speed.
  // 4- Make sure no singleton port has the reserved CPU port ID. CPU port is
  //    a special port and is not in the list of singleton ports. It is
  //    configured separately.
  // 5- Make sure IDs of the singleton ports are unique per node.
  std::map<uint64, std::set<uint32>> node_id_to_port_ids;
  std::set<PortKey> singleton_port_keys;
  for (const auto& singleton_port : config.singleton_ports()) {
    RET_CHECK(singleton_port.id() > 0)
        << "No positive ID in " << PrintSingletonPort(singleton_port) << ".";
    RET_CHECK(singleton_port.id() != kCpuPortId)
        << "SingletonPort " << PrintSingletonPort(singleton_port)
        << " has the reserved CPU port ID (" << kCpuPortId << ").";
    RET_CHECK(singleton_port.id() != kSdnCpuPortId)
        << "SingletonPort " << PrintSingletonPort(singleton_port)
        << " has the reserved CPU port ID (" << kSdnCpuPortId << ").";
    RET_CHECK(singleton_port.slot() > 0)
        << "No valid slot in " << singleton_port.ShortDebugString() << ".";
    RET_CHECK(singleton_port.port() > 0)
        << "No valid port in " << singleton_port.ShortDebugString() << ".";
    RET_CHECK(singleton_port.speed_bps() > 0)
        << "No valid speed_bps in " << singleton_port.ShortDebugString() << ".";
    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    RET_CHECK(!singleton_port_keys.count(singleton_port_key))
        << "The (slot, port, channel) tuple for SingletonPort "
        << PrintSingletonPort(singleton_port)
        << " was already recorded for another SingletonPort in the config.";
    singleton_port_keys.insert(singleton_port_key);
    RET_CHECK(singleton_port.node() > 0)
        << "No valid node ID in " << singleton_port.ShortDebugString() << ".";
    RET_CHECK(node_id_to_device.count(singleton_port.node()))
        << "Node ID " << singleton_port.node() << " given for SingletonPort "
        << PrintSingletonPort(singleton_port)
        << " has not been given to any Node in the config.";
    RET_CHECK(
        !node_id_to_port_ids[singleton_port.node()].count(singleton_port.id()))
        << "The id for SingletonPort " << PrintSingletonPort(singleton_port)
        << " was already recorded for another SingletonPort for node with ID "
        << singleton_port.node() << ".";
    node_id_to_port_ids[singleton_port.node()].insert(singleton_port.id());
  }

  std::map<uint64, std::map<uint32, PortKey>>
      node_id_to_port_id_to_singleton_port_key;
  std::map<uint64, std::map<uint32, uint32>> node_id_to_port_id_to_sdk_port_id;
  std::map<uint64, std::map<uint32, uint32>> node_id_to_sdk_port_id_to_port_id;

  for (const auto& singleton_port : config.singleton_ports()) {
    uint32 port_id = singleton_port.id();
    uint64 node_id = singleton_port.node();

    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    node_id_to_port_id_to_singleton_port_key[node_id][port_id] =
        singleton_port_key;

    // Make sure that the port exists by getting the SDK port ID.
    const int* device = gtl::FindOrNull(node_id_to_device, node_id);
    RET_CHECK(device != nullptr)
        << "Node " << node_id << " not found for port " << port_id << ".";
    ASSIGN_OR_RETURN(uint32 sdk_port, bf_sde_interface_->GetPortIdFromPortKey(
                                          *device, singleton_port_key));
    node_id_to_port_id_to_sdk_port_id[node_id][port_id] = sdk_port;
    node_id_to_sdk_port_id_to_port_id[node_id][sdk_port] = port_id;
  }

  // Verify the QoS configuration.
  if (config.has_vendor_config() &&
      config.vendor_config().has_tofino_config()) {
    const auto& node_id_to_qos_config =
        config.vendor_config().tofino_config().node_id_to_qos_config();
    for (const auto& e : node_id_to_qos_config) {
      const uint64 node_id = e.first;
      const TofinoConfig::TofinoQosConfig& qos_config = e.second;
      const int* device = gtl::FindOrNull(node_id_to_device, node_id);
      RET_CHECK(device != nullptr) << "Node " << node_id << " not found.";
      for (const auto& queue_config : qos_config.queue_configs()) {
        uint32 sdk_port_id;
        switch (queue_config.port_type_case()) {
          case TofinoConfig::TofinoQosConfig::QueueConfig::kSdkPort:
            sdk_port_id = queue_config.sdk_port();
            break;
          case TofinoConfig::TofinoQosConfig::QueueConfig::kPort: {
            RET_CHECK(node_id_to_port_id_to_sdk_port_id[node_id].count(
                queue_config.port()))
                << "Invalid singleton port " << queue_config.port()
                << " in queue config " << queue_config.ShortDebugString()
                << ".";
            sdk_port_id =
                node_id_to_port_id_to_sdk_port_id[node_id][queue_config.port()];
            break;
          }
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Unsupported port type in QueueConfig "
                   << queue_config.ShortDebugString() << ".";
        }
        RET_CHECK(gtl::FindOrNull(node_id_to_sdk_port_id_to_port_id[node_id],
                                  sdk_port_id) != nullptr)
            << "Invalid port " << sdk_port_id << " in queue config "
            << queue_config.ShortDebugString() << ".";
        RET_CHECK(queue_config.queue_mapping_size() <= kMaxQueuesPerPort);
        // Check that queue mappings are in ascending order starting from zero.
        for (int i = 0; i < queue_config.queue_mapping_size(); ++i) {
          RET_CHECK(i == queue_config.queue_mapping(i).queue_id())
              << "Found out-of-order queue mapping for queue id "
              << queue_config.queue_mapping(i).queue_id() << " in queue config "
              << queue_config.ShortDebugString() << ".";
        }
      }
    }
  }

  // If the class is initialized, we also need to check if the new config will
  // require a change in the port layout. If so, report reboot required.
  if (initialized_) {
    if (node_id_to_port_id_to_singleton_port_key !=
        node_id_to_port_id_to_singleton_port_key_) {
      return MAKE_ERROR(ERR_REBOOT_REQUIRED)
             << "The switch is already initialized, but we detected the newly "
                "pushed config requires a change in the port layout. The stack "
                "needs to be rebooted to finish config push.";
    }

    if (node_id_to_device != node_id_to_device_) {
      return MAKE_ERROR(ERR_REBOOT_REQUIRED)
             << "The switch is already initialized, but we detected the newly "
                "pushed config requires a change in node_id_to_device. The "
                "stack needs to be rebooted to finish config push.";
    }
  }

  return ::util::OkStatus();
}

::util::Status BfChassisManager::RegisterEventNotifyWriter(
    const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BfChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::StatusOr<const BfChassisManager::PortConfig*>
BfChassisManager::GetPortConfig(uint64 node_id, uint32 port_id) const {
  auto* port_id_to_config =
      gtl::FindOrNull(node_id_to_port_id_to_port_config_, node_id);
  RET_CHECK(port_id_to_config != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortConfig* config = gtl::FindOrNull(*port_id_to_config, port_id);
  RET_CHECK(config != nullptr)
      << "Port " << port_id << " is not configured or not known for node "
      << node_id << ".";
  return config;
}

::util::StatusOr<uint32> BfChassisManager::GetSdkPortId(uint64 node_id,
                                                        uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  const auto* port_map =
      gtl::FindOrNull(node_id_to_port_id_to_sdk_port_id_, node_id);
  RET_CHECK(port_map != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  const uint32* sdk_port_id = gtl::FindOrNull(*port_map, port_id);
  RET_CHECK(sdk_port_id != nullptr)
      << "Port " << port_id << " for node " << node_id
      << " is not configured or not known.";

  return *sdk_port_id;
}

::util::StatusOr<DataResponse> BfChassisManager::GetPortData(
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
      ASSIGN_OR_RETURN(absl::Time last_changed,
                       GetPortTimeLastChanged(request.oper_status().node_id(),
                                              request.oper_status().port_id()));
      resp.mutable_oper_status()->set_time_last_changed(
          absl::ToUnixNanos(last_changed));
      break;
    }
    case Request::kAdminStatus: {
      ASSIGN_OR_RETURN(auto* config,
                       GetPortConfig(request.admin_status().node_id(),
                                     request.admin_status().port_id()));
      resp.mutable_admin_status()->set_state(config->admin_state);
      break;
    }
    case Request::kMacAddress: {
      // TODO(unknown) Find out why the controller needs it.
      // Find MAC address of port located at:
      // - node_id: req.mac_address().node_id()
      // - port_id: req.mac_address().port_id()
      // and then write it into the response.
      resp.mutable_mac_address()->set_mac_address(kDummyMacAddress);
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
    case DataRequest::Request::kLacpRouterMac: {
      // Find LACP System ID MAC address of port located at:
      // - node_id: req.lacp_router_mac().node_id()
      // - port_id: req.lacp_router_mac().port_id()
      // and then write it into the response.
      resp.mutable_lacp_router_mac()->set_mac_address(kDummyMacAddress);
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
    case Request::kSdnPortId: {
      ASSIGN_OR_RETURN(auto sdk_port_id,
                       GetSdkPortId(request.sdn_port_id().node_id(),
                                    request.sdn_port_id().port_id()));
      resp.mutable_sdn_port_id()->set_port_id(sdk_port_id);
      break;
    }
    case Request::kForwardingViability: {
      // Find current port forwarding viable state for port located at:
      // - node_id: req.forwarding_viable().node_id()
      // - port_id: req.forwarding_viable().port_id()
      // and then write it into the response.
      resp.mutable_forwarding_viability()->set_state(
          TRUNK_MEMBER_BLOCK_STATE_UNKNOWN);
      break;
    }
    case DataRequest::Request::kHealthIndicator: {
      // Find current port health indicator (LED) for port located at:
      // - node_id: req.health_indicator().node_id()
      // - port_id: req.health_indicator().port_id()
      // and then write it into the response.
      resp.mutable_health_indicator()->set_state(HEALTH_STATE_UNKNOWN);
      break;
    }
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Not supported yet";
  }
  return resp;
}

::util::StatusOr<PortState> BfChassisManager::GetPortState(
    uint64 node_id, uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  const std::map<uint32, PortState>* port_id_to_port_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  RET_CHECK(port_id_to_port_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortState* port_state =
      gtl::FindOrNull(*port_id_to_port_state, port_id);
  RET_CHECK(port_state != nullptr)
      << "Port " << port_id << " is not known on node " << node_id << ".";

  if (*port_state == PORT_STATE_UNKNOWN) {
    // If state is unknown, query the current state from the SDE.
    ASSIGN_OR_RETURN(auto device, GetDeviceFromNodeId(node_id));
    ASSIGN_OR_RETURN(auto sdk_port_id, GetSdkPortId(node_id, port_id));
    ASSIGN_OR_RETURN(auto current_port_state,
                     bf_sde_interface_->GetPortState(device, sdk_port_id));
    return current_port_state;
  }

  return *port_state;
}

::util::StatusOr<absl::Time> BfChassisManager::GetPortTimeLastChanged(
    uint64 node_id, uint32 port_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  RET_CHECK(node_id_to_port_id_to_time_last_changed_.count(node_id));
  RET_CHECK(node_id_to_port_id_to_time_last_changed_[node_id].count(port_id));
  return node_id_to_port_id_to_time_last_changed_[node_id][port_id];
}

::util::Status BfChassisManager::GetPortCounters(uint64 node_id, uint32 port_id,
                                                 PortCounters* counters) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto device, GetDeviceFromNodeId(node_id));
  ASSIGN_OR_RETURN(auto sdk_port_id, GetSdkPortId(node_id, port_id));
  return bf_sde_interface_->GetPortCounters(device, sdk_port_id, counters);
}

::util::StatusOr<std::map<uint64, int>> BfChassisManager::GetNodeIdToDeviceMap()
    const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED).without_logging()
           << "Not initialized!";
  }

  return node_id_to_device_;
}

::util::Status BfChassisManager::ReplayChassisConfig(uint64 node_id) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto device, GetDeviceFromNodeId(node_id));

  for (auto& p : node_id_to_port_id_to_port_state_[node_id])
    p.second = PORT_STATE_UNKNOWN;

  for (auto& p : node_id_to_port_id_to_time_last_changed_[node_id]) {
    p.second = absl::UnixEpoch();
  }

  auto replay_one_port = [node_id, device, this](
                             uint32 port_id, const PortConfig& config,
                             PortConfig* config_new) -> ::util::Status {
    if (config.admin_state == ADMIN_STATE_UNKNOWN) {
      LOG(WARNING) << "Port " << port_id << " in node " << node_id
                   << " was not configured properly, so skipping replay.";
      return ::util::OkStatus();
    }

    if (!config.speed_bps) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Invalid internal state in BfChassisManager, speed_bps field "
                "should contain a value";
    }
    if (!config.fec_mode) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Invalid internal state in BfChassisManager, fec_mode field "
                "should contain a value";
    }

    ASSIGN_OR_RETURN(auto sdk_port_id, GetSdkPortId(node_id, port_id));
    RETURN_IF_ERROR(bf_sde_interface_->AddPort(
        device, sdk_port_id, *config.speed_bps, *config.fec_mode));
    config_new->speed_bps = *config.speed_bps;
    config_new->admin_state = ADMIN_STATE_DISABLED;
    config_new->fec_mode = *config.fec_mode;

    if (config.mtu) {
      RETURN_IF_ERROR(
          bf_sde_interface_->SetPortMtu(device, sdk_port_id, *config.mtu));
      config_new->mtu = *config.mtu;
      VLOG(1) << "Set MTU " << *config.mtu << " for port " << port_id
              << " in node " << node_id << " (SDK Port " << sdk_port_id << ").";
    }
    if (config.autoneg) {
      RETURN_IF_ERROR(bf_sde_interface_->SetPortAutonegPolicy(
          device, sdk_port_id, *config.autoneg));
      config_new->autoneg = *config.autoneg;
      VLOG(1) << "Set autoneg policy " << TriState_Name(*config.autoneg)
              << " for port " << port_id << " in node " << node_id
              << " (SDK Port " << sdk_port_id << ").";
    }
    if (config.loopback_mode) {
      RETURN_IF_ERROR(bf_sde_interface_->SetPortLoopbackMode(
          device, sdk_port_id, *config.loopback_mode));
      config_new->loopback_mode = *config.loopback_mode;
      VLOG(1) << "Set loopback mode "
              << LoopbackState_Name(*config.loopback_mode) << " for port "
              << port_id << " in node " << node_id << " (SDK Port "
              << sdk_port_id << ").";
    }

    if (config.admin_state == ADMIN_STATE_ENABLED) {
      RETURN_IF_ERROR(bf_sde_interface_->EnablePort(device, sdk_port_id));
      config_new->admin_state = ADMIN_STATE_ENABLED;
      VLOG(1) << "Enabled port " << port_id << " in node " << node_id
              << " (SDK Port " << sdk_port_id << ").";
    }

    if (config.shaping_config) {
      RETURN_IF_ERROR(ApplyPortShapingConfig(node_id, device, sdk_port_id,
                                             *config.shaping_config));
      config_new->shaping_config = config.shaping_config;
    }

    VLOG(1) << "Replayed port " << port_id << " in node " << node_id << ".";

    return ::util::OkStatus();
  };

  ::util::Status status = ::util::OkStatus();  // errors to keep track of.

  for (auto& p : node_id_to_port_id_to_port_config_[node_id]) {
    uint32 port_id = p.first;
    PortConfig config_new;
    APPEND_STATUS_IF_ERROR(status,
                           replay_one_port(port_id, p.second, &config_new));
    p.second = config_new;
  }

  // Replay QoS configuration.
  RETURN_IF_ERROR(
      bf_sde_interface_->ConfigureQos(device, node_id_to_qos_config_[node_id]));

  for (const auto& drop_target :
       node_id_to_deflect_on_drop_config_[node_id].drop_targets()) {
    uint32 sdk_port_id;
    switch (drop_target.port_type_case()) {
      case TofinoConfig::DeflectOnPacketDropConfig::DropTarget::kPort: {
        ASSIGN_OR_RETURN(sdk_port_id,
                         GetSdkPortId(node_id, drop_target.port()));
        break;
      }
      case TofinoConfig::DeflectOnPacketDropConfig::DropTarget::kSdkPort: {
        sdk_port_id = drop_target.sdk_port();
        break;
      }
      default:
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Unsupported port type in DropTarget "
               << drop_target.ShortDebugString();
    }

    RETURN_IF_ERROR(bf_sde_interface_->SetDeflectOnDropDestination(
        device, sdk_port_id, drop_target.queue()));
    LOG(INFO) << "Configured deflect on drop target port " << sdk_port_id
              << " in node " << node_id << ".";
  }

  // Re-configure the CPU port in the traffic manager.
  ASSIGN_OR_RETURN(auto cpu_port, bf_sde_interface_->GetPcieCpuPort(device));
  RETURN_IF_ERROR(bf_sde_interface_->SetTmCpuPort(device, cpu_port));

  LOG(INFO) << "Replayed chassis config for node " << node_id << ".";

  return status;
}

::util::Status BfChassisManager::GetFrontPanelPortInfo(
    uint64 node_id, uint32 port_id, FrontPanelPortInfo* fp_port_info) {
  auto* port_id_to_port_key =
      gtl::FindOrNull(node_id_to_port_id_to_singleton_port_key_, node_id);
  RET_CHECK(port_id_to_port_key != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  auto* port_key = gtl::FindOrNull(*port_id_to_port_key, port_id);
  RET_CHECK(port_key != nullptr) << "Node " << node_id << ", port " << port_id
                                 << " is not configured or not known.";
  return phal_interface_->GetFrontPanelPortInfo(port_key->slot, port_key->port,
                                                fp_port_info);
}

std::unique_ptr<BfChassisManager> BfChassisManager::CreateInstance(
    OperationMode mode, PhalInterface* phal_interface,
    BfSdeInterface* bf_sde_interface) {
  return absl::WrapUnique(
      new BfChassisManager(mode, phal_interface, bf_sde_interface));
}

void BfChassisManager::SendPortOperStateGnmiEvent(
    uint64 node_id, uint32 port_id, PortState new_state,
    absl::Time time_last_changed) {
  absl::ReaderMutexLock l(&gnmi_event_lock_);
  if (!gnmi_event_writer_) return;
  // Allocate and initialize a PortOperStateChangedEvent event and pass it to
  // the gNMI publisher using the gNMI event notification channel.
  // The GnmiEventPtr is a smart pointer (shared_ptr<>) and it takes care of
  // the memory allocated to this event object once the event is handled by
  // the GnmiPublisher.
  if (!gnmi_event_writer_->Write(GnmiEventPtr(new PortOperStateChangedEvent(
          node_id, port_id, new_state,
          absl::ToUnixNanos(time_last_changed))))) {
    // Remove WriterInterface if it is no longer operational.
    gnmi_event_writer_.reset();
  }
}

void* BfChassisManager::PortStatusEventHandlerThreadFunc(void* arg) {
  CHECK(arg != nullptr);
  // Retrieve arguments.
  auto* args = reinterpret_cast<ReaderArgs<PortStatusEvent>*>(arg);
  auto* manager = args->manager;
  std::unique_ptr<ChannelReader<PortStatusEvent>> reader =
      std::move(args->reader);
  delete args;
  manager->ReadPortStatusEvents(reader);
  return nullptr;
}

void BfChassisManager::ReadPortStatusEvents(
    const std::unique_ptr<ChannelReader<PortStatusEvent>>& reader) {
  PortStatusEvent event;
  do {
    // Check switch shutdown.
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    // Block on the next linkscan event message from the Channel.
    int code = reader->Read(&event, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle received message.
    PortStatusEventHandler(event.device, event.port, event.state,
                           event.time_last_changed);
  } while (true);
}

void BfChassisManager::PortStatusEventHandler(int device, int port,
                                              PortState new_state,
                                              absl::Time time_last_changed) {
  absl::WriterMutexLock l(&chassis_lock);
  if (shutdown) {
    VLOG(1) << "The class is already shutdown. Exiting.";
    return;
  }

  // Update the state.
  const uint64* node_id = gtl::FindOrNull(device_to_node_id_, device);
  if (node_id == nullptr) {
    LOG(ERROR) << "Inconsistent state. Device " << device << " is not known!";
    return;
  }
  const uint32* port_id =
      gtl::FindOrNull(node_id_to_sdk_port_id_to_port_id_[*node_id], port);
  if (port_id == nullptr) {
    // We get a notification for all ports, even ports that were not added,
    // when doing a Fast Refresh, which can be confusing, so we use VLOG
    // instead.
    VLOG(1)
        << "Ignored an unknown SdkPort " << port << " on node " << *node_id
        << ". Most probably this is a non-configured channel of a flex port.";
    return;
  }
  node_id_to_port_id_to_port_state_[*node_id][*port_id] = new_state;
  node_id_to_port_id_to_time_last_changed_[*node_id][*port_id] =
      time_last_changed;

  // Notify the managers about the change of port state.
  // Nothing to do for now.

  // Notify gNMI about the change of logical port state.
  SendPortOperStateGnmiEvent(*node_id, *port_id, new_state, time_last_changed);

  LOG(INFO) << "State of port " << *port_id << " in node " << *node_id
            << " (SDK port " << port << "): " << PrintPortState(new_state)
            << ".";
}

void* BfChassisManager::TransceiverEventHandlerThreadFunc(void* arg) {
  CHECK(arg != nullptr);
  // Retrieve arguments.
  auto* args = reinterpret_cast<ReaderArgs<TransceiverEvent>*>(arg);
  auto* manager = args->manager;
  std::unique_ptr<ChannelReader<TransceiverEvent>> reader =
      std::move(args->reader);
  delete args;
  manager->ReadTransceiverEvents(reader);
  return nullptr;
}

void BfChassisManager::ReadTransceiverEvents(
    const std::unique_ptr<ChannelReader<TransceiverEvent>>& reader) {
  do {
    // Check switch shutdown.
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    TransceiverEvent event;
    // Block on the next transceiver event message from the Channel.
    int code = reader->Read(&event, absl::InfiniteDuration()).error_code();
    // Exit if the Channel is closed.
    if (code == ERR_CANCELLED) break;
    // Read should never timeout.
    if (code == ERR_ENTRY_NOT_FOUND) {
      LOG(ERROR) << "Read with infinite timeout failed with ENTRY_NOT_FOUND.";
      continue;
    }
    // Handle received message.
    TransceiverEventHandler(event.slot, event.port, event.state);
  } while (true);
}

void BfChassisManager::TransceiverEventHandler(int slot, int port,
                                               HwState new_state) {
  absl::WriterMutexLock l(&chassis_lock);
  if (shutdown) {
    VLOG(1) << "The class is already shutdown. Exiting.";
    return;
  }

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

::util::Status BfChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }
  // If we have not done that yet, create port status event Channel, register
  // Writer, and create Reader thread.
  if (!port_status_event_channel_) {
    port_status_event_channel_ =
        Channel<PortStatusEvent>::Create(kMaxPortStatusEventDepth);
    // Create and hand-off Writer to the BfSdeInterface.
    auto writer =
        ChannelWriter<PortStatusEvent>::Create(port_status_event_channel_);
    RETURN_IF_ERROR(
        bf_sde_interface_->RegisterPortStatusEventWriter(std::move(writer)));
    LOG(INFO) << "Successfully registered port status notification callback.";
    // Create and hand-off Reader to new reader thread.
    auto reader =
        ChannelReader<PortStatusEvent>::Create(port_status_event_channel_);
    pthread_t port_status_event_reader_tid;
    int ret = pthread_create(
        &port_status_event_reader_tid, nullptr,
        PortStatusEventHandlerThreadFunc,
        new ReaderArgs<PortStatusEvent>{this, std::move(reader)});
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create port status thread. Err: " << ret << ".";
    }
    // We don't care about the return value. The thread should exit following
    // the closing of the Channel in UnregisterEventWriters().
    ret = pthread_detach(port_status_event_reader_tid);
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to detach port status thread. Err: " << ret << ".";
    }
  }

  // If we have not done that yet, create transceiver module insert/removal
  // event Channel, register ChannelWriter, and create ChannelReader thread.
  if (xcvr_event_writer_id_ == kInvalidWriterId) {
    xcvr_event_channel_ = Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);
    // Create and hand-off ChannelWriter to the PhalInterface.
    auto writer = ChannelWriter<TransceiverEvent>::Create(xcvr_event_channel_);
    int priority = PhalInterface::kTransceiverEventWriterPriorityHigh;
    ASSIGN_OR_RETURN(xcvr_event_writer_id_,
                     phal_interface_->RegisterTransceiverEventWriter(
                         std::move(writer), priority));
    // Create and hand-off ChannelReader to new reader thread.
    auto reader = ChannelReader<TransceiverEvent>::Create(xcvr_event_channel_);
    pthread_t xcvr_event_reader_tid;
    int ret = pthread_create(
        &xcvr_event_reader_tid, nullptr, TransceiverEventHandlerThreadFunc,
        new ReaderArgs<TransceiverEvent>{this, std::move(reader)});
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create transceiver event thread. Err: " << ret
             << ".";
    }

    // We don't care about the return value of the thread. It should exit once
    // the Channel is closed in UnregisterEventWriters().
    ret = pthread_detach(xcvr_event_reader_tid);
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to detach transceiver event thread. Err: " << ret
             << ".";
    }
  }

  return ::util::OkStatus();
}

::util::Status BfChassisManager::UnregisterEventWriters() {
  absl::WriterMutexLock l(&chassis_lock);
  ::util::Status status = ::util::OkStatus();
  // Unregister the linkscan and transceiver module event Writers.
  APPEND_STATUS_IF_ERROR(status,
                         bf_sde_interface_->UnregisterPortStatusEventWriter());
  // Close Channel.
  if (!port_status_event_channel_ || !port_status_event_channel_->Close()) {
    ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                           << "Error when closing port status change"
                           << " event channel.";
    APPEND_STATUS_IF_ERROR(status, error);
  }
  port_status_event_channel_.reset();
  if (xcvr_event_writer_id_ != kInvalidWriterId) {
    APPEND_STATUS_IF_ERROR(status,
                           phal_interface_->UnregisterTransceiverEventWriter(
                               xcvr_event_writer_id_));
    xcvr_event_writer_id_ = kInvalidWriterId;
    // Close Channel.
    if (!xcvr_event_channel_ || !xcvr_event_channel_->Close()) {
      ::util::Status error = MAKE_ERROR(ERR_INTERNAL)
                             << "Error when closing transceiver event channel.";
      APPEND_STATUS_IF_ERROR(status, error);
    }
    xcvr_event_channel_.reset();
  }

  return status;
}

::util::StatusOr<int> BfChassisManager::GetDeviceFromNodeId(
    uint64 node_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const int* device = gtl::FindOrNull(node_id_to_device_, node_id);
  RET_CHECK(device != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  return *device;
}

void BfChassisManager::CleanupInternalState() {
  device_to_node_id_.clear();
  node_id_to_device_.clear();
  node_id_to_port_id_to_port_state_.clear();
  node_id_to_port_id_to_time_last_changed_.clear();
  node_id_to_port_id_to_port_config_.clear();
  node_id_to_port_id_to_singleton_port_key_.clear();
  node_id_to_port_id_to_sdk_port_id_.clear();
  node_id_to_sdk_port_id_to_port_id_.clear();
  node_id_to_deflect_on_drop_config_.clear();
  node_id_to_qos_config_.clear();
  xcvr_port_key_to_xcvr_state_.clear();
}

::util::Status BfChassisManager::Shutdown() {
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
