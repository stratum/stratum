// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"

#include <pthread.h>

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

DEFINE_string(base_bcm_chassis_map_file, "",
              "The file to read the base_bcm_chassis_map proto.");
DEFINE_string(applied_bcm_chassis_map_file,
              "/var/run/stratum/applied_bcm_chassis_map.pb.txt",
              "The file to write the applied_bcm_chassis_map proto created as "
              "part of initial config push, for debugging purposes.");
DEFINE_string(bcm_sdk_config_file, "/var/run/stratum/config.bcm",
              "The BCM config file loaded by SDK while initializing.");
DEFINE_string(bcm_sdk_config_flush_file, "/var/run/stratum/config.bcm.tmp",
              "The BCM config flush file loaded by SDK while initializing.");
DEFINE_string(bcm_sdk_shell_log_file, "/var/log/stratum/bcm.log",
              "The BCM shell log file loaded by SDK while initializing.");
DEFINE_string(bcm_sdk_checkpoint_dir, "",
              "The dir used by SDK to save checkpoints. Default is empty and "
              "it is expected to be explicitly given by flags.");

namespace stratum {
namespace hal {
namespace bcm {

using LinkscanEvent = BcmSdkInterface::LinkscanEvent;
using TransceiverEvent = PhalInterface::TransceiverEvent;

constexpr int BcmChassisManager::kTridentPlusMaxBcmPortsPerChip;
constexpr int BcmChassisManager::kTridentPlusMaxBcmPortsInXPipeline;
constexpr int BcmChassisManager::kTrident2MaxBcmPortsPerChip;
constexpr int BcmChassisManager::kTomahawkMaxBcmPortsPerChip;
constexpr int BcmChassisManager::kTomahawkPlusMaxBcmPortsPerChip;
constexpr int BcmChassisManager::kMaxLinkscanEventDepth;
constexpr int BcmChassisManager::kMaxXcvrEventDepth;

BcmChassisManager::BcmChassisManager(OperationMode mode,
                                     PhalInterface* phal_interface,
                                     BcmSdkInterface* bcm_sdk_interface,
                                     BcmSerdesDbManager* bcm_serdes_db_manager)
    : mode_(mode),
      initialized_(false),
      linkscan_event_writer_id_(kInvalidWriterId),
      xcvr_event_writer_id_(kInvalidWriterId),
      base_bcm_chassis_map_(nullptr),
      applied_bcm_chassis_map_(nullptr),
      unit_to_bcm_chip_(),
      singleton_port_key_to_bcm_port_(),
      port_group_key_to_flex_bcm_ports_(),
      port_group_key_to_non_flex_bcm_ports_(),
      node_id_to_unit_(),
      unit_to_node_id_(),
      node_id_to_port_ids_(),
      node_id_to_trunk_ids_(),
      node_id_to_port_id_to_singleton_port_key_(),
      node_id_to_port_id_to_sdk_port_(),
      node_id_to_trunk_id_to_sdk_trunk_(),
      node_id_to_sdk_port_to_port_id_(),
      node_id_to_sdk_trunk_to_trunk_id_(),
      xcvr_port_key_to_xcvr_state_(),
      node_id_to_port_id_to_port_state_(),
      node_id_to_trunk_id_to_trunk_state_(),
      node_id_to_trunk_id_to_members_(),
      node_id_to_port_id_to_trunk_membership_info_(),
      node_id_to_port_id_to_admin_state_(),
      node_id_to_port_id_to_health_state_(),
      node_id_to_port_id_to_loopback_state_(),
      xcvr_event_channel_(nullptr),
      linkscan_event_channel_(nullptr),
      phal_interface_(ABSL_DIE_IF_NULL(phal_interface)),
      bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      bcm_serdes_db_manager_(ABSL_DIE_IF_NULL(bcm_serdes_db_manager)) {}

// Default constructor is called by the mock class only.
BcmChassisManager::BcmChassisManager()
    : mode_(OPERATION_MODE_STANDALONE),
      initialized_(false),
      linkscan_event_writer_id_(kInvalidWriterId),
      xcvr_event_writer_id_(kInvalidWriterId),
      base_bcm_chassis_map_(nullptr),
      applied_bcm_chassis_map_(nullptr),
      unit_to_bcm_chip_(),
      singleton_port_key_to_bcm_port_(),
      port_group_key_to_flex_bcm_ports_(),
      port_group_key_to_non_flex_bcm_ports_(),
      node_id_to_unit_(),
      unit_to_node_id_(),
      node_id_to_port_ids_(),
      node_id_to_trunk_ids_(),
      node_id_to_port_id_to_singleton_port_key_(),
      node_id_to_port_id_to_sdk_port_(),
      node_id_to_trunk_id_to_sdk_trunk_(),
      node_id_to_sdk_port_to_port_id_(),
      node_id_to_sdk_trunk_to_trunk_id_(),
      xcvr_port_key_to_xcvr_state_(),
      node_id_to_port_id_to_port_state_(),
      node_id_to_trunk_id_to_trunk_state_(),
      node_id_to_trunk_id_to_members_(),
      node_id_to_port_id_to_trunk_membership_info_(),
      node_id_to_port_id_to_admin_state_(),
      node_id_to_port_id_to_health_state_(),
      node_id_to_port_id_to_loopback_state_(),
      xcvr_event_channel_(nullptr),
      linkscan_event_channel_(nullptr),
      phal_interface_(nullptr),
      bcm_sdk_interface_(nullptr),
      bcm_serdes_db_manager_(nullptr) {}

BcmChassisManager::~BcmChassisManager() {
  // NOTE: We should not detach any unit or unregister any handler in the
  // deconstructor as phal_interface_ or bcm_sdk_interface_ or can be deleted
  // before this class. Make sure you call Shutdown() before deleting the class
  // instance.
  if (initialized_) {
    LOG(ERROR) << "Deleting BcmChassisManager while initialized_ is still "
               << "true. You did not call Shutdown() before deleting the class "
               << "instance. This can lead to unexpected behavior.";
  }
  CleanupInternalState();
}

::util::Status BcmChassisManager::PushChassisConfig(
    const ChassisConfig& config) {
  if (!initialized_) {
    // If the class is not initialized. Perform an end-to-end coldboot
    // initialization sequence.
    if (mode_ == OPERATION_MODE_STANDALONE) {
      RETURN_IF_ERROR(bcm_serdes_db_manager_->Load());
    }
    BcmChassisMap base_bcm_chassis_map, target_bcm_chassis_map;
    RETURN_IF_ERROR(GenerateBcmChassisMapFromConfig(
        config, &base_bcm_chassis_map, &target_bcm_chassis_map));
    RETURN_IF_ERROR(
        InitializeBcmChips(base_bcm_chassis_map, target_bcm_chassis_map));
    RETURN_IF_ERROR(
        InitializeInternalState(base_bcm_chassis_map, target_bcm_chassis_map));
    RETURN_IF_ERROR(SyncInternalState(config));
    RETURN_IF_ERROR(ConfigurePortGroups());
    RETURN_IF_ERROR(RegisterEventWriters());
    initialized_ = true;
  } else {
    // If already initialized, sync the internal state and (re-)configure the
    // the flex and non-flex port groups.
    RETURN_IF_ERROR(SyncInternalState(config));
    RETURN_IF_ERROR(ConfigurePortGroups());
  }

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::VerifyChassisConfig(
    const ChassisConfig& config) {
  // Try creating the bcm_chassis_map based on the given config. This will
  // verify almost everything in the config as far as this class is concerned.
  BcmChassisMap base_bcm_chassis_map, target_bcm_chassis_map;
  RETURN_IF_ERROR(GenerateBcmChassisMapFromConfig(config, &base_bcm_chassis_map,
                                                  &target_bcm_chassis_map));

  // If the class is initialized, we also need to check if the new config will
  // require a change in bcm_chassis_map or node_id_to_unit. If so,
  // report reboot required.
  if (initialized_) {
    if (!ProtoEqual(target_bcm_chassis_map, *applied_bcm_chassis_map_)) {
      return MAKE_ERROR(ERR_REBOOT_REQUIRED)
             << "The switch is already initialized, but we detected the newly "
             << "pushed config requires a change in applied_bcm_chassis_map_. "
             << "The stack needs to be rebooted to finish config push.";
    }
    // Find node_id_to_unit that will be generated based on this config.
    std::map<uint64, int> node_id_to_unit;
    for (const auto& singleton_port : config.singleton_ports()) {
      for (const auto& bcm_port : base_bcm_chassis_map.bcm_ports()) {
        if (IsSingletonPortMatchesBcmPort(singleton_port, bcm_port)) {
          node_id_to_unit[singleton_port.node()] = bcm_port.unit();
        }
      }
    }
    if (node_id_to_unit != node_id_to_unit_) {
      return MAKE_ERROR(ERR_REBOOT_REQUIRED)
             << "The switch is already initialized, but we detected the newly "
             << "pushed config requires a change in node_id_to_unit. "
             << "The stack needs to be rebooted to finish config push.";
    }
  }

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::Shutdown() {
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, UnregisterEventWriters());
  APPEND_STATUS_IF_ERROR(status, bcm_sdk_interface_->ShutdownAllUnits());
  initialized_ = false;  // Set to false even if there is an error
  CleanupInternalState();

  return status;
}

void BcmChassisManager::SetUnitToBcmNodeMap(
    const std::map<int, BcmNode*>& unit_to_bcm_node) {
  absl::WriterMutexLock l(&chassis_lock);
  unit_to_bcm_node_ = unit_to_bcm_node;
}

::util::StatusOr<BcmChip> BcmChassisManager::GetBcmChip(int unit) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const BcmChip* bcm_chip = gtl::FindPtrOrNull(unit_to_bcm_chip_, unit);
  CHECK_RETURN_IF_FALSE(bcm_chip != nullptr) << "Unknown unit " << unit << ".";

  return BcmChip(*bcm_chip);
}

::util::StatusOr<BcmPort> BcmChassisManager::GetBcmPort(int slot, int port,
                                                        int channel) const {
  const PortKey singleton_port_key(slot, port, channel);
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const BcmPort* bcm_port =
      gtl::FindPtrOrNull(singleton_port_key_to_bcm_port_, singleton_port_key);
  CHECK_RETURN_IF_FALSE(bcm_port != nullptr)
      << "Unknown singleton port key: " << singleton_port_key.ToString() << ".";

  return BcmPort(*bcm_port);
}

::util::StatusOr<BcmPort> BcmChassisManager::GetBcmPort(uint64 node_id,
                                                        uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const auto* port_id_to_port_key =
      gtl::FindOrNull(node_id_to_port_id_to_singleton_port_key_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_key != nullptr)
      << "Unknown node " << node_id << ".";
  const auto* port_key = gtl::FindOrNull(*port_id_to_port_key, port_id);
  CHECK_RETURN_IF_FALSE(port_key != nullptr)
      << "Unknown port " << port_id << " on node " << node_id << ".";
  const auto* bcm_port =
      gtl::FindPtrOrNull(singleton_port_key_to_bcm_port_, *port_key);
  CHECK_RETURN_IF_FALSE(bcm_port != nullptr)
      << "Unknown singleton port key: " << port_key->ToString() << ".";
  return *bcm_port;
}

::util::StatusOr<std::map<uint64, int>> BcmChassisManager::GetNodeIdToUnitMap()
    const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED).without_logging()
        << "Not initialized!";
  }

  return node_id_to_unit_;
}

::util::StatusOr<int> BcmChassisManager::GetUnitFromNodeId(
    uint64 node_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const int* unit = gtl::FindOrNull(node_id_to_unit_, node_id);
  CHECK_RETURN_IF_FALSE(unit != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  return *unit;
}

::util::StatusOr<std::map<uint32, SdkPort>>
BcmChassisManager::GetPortIdToSdkPortMap(uint64 node_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, SdkPort>* port_id_to_sdk_port =
      gtl::FindOrNull(node_id_to_port_id_to_sdk_port_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_sdk_port != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  return *port_id_to_sdk_port;
}

::util::StatusOr<std::map<uint32, SdkTrunk>>
BcmChassisManager::GetTrunkIdToSdkTrunkMap(uint64 node_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, SdkTrunk>* trunk_id_to_sdk_trunk =
      gtl::FindOrNull(node_id_to_trunk_id_to_sdk_trunk_, node_id);
  CHECK_RETURN_IF_FALSE(trunk_id_to_sdk_trunk != nullptr)
      << "Node " << node_id << " is not configured or not known.";

  return *trunk_id_to_sdk_trunk;
}

::util::StatusOr<PortState> BcmChassisManager::GetPortState(
    uint64 node_id, uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, PortState>* port_id_to_port_state =
      gtl::FindOrNull(node_id_to_port_id_to_port_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_port_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const PortState* port_state =
      gtl::FindOrNull(*port_id_to_port_state, port_id);
  CHECK_RETURN_IF_FALSE(port_state != nullptr)
      << "Port " << port_id << " is not known on node " << node_id << ".";

  return *port_state;
}

::util::StatusOr<PortState> BcmChassisManager::GetPortState(
    const SdkPort& sdk_port) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }

  auto* node_id = gtl::FindOrNull(unit_to_node_id_, sdk_port.unit);
  CHECK_RETURN_IF_FALSE(node_id != nullptr)
      << "Attempting to query state of port on unknown unit " << sdk_port.unit
      << ".";
  auto* sdk_port_to_port_id =
      gtl::FindOrNull(node_id_to_sdk_port_to_port_id_, *node_id);
  CHECK_RETURN_IF_FALSE(sdk_port_to_port_id != nullptr)
      << "Inconsistent state! No sdk_port_to_port_id map for unit "
      << sdk_port.unit << ", node " << *node_id << ".";
  auto* port_id = gtl::FindOrNull(*sdk_port_to_port_id, sdk_port);
  CHECK_RETURN_IF_FALSE(port_id != nullptr)
      << "Attempting to retrieve state of unknown SDK port "
      << sdk_port.ToString() << ".";
  return GetPortState(*node_id, *port_id);
}

::util::StatusOr<TrunkState> BcmChassisManager::GetTrunkState(
    uint64 node_id, uint32 trunk_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, TrunkState>* trunk_id_to_trunk_state =
      gtl::FindOrNull(node_id_to_trunk_id_to_trunk_state_, node_id);
  CHECK_RETURN_IF_FALSE(trunk_id_to_trunk_state != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const TrunkState* trunk_state =
      gtl::FindOrNull(*trunk_id_to_trunk_state, trunk_id);
  CHECK_RETURN_IF_FALSE(trunk_state != nullptr)
      << "Trunk " << trunk_id << " is not known on node " << node_id << ".";

  return *trunk_state;
}

::util::StatusOr<std::set<uint32>> BcmChassisManager::GetTrunkMembers(
    uint64 node_id, uint32 trunk_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, std::set<uint32>>* trunk_id_to_members =
      gtl::FindOrNull(node_id_to_trunk_id_to_members_, node_id);
  CHECK_RETURN_IF_FALSE(trunk_id_to_members != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const std::set<uint32>* members =
      gtl::FindOrNull(*trunk_id_to_members, trunk_id);
  CHECK_RETURN_IF_FALSE(members != nullptr)
      << "Trunk " << trunk_id << " is not known on node " << node_id << ".";

  return *members;
}

::util::StatusOr<uint32> BcmChassisManager::GetParentTrunkId(
    uint64 node_id, uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const std::map<uint32, TrunkMembershipInfo>*
      port_id_to_trunk_membership_info = gtl::FindOrNull(
          node_id_to_port_id_to_trunk_membership_info_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_trunk_membership_info != nullptr)
      << "Node " << node_id << " is not configured or not known.";
  const TrunkMembershipInfo* membership_info =
      gtl::FindOrNull(*port_id_to_trunk_membership_info, port_id);
  // We can't use CHECK_RETURN_IF_FALSE here, because we want without_logging()
  if (membership_info == nullptr) {
    return MAKE_ERROR(ERR_INVALID_PARAM).without_logging()
      << "Port " << port_id
      << " is not known or does not belong to any trunk on node " << node_id
      << ".";
  }

  return membership_info->parent_trunk_id;
}

::util::StatusOr<AdminState> BcmChassisManager::GetPortAdminState(
    uint64 node_id, uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const auto* port_id_to_admin_state =
      gtl::FindOrNull(node_id_to_port_id_to_admin_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_admin_state != nullptr)
      << "Unknown node " << node_id << ".";
  const auto* admin_state = gtl::FindOrNull(*port_id_to_admin_state, port_id);
  CHECK_RETURN_IF_FALSE(admin_state != nullptr)
      << "Unknown port " << port_id << " on node " << node_id << ".";
  return *admin_state;
}

::util::StatusOr<LoopbackState> BcmChassisManager::GetPortLoopbackState(
    uint64 node_id, uint32 port_id) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  const auto* port_id_to_loopback_state =
      gtl::FindOrNull(node_id_to_port_id_to_loopback_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_loopback_state != nullptr)
      << "Unknown node " << node_id << ".";
  const auto* loopback_state =
      gtl::FindOrNull(*port_id_to_loopback_state, port_id);
  CHECK_RETURN_IF_FALSE(loopback_state != nullptr)
      << "Unknown port " << port_id << " on node " << node_id << ".";
  return *loopback_state;
}

::util::Status BcmChassisManager::GetPortCounters(uint64 node_id,
                                                  uint32 port_id,
                                                  PortCounters* pc) const {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  ASSIGN_OR_RETURN(auto unit, GetUnitFromNodeId(node_id));
  ASSIGN_OR_RETURN(auto bcm_port, GetBcmPort(node_id, port_id));
  return bcm_sdk_interface_->GetPortCounters(unit, bcm_port.logical_port(), pc);
}

::util::Status BcmChassisManager::SetTrunkMemberBlockState(
    uint64 node_id, uint32 trunk_id, uint32 port_id,
    TrunkMemberBlockState state) {
  // TODO(unknown): Implement this method.
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "SetTrunkMemberBlockState is not implemented.";
}

::util::Status BcmChassisManager::SetPortAdminState(uint64 node_id,
                                                    uint32 port_id,
                                                    AdminState state) {
  // TODO(unknown): Implement this method.
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "SetPortAdminState is not implemented.";
}
::util::Status BcmChassisManager::SetPortHealthState(uint64 node_id,
                                                     uint32 port_id,
                                                     HealthState state) {
  // TODO(unknown): Implement this method.
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "SetPortHealthState is not implemented.";
}

::util::Status BcmChassisManager::SetPortLoopbackState(uint64 node_id,
                                                       uint32 port_id,
                                                       LoopbackState state) {
  if (!initialized_) {
    return MAKE_ERROR(ERR_NOT_INITIALIZED) << "Not initialized!";
  }
  if (state == LoopbackState::LOOPBACK_STATE_UNKNOWN) {
    return ::util::OkStatus();
  }

  ASSIGN_OR_RETURN(auto unit, GetUnitFromNodeId(node_id));
  ASSIGN_OR_RETURN(auto bcm_port, GetBcmPort(node_id, port_id));
  BcmPortOptions options;
  options.set_loopback_mode(state);
  RETURN_IF_ERROR(bcm_sdk_interface_->SetPortOptions(
      bcm_port.unit(), bcm_port.logical_port(), options));

  // Update internal map.
  auto* port_id_to_loopback_state =
      gtl::FindOrNull(node_id_to_port_id_to_loopback_state_, node_id);
  CHECK_RETURN_IF_FALSE(port_id_to_loopback_state)
      << "Unknown node " << node_id << ".";
  auto* loopback_state = gtl::FindOrNull(*port_id_to_loopback_state, port_id);
  CHECK_RETURN_IF_FALSE(loopback_state)
      << "Unknown port " << port_id << " on node " << node_id << ".";
  *loopback_state = state;

  return ::util::OkStatus();
}

std::unique_ptr<BcmChassisManager> BcmChassisManager::CreateInstance(
    OperationMode mode, PhalInterface* phal_interface,
    BcmSdkInterface* bcm_sdk_interface,
    BcmSerdesDbManager* bcm_serdes_db_manager) {
  return absl::WrapUnique(new BcmChassisManager(
      mode, phal_interface, bcm_sdk_interface, bcm_serdes_db_manager));
}

namespace {

// A helper method that checks whether a given BcmPort belong to a BcmChip of
// type TRIDENT_PLUS and is a GE port.
bool IsGePortOnTridentPlus(const BcmPort& bcm_port,
                           const BcmChassisMap& bcm_chassis_map) {
  if (bcm_port.type() != BcmPort::GE) return false;
  for (const auto& bcm_chip : bcm_chassis_map.bcm_chips()) {
    if (bcm_chip.unit() == bcm_port.unit()) {
      return bcm_chip.type() == BcmChip::TRIDENT_PLUS;
    }
  }

  return false;
}

}  // namespace

// TODO(unknown): Include MGMT ports in the config if needed.
::util::Status BcmChassisManager::GenerateBcmChassisMapFromConfig(
    const ChassisConfig& config, BcmChassisMap* base_bcm_chassis_map,
    BcmChassisMap* target_bcm_chassis_map) const {
  if (base_bcm_chassis_map == nullptr || target_bcm_chassis_map == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Null base_bcm_chassis_map or target_bcm_chassis_map.";
  }

  // Clear the map explicitly and re-generate everything from scratch.
  base_bcm_chassis_map->Clear();
  target_bcm_chassis_map->Clear();

  // Load base_bcm_chassis_map before anything else if not done before.
  std::string bcm_chassis_map_id = "";
  if (config.has_vendor_config() &&
      config.vendor_config().has_google_config()) {
    bcm_chassis_map_id =
        config.vendor_config().google_config().bcm_chassis_map_id();
  }
  RETURN_IF_ERROR(
      ReadBaseBcmChassisMapFromFile(bcm_chassis_map_id, base_bcm_chassis_map));

  // Before doing anything, we populate the slot based on the pushed chassis
  // config if we need to do so.
  if (base_bcm_chassis_map->auto_add_slot()) {
    RETURN_IF_ERROR(
        PopulateSlotFromPushedChassisConfig(config, base_bcm_chassis_map));
  }

  // Find the supported BCM chip types based on the given platform.
  CHECK_RETURN_IF_FALSE(config.has_chassis() && config.chassis().platform())
      << "Config needs a Chassis message with correct platform.";
  std::set<BcmChip::BcmChipType> supported_chip_types;
  switch (config.chassis().platform()) {
    case PLT_GENERIC_TRIDENT_PLUS:
      supported_chip_types.insert(BcmChip::TRIDENT_PLUS);
      break;
    case PLT_GENERIC_TRIDENT2:
      supported_chip_types.insert(BcmChip::TRIDENT2);
      break;
    case PLT_GENERIC_TOMAHAWK:
      supported_chip_types.insert(BcmChip::TOMAHAWK);
      break;
    case PLT_GENERIC_TOMAHAWK_PLUS:
        supported_chip_types.insert(BcmChip::TOMAHAWK_PLUS);
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Unsupported platform: "
             << Platform_Name(config.chassis().platform());
  }

  // IDs should match (if there).
  if (!base_bcm_chassis_map->id().empty()) {
    target_bcm_chassis_map->set_id(base_bcm_chassis_map->id());
  }

  // auto_add_logical_ports should match (if there).
  target_bcm_chassis_map->set_auto_add_logical_ports(
      base_bcm_chassis_map->auto_add_logical_ports());

  // auto_add_slot should match (if there).
  target_bcm_chassis_map->set_auto_add_slot(
      base_bcm_chassis_map->auto_add_slot());

  // Include the BcmChassis from base_bcm_chassis_map.
  if (base_bcm_chassis_map->has_bcm_chassis()) {
    *target_bcm_chassis_map->mutable_bcm_chassis() =
        base_bcm_chassis_map->bcm_chassis();
  }

  // Validate Node messages. Make sure there is no two nodes with the same id.
  std::map<uint64, int> node_id_to_unit;
  for (const auto& node : config.nodes()) {
    CHECK_RETURN_IF_FALSE(node.slot() > 0)
        << "No positive slot in " << node.ShortDebugString();
    CHECK_RETURN_IF_FALSE(node.id() > 0)
        << "No positive ID in " << node.ShortDebugString();
    CHECK_RETURN_IF_FALSE(
        gtl::InsertIfNotPresent(&node_id_to_unit, node.id(), -1))
        << "The id for Node " << PrintNode(node) << " was already recorded "
        << "for another Node in the config.";
  }

  // Go over all the singleton ports in the config:
  // 1- Validate the basic singleton port properties.
  // 2- For non-flex ports, find the corresponding BcmPort in the
  //    base_bcm_chassis_map and add them to bcm_chassis_map.
  // 3- For flex ports, just save the (slot, port) pairs of flex port groups,
  //    but do not add anything to bcm_chassis_map just yet.
  // 4- Make sure there is no two ports with the same (slot, port, channel).
  // 5- Make sure all the ports with the same (slot, port) have the same
  //    speed.
  // 6- Make sure for each (slot, port) pair, the channels of all the ports
  //    are valid. This depends on the port speed.
  // 7- Make sure no singleton port has the reserved CPU port ID. CPU port is
  //    a special port and is not in the list of singleton ports. It is
  //    configured separately.
  // 8- Make sure IDs of the singleton ports are unique per node.
  // 9- Keep the set of unit numbers that ports are using so that we can later
  //    add the corresponding BcmChips.
  std::map<uint64, std::set<uint32>> node_id_to_port_ids;
  std::set<PortKey> singleton_port_keys;
  std::set<PortKey> flex_port_group_keys;
  std::map<PortKey, std::set<int>> port_group_key_to_channels;
  std::map<PortKey, std::set<uint64>> port_group_key_to_speed_bps;
  std::map<PortKey, std::set<bool>> port_group_key_to_internal;
  for (const auto& singleton_port : config.singleton_ports()) {
    CHECK_RETURN_IF_FALSE(singleton_port.id() > 0)
        << "No positive ID in " << PrintSingletonPort(singleton_port) << ".";
    CHECK_RETURN_IF_FALSE(singleton_port.id() != kCpuPortId)
        << "SingletonPort " << PrintSingletonPort(singleton_port)
        << " has the reserved CPU port ID (" << kCpuPortId << ").";
    CHECK_RETURN_IF_FALSE(singleton_port.slot() > 0)
        << "No valid slot in " << singleton_port.ShortDebugString() << ".";
    CHECK_RETURN_IF_FALSE(singleton_port.port() > 0)
        << "No valid port in " << singleton_port.ShortDebugString() << ".";
    CHECK_RETURN_IF_FALSE(singleton_port.speed_bps() > 0)
        << "No valid speed_bps in " << singleton_port.ShortDebugString() << ".";
    PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                               singleton_port.channel());
    CHECK_RETURN_IF_FALSE(!singleton_port_keys.count(singleton_port_key))
        << "The (slot, port, channel) tuple for SingletonPort "
        << PrintSingletonPort(singleton_port)
        << " was already recorded for another SingletonPort in the config.";
    CHECK_RETURN_IF_FALSE(singleton_port.node() > 0)
        << "No valid node ID in " << singleton_port.ShortDebugString() << ".";
    CHECK_RETURN_IF_FALSE(node_id_to_unit.count(singleton_port.node()))
        << "Node ID " << singleton_port.node() << " given for SingletonPort "
        << PrintSingletonPort(singleton_port)
        << " has not been given to any Node in the config.";
    CHECK_RETURN_IF_FALSE(
        !node_id_to_port_ids[singleton_port.node()].count(singleton_port.id()))
        << "The id for SingletonPort " << PrintSingletonPort(singleton_port)
        << " was already recorded for another SingletonPort for node with ID "
        << singleton_port.node() << ".";
    node_id_to_port_ids[singleton_port.node()].insert(singleton_port.id());
    bool found = false;  // set to true when we find BcmPort for this singleton.
    PortKey port_group_key(singleton_port.slot(), singleton_port.port());
    for (const auto& bcm_port : base_bcm_chassis_map->bcm_ports()) {
      if (IsSingletonPortMatchesBcmPort(singleton_port, bcm_port)) {
        if (bcm_port.flex_port()) {
          // Flex port detected. Add the (slot, port) to flex_port_group_keys
          // set capturing the (slot, port) of all the port groups.
          flex_port_group_keys.insert(port_group_key);
        } else {
          // Make sure the (slot, port) for this port is not in
          // flex_port_group_keys. This is an invalid situation. We either have
          // all the channels of a transceiver port flex or all non-flex.
          CHECK_RETURN_IF_FALSE(!flex_port_group_keys.count(port_group_key))
              << "The (slot, port) pair for the non-flex SingletonPort "
              << PrintSingletonPort(singleton_port)
              << " is in flex_port_group_keys.";
          *target_bcm_chassis_map->add_bcm_ports() = bcm_port;
        }
        if (node_id_to_unit[singleton_port.node()] == -1) {
          // First time we are recording unit for this node.
          node_id_to_unit[singleton_port.node()] = bcm_port.unit();
        } else {
          CHECK_RETURN_IF_FALSE(node_id_to_unit[singleton_port.node()] ==
                                bcm_port.unit())
              << "Inconsistent config. SingletonPort "
              << PrintSingletonPort(singleton_port) << " has Node ID "
              << singleton_port.node()
              << " which was previously attched to unit "
              << node_id_to_unit[singleton_port.node()]
              << ". But BcmChassisMap now suggests unit " << bcm_port.unit()
              << " for this port.";
        }
        found = true;
        singleton_port_keys.insert(singleton_port_key);
        port_group_key_to_internal[port_group_key].insert(bcm_port.internal());
        break;
      }
    }
    CHECK_RETURN_IF_FALSE(found)
        << "Could not find any BcmPort in base_bcm_chassis_map whose (slot, "
        << "port, channel, speed_bps) tuple matches non-flex SingletonPort "
        << PrintSingletonPort(singleton_port) << ".";
    port_group_key_to_channels[port_group_key].insert(singleton_port.channel());
    port_group_key_to_speed_bps[port_group_key].insert(
        singleton_port.speed_bps());
  }

  // If after adding all the we have an entry where unit for a node is not
  // found, it means there was no port for that unit in the config. This is
  // considered an error.
  for (const auto& e : node_id_to_unit) {
    CHECK_RETURN_IF_FALSE(e.second >= 0)
        << "No port found for Node with ID " << e.first << " in the config.";
  }

  // Go over all the trunk ports in the config:
  // 1- Validate the basic trunk port properties.
  // 2- Make sure IDs of the trunk ports are unique per node.
  // 3- Make sure IDs of the trunk ports do not interfere with the IDs if the
  //    singleton ports for each node.
  // 4- Make sure the members of the trunk, if given, are all known singleton
  //    ports.
  std::map<uint64, std::set<uint32>> node_id_to_trunk_ids;
  for (const auto& trunk_port : config.trunk_ports()) {
    CHECK_RETURN_IF_FALSE(trunk_port.id() > 0)
        << "No positive ID in " << PrintTrunkPort(trunk_port) << ".";
    CHECK_RETURN_IF_FALSE(trunk_port.type() != TrunkPort::UNKNOWN_TRUNK)
        << "No type in " << PrintTrunkPort(trunk_port) << ".";
    CHECK_RETURN_IF_FALSE(trunk_port.id() != kCpuPortId)
        << "TrunkPort " << PrintTrunkPort(trunk_port)
        << " has the reserved CPU port ID (" << kCpuPortId << ").";
    CHECK_RETURN_IF_FALSE(trunk_port.node() > 0)
        << "No valid node ID in " << trunk_port.ShortDebugString() << ".";
    CHECK_RETURN_IF_FALSE(node_id_to_unit.count(trunk_port.node()))
        << "Node ID " << trunk_port.node() << " given for TrunkPort "
        << PrintTrunkPort(trunk_port)
        << " has not been given to any Node in the config.";
    CHECK_RETURN_IF_FALSE(
        !node_id_to_trunk_ids[trunk_port.node()].count(trunk_port.id()))
        << "The id for TrunkPort " << PrintTrunkPort(trunk_port)
        << " was already recorded for another TrunkPort for node with ID "
        << trunk_port.node() << ".";
    CHECK_RETURN_IF_FALSE(
        !node_id_to_port_ids[trunk_port.node()].count(trunk_port.id()))
        << "The id for TrunkPort " << PrintTrunkPort(trunk_port)
        << " was already recorded for another SingletonPort for node with ID "
        << trunk_port.node() << ".";
    node_id_to_trunk_ids[trunk_port.node()].insert(trunk_port.id());
    for (uint32 port_id : trunk_port.members()) {
      CHECK_RETURN_IF_FALSE(
          node_id_to_port_ids[trunk_port.node()].count(port_id))
          << "Unknown member SingletonPort " << port_id << " for TrunkPort "
          << PrintTrunkPort(trunk_port) << ".";
    }
  }

  // 1- Add all the BcmChips corresponding to the nodes with the detected unit
  //    numbers.
  // 2- Make sure the chip type is supported.
  for (const auto& e : node_id_to_unit) {
    int unit = e.second;
    bool found = false;  // set to true when we find BcmChip for this node.
    for (const auto& bcm_chip : base_bcm_chassis_map->bcm_chips()) {
      if (unit == bcm_chip.unit()) {
        CHECK_RETURN_IF_FALSE(supported_chip_types.count(bcm_chip.type()))
            << "Chip type " << BcmChip::BcmChipType_Name(bcm_chip.type())
            << " is not supported on platform "
            << Platform_Name(config.chassis().platform()) << ".";
        *target_bcm_chassis_map->add_bcm_chips() = bcm_chip;
        found = true;
        break;
      }
    }
    CHECK_RETURN_IF_FALSE(found) << "Could not find any BcmChip for unit "
                                 << unit << " in base_bcm_chassis_map.";
  }

  // Validate internal ports if any.
  for (const auto& e : port_group_key_to_internal) {
    CHECK_RETURN_IF_FALSE(e.second.size() == 1)
        << "For SingletonPorts with " << e.first.ToString()
        << " found both internal and external BCM ports. This is invalid.";
  }

  // Validate the speed_bps and channels for all transceiver ports.
  absl::flat_hash_map<uint64, std::set<int>> speed_bps_to_expected_channels = {
      {kOneGigBps, {0}},         {kHundredGigBps, {0}},
      {kFortyGigBps, {0}},       {kFiftyGigBps, {1, 2}},
      {kTwentyGigBps, {1, 2}},   {kTwentyFiveGigBps, {1, 2, 3, 4}},
      {kTenGigBps, {1, 2, 3, 4}}};
  for (const auto& e : port_group_key_to_speed_bps) {
    const PortKey& port_group_key = e.first;
    CHECK_RETURN_IF_FALSE(e.second.size() == 1)
        << "For SingletonPorts with " << e.first.ToString() << " found "
        << e.second.size() << " different speed_bps. This is invalid.";
    uint64 speed_bps = *e.second.begin();
    const std::set<int>* expected_channels =
        gtl::FindOrNull(speed_bps_to_expected_channels, speed_bps);
    CHECK_RETURN_IF_FALSE(expected_channels != nullptr)
        << "Unsupported speed_bps: " << speed_bps << ".";
    const std::set<int>& existing_channels =
        port_group_key_to_channels[port_group_key];
    CHECK_RETURN_IF_FALSE(
        std::includes(expected_channels->begin(), expected_channels->end(),
                      existing_channels.begin(), existing_channels.end()))
        << "For SingletonPorts with " << e.first.ToString()
        << " and speed_bps = " << speed_bps << " found invalid channels.";
  }

  // Now add the flex ports. For each flex port, we add all the 4 channels
  // with a specific speed which depends on the chip.
  for (const auto& port_group_key : flex_port_group_keys) {
    // Find the BcmChip that contains this (slot, port) pair. We expect the will
    // be one and only one BcmChip what contains this pair.
    std::set<int> units;
    for (const auto& bcm_port : base_bcm_chassis_map->bcm_ports()) {
      if (bcm_port.slot() == port_group_key.slot &&
          bcm_port.port() == port_group_key.port) {
        units.insert(bcm_port.unit());
      }
    }
    CHECK_RETURN_IF_FALSE(units.size() == 1U)
        << "Found ports with (slot, port) = (" << port_group_key.slot << ", "
        << port_group_key.port << ") that are on different chips.";
    int unit = *units.begin();
    // We dont use GetBcmChip as unit_to_bcm_chip_ may not be populated when
    // this function is called. This function must be self contained.
    BcmChip::BcmChipType chip_type = BcmChip::UNKNOWN;
    for (const auto& bcm_chip : base_bcm_chassis_map->bcm_chips()) {
      if (bcm_chip.unit() == unit) {
        chip_type = bcm_chip.type();
        break;
      }
    }
    // For each (slot, port) pair, we need to populate all the 4 channels. The
    // speed for these channels depends on the chip type.
    std::vector<int> channels = {1, 2, 3, 4};
    uint64 min_speed_bps;
    switch (chip_type) {
      case BcmChip::TOMAHAWK:
      case BcmChip::TOMAHAWK_PLUS:
        min_speed_bps = kTwentyFiveGigBps;
        break;
      case BcmChip::TRIDENT_PLUS:
      case BcmChip::TRIDENT2:
        min_speed_bps = kTenGigBps;
        break;
      default:
        return MAKE_ERROR(ERR_INTERNAL) << "Un-supported BCM chip type: "
                                        << BcmChip::BcmChipType_Name(chip_type);
    }
    for (const int channel : channels) {
      SingletonPort singleton_port;
      singleton_port.set_slot(port_group_key.slot);
      singleton_port.set_port(port_group_key.port);
      singleton_port.set_channel(channel);
      singleton_port.set_speed_bps(min_speed_bps);
      bool found = false;
      for (const auto& bcm_port : base_bcm_chassis_map->bcm_ports()) {
        if (IsSingletonPortMatchesBcmPort(singleton_port, bcm_port)) {
          *target_bcm_chassis_map->add_bcm_ports() = bcm_port;
          found = true;
          break;
        }
      }
      CHECK_RETURN_IF_FALSE(found)
          << "Could not find any BcmPort in base_bcm_chassis_map whose (slot, "
          << "port, channel, speed_bps) tuple matches flex SingletonPort "
          << PrintSingletonPort(singleton_port);
    }
  }

  // Now, we need to find the map form unit to PortKey instances encapsulating
  // the (slot, port, channel) of all the BcmPort messages in the chassis map,
  // as well as the map from unit to chip types. These maps are used for two
  // purposes:
  // 1- Check for max number of ports per chip.
  // 2- For the case logical ports are expected to be auto added by the
  //    software. In this case, we rewrite the logical port numbers based on
  //    the index of the port within the chip, starting from '1'.
  std::map<int, std::set<PortKey>> unit_to_bcm_port_keys;
  std::map<int, BcmChip::BcmChipType> unit_to_chip_type;
  for (const auto& bcm_chip : target_bcm_chassis_map->bcm_chips()) {
    unit_to_chip_type[bcm_chip.unit()] = bcm_chip.type();
  }
  for (const auto& bcm_port : target_bcm_chassis_map->bcm_ports()) {
    // MGMT and GE ports are not considered here. Only regular data plane
    // ports are subjected to a max number of ports per chip.
    if (bcm_port.type() != BcmPort::GE && bcm_port.type() != BcmPort::MGMT) {
      unit_to_bcm_port_keys[bcm_port.unit()].emplace(
          bcm_port.slot(), bcm_port.port(), bcm_port.channel());
    }
  }

  // Check for max num of ports per chip.
  std::map<BcmChip::BcmChipType, size_t> chip_type_to_max_num_ports = {
      {BcmChip::TRIDENT_PLUS, kTridentPlusMaxBcmPortsPerChip},
      {BcmChip::TRIDENT2, kTrident2MaxBcmPortsPerChip},
      {BcmChip::TOMAHAWK, kTomahawkMaxBcmPortsPerChip},
      {BcmChip::TOMAHAWK_PLUS, kTomahawkPlusMaxBcmPortsPerChip}};
  for (const auto& e : unit_to_chip_type) {
    CHECK_RETURN_IF_FALSE(unit_to_bcm_port_keys[e.first].size() <=
                          chip_type_to_max_num_ports[e.second])
        << "Max num of BCM ports for a " << BcmChip::BcmChipType_Name(e.second)
        << " chip is " << chip_type_to_max_num_ports[e.second]
        << ", but we found " << unit_to_bcm_port_keys[e.first].size()
        << " ports.";
  }

  // Auto add logical_port numbers for the BCM ports if requested.
  if (target_bcm_chassis_map->auto_add_logical_ports()) {
    // The logical_port will be the 1-based index of the corresponding
    // (slot, port, channel) tuple in the sorted list of tuples found for the
    // unit hosting the port.
    for (auto& bcm_port : *target_bcm_chassis_map->mutable_bcm_ports()) {
      const auto& bcm_port_keys = unit_to_bcm_port_keys[bcm_port.unit()];
      PortKey bcm_port_key(bcm_port.slot(), bcm_port.port(),
                           bcm_port.channel());
      auto it = bcm_port_keys.find(bcm_port_key);
      CHECK_RETURN_IF_FALSE(it != bcm_port_keys.end())
          << "Invalid state. " << bcm_port_key.ToString()
          << " is not found on unit " << bcm_port.unit() << ".";
      int idx = std::distance(bcm_port_keys.begin(), it);
      // Make sure the logical ports start from 1, so we skip the CMIC port (
      // logical port 0).
      bcm_port.set_logical_port(idx + 1);
    }
  }

  // Need to add logical_port for GE port on T+. This is a target specific logic
  // and works as follows:
  // 1- We find all the ports is the range [1..32] (T+ has 32 logical ports
  //    max per chip in X pipeline), that are not assigned to any logical_port
  //    for any XE port.
  // 2- If there is no such port, we return an error. This means we have a
  //    case where all the 32 ports in X pipeline on a T+ are used and there is
  //     no room to add the GE port.
  // 3- If there are a couple of unused numbers in the range, we pick the
  //    largest number as the logical_port number for the GE port.
  // Note that inclusion of a GE port or whether it is at all needed to be
  // enabled on a T+ is up to the config generator.
  for (auto& bcm_port : *target_bcm_chassis_map->mutable_bcm_ports()) {
    if (IsGePortOnTridentPlus(bcm_port, *target_bcm_chassis_map)) {
      std::set<int> free_logical_ports;
      for (int i = 1; i <= kTridentPlusMaxBcmPortsInXPipeline; ++i) {
        free_logical_ports.insert(i);
      }
      for (const auto& p : target_bcm_chassis_map->bcm_ports()) {
        if (p.type() != BcmPort::GE && p.unit() == bcm_port.unit()) {
          free_logical_ports.erase(p.logical_port());
        }
      }
      CHECK_RETURN_IF_FALSE(!free_logical_ports.empty())
          << "There is no empty logical_port in X pipeline of the T+ chip to "
          << "assign to GE port " << PrintBcmPort(bcm_port) << ".";
      bcm_port.set_logical_port(*free_logical_ports.rbegin());
    }
  }

  // Post validation of target_bcm_chassis_map by checking the validity of the
  // internal BCM ports.
  std::map<int, std::set<int>> unit_to_bcm_phy_ports;
  std::map<int, std::set<int>> unit_to_bcm_diag_ports;
  std::map<int, std::set<int>> unit_to_bcm_logical_ports;
  for (const auto& bcm_chip : target_bcm_chassis_map->bcm_chips()) {
    // For all the BCM unit, fixed CPU logical_port cannot be used for anything
    // else.
    unit_to_bcm_logical_ports[bcm_chip.unit()].insert(kCpuLogicalPort);
  }

  for (const auto& bcm_port : target_bcm_chassis_map->bcm_ports()) {
    CHECK_RETURN_IF_FALSE(
        !unit_to_bcm_phy_ports[bcm_port.unit()].count(bcm_port.physical_port()))
        << "Duplicate BCM physcial_port for unit " << bcm_port.unit() << ": "
        << bcm_port.physical_port();
    CHECK_RETURN_IF_FALSE(
        !unit_to_bcm_diag_ports[bcm_port.unit()].count(bcm_port.diag_port()))
        << "Duplicate BCM diag_port for unit " << bcm_port.unit() << ": "
        << bcm_port.diag_port();
    CHECK_RETURN_IF_FALSE(!unit_to_bcm_logical_ports[bcm_port.unit()].count(
        bcm_port.logical_port()))
        << "Duplicate BCM logical_port for unit " << bcm_port.unit() << ": "
        << bcm_port.ShortDebugString();
    unit_to_bcm_phy_ports[bcm_port.unit()].insert(bcm_port.physical_port());
    unit_to_bcm_diag_ports[bcm_port.unit()].insert(bcm_port.diag_port());
    unit_to_bcm_logical_ports[bcm_port.unit()].insert(bcm_port.logical_port());
  }

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::InitializeBcmChips(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "InitializeBcmChips() can be called only before the class is "
           << "initialized.";
  }

  // Need to make sure target_bcm_chassis_map given here is a pruned version of
  // the base_bcm_chassis_map.
  CHECK_RETURN_IF_FALSE(base_bcm_chassis_map.id() ==
                        target_bcm_chassis_map.id())
      << "The value of 'id' in base_bcm_chassis_map and "
      << "target_bcm_chassis_map must match (" << base_bcm_chassis_map.id()
      << " != " << target_bcm_chassis_map.id() << ").";
  CHECK_RETURN_IF_FALSE(base_bcm_chassis_map.auto_add_logical_ports() ==
                        target_bcm_chassis_map.auto_add_logical_ports())
      << "The value of 'auto_add_logical_ports' in base_bcm_chassis_map and "
      << "target_bcm_chassis_map must match.";
  CHECK_RETURN_IF_FALSE(base_bcm_chassis_map.has_bcm_chassis() ==
                        target_bcm_chassis_map.has_bcm_chassis())
      << "Both base_bcm_chassis_map and target_bcm_chassis_map must either "
      << "have 'bcm_chassis' or miss it.";
  if (target_bcm_chassis_map.has_bcm_chassis()) {
    CHECK_RETURN_IF_FALSE(ProtoEqual(target_bcm_chassis_map.bcm_chassis(),
                                     base_bcm_chassis_map.bcm_chassis()))
        << "BcmChassis in base_bcm_chassis_map and target_bcm_chassis_map do "
        << "not match.";
  }
  for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
    CHECK_RETURN_IF_FALSE(std::any_of(base_bcm_chassis_map.bcm_chips().begin(),
                          base_bcm_chassis_map.bcm_chips().end(),
                          [&bcm_chip](const ::google::protobuf::Message& x) {
                                        return ProtoEqual(x, bcm_chip);
                                      }))
        << "BcmChip " << bcm_chip.ShortDebugString() << " was not found in "
        << "base_bcm_chassis_map.";
  }
  std::stringstream ss;
  ss << "Portmap:\nPanel, logical (PORT_ID), physical (PC_PHYS_PORT_ID)\n";
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    BcmPort p(bcm_port);
    if (target_bcm_chassis_map.auto_add_logical_ports() ||
        IsGePortOnTridentPlus(bcm_port, target_bcm_chassis_map)) {
      // The base comes with no logical_port assigned.
      p.clear_logical_port();
    }
    CHECK_RETURN_IF_FALSE(std::any_of(
        base_bcm_chassis_map.bcm_ports().begin(),
        base_bcm_chassis_map.bcm_ports().end(),
        [&p](const ::google::protobuf::Message& x) {
          return ProtoEqual(x, p); }))
        << "BcmPort " << p.ShortDebugString() << " was not found in "
        << "base_bcm_chassis_map.";
    ss << absl::StrFormat("%3i, %3i, %3i\n", bcm_port.port(),
                          bcm_port.logical_port(), bcm_port.physical_port());
  }
  LOG(INFO) << ss.str();

  // Generate the config.bcm file given target_bcm_chassis_map.
  RETURN_IF_ERROR(
      WriteBcmConfigFile(base_bcm_chassis_map, target_bcm_chassis_map));

  // Create SDK checkpoint dir. This needs to be create before SDK is
  // initialized.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_bcm_sdk_checkpoint_dir));

  // Initialize the SDK.
  RETURN_IF_ERROR(bcm_sdk_interface_->InitializeSdk(
      FLAGS_bcm_sdk_config_file, FLAGS_bcm_sdk_config_flush_file,
      FLAGS_bcm_sdk_shell_log_file));

  // Attach all the units. Note that we keep the things simple. We will move
  // forward iff all the units are attched successfully.
  for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
    RETURN_IF_ERROR(
        bcm_sdk_interface_->FindUnit(bcm_chip.unit(), bcm_chip.pci_bus(),
                                     bcm_chip.pci_slot(), bcm_chip.type()));
    RETURN_IF_ERROR(bcm_sdk_interface_->InitializeUnit(bcm_chip.unit(),
                                                       /*warm_boot=*/false));
    RETURN_IF_ERROR(
        bcm_sdk_interface_->SetModuleId(bcm_chip.unit(), bcm_chip.module()));
  }

  // Initialize all the ports (flex or not).
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    RETURN_IF_ERROR(bcm_sdk_interface_->InitializePort(
        bcm_port.unit(), bcm_port.logical_port()));
  }

  // Start the diag thread.
  RETURN_IF_ERROR(bcm_sdk_interface_->StartDiagShellServer());

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::InitializeInternalState(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL) << "InitializeInternalState() can be "
                                    << "called only before the class is "
                                    << "initialized.";
  }

  // By the time we get here, target_bcm_chassis_map is verified and the chips
  // has been initialized using it, save the copy of this proto and
  // base_bcm_chassis_map.
  base_bcm_chassis_map_ =
      absl::make_unique<BcmChassisMap>(base_bcm_chassis_map);
  applied_bcm_chassis_map_ =
      absl::make_unique<BcmChassisMap>(target_bcm_chassis_map);

  // Also, after initialization is done for all the ports, set the initial state
  // of the transceivers.
  xcvr_port_key_to_xcvr_state_.clear();
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    PortKey port_group_key(bcm_port.slot(), bcm_port.port());
    // For external ports, wait for transceiver module event handler to find all
    // the inserted transceiver modules (QSFPs, SFPs, etc). For internal ports,
    // there is no transceiver module event. They are always up, but we set them
    // as HW_STATE_PRESENT (unconfigured) so they get
    // configured later.
    if (bcm_port.internal()) {
      xcvr_port_key_to_xcvr_state_[port_group_key] = HW_STATE_PRESENT;
    } else {
      xcvr_port_key_to_xcvr_state_[port_group_key] = HW_STATE_UNKNOWN;
    }
  }

  // Write applied_bcm_chassis_map_ into file for debugging purposes.
  if (!FLAGS_applied_bcm_chassis_map_file.empty()) {
    RETURN_IF_ERROR(WriteProtoToTextFile(*applied_bcm_chassis_map_,
                                         FLAGS_applied_bcm_chassis_map_file));
  }

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::SyncInternalState(
    const ChassisConfig& config) {
  // Populate the internal map. We have done verification before we get to
  // this point. So, no need to re-verify the config.
  gtl::STLDeleteValues(&unit_to_bcm_chip_);
  gtl::STLDeleteValues(&singleton_port_key_to_bcm_port_);
  port_group_key_to_flex_bcm_ports_.clear();  // no need to delete the pointer
  port_group_key_to_non_flex_bcm_ports_
      .clear();  // no need to delete the pointer
  node_id_to_unit_.clear();
  unit_to_node_id_.clear();
  node_id_to_port_ids_.clear();
  node_id_to_trunk_ids_.clear();
  node_id_to_port_id_to_singleton_port_key_.clear();
  node_id_to_port_id_to_sdk_port_.clear();
  node_id_to_trunk_id_to_sdk_trunk_.clear();
  node_id_to_sdk_port_to_port_id_.clear();
  node_id_to_sdk_trunk_to_trunk_id_.clear();
  node_id_to_trunk_id_to_trunk_state_.clear();
  node_id_to_trunk_id_to_members_.clear();
  node_id_to_port_id_to_trunk_membership_info_.clear();

  // Initialize the maps that have node ID as key, i.e. node_id_to_unit_,
  // node_id_to_port_ids_, node_id_to_trunk_ids_, etc.
  for (const auto& node : config.nodes()) {
    node_id_to_unit_[node.id()] = -1;
    node_id_to_port_ids_[node.id()] = {};
    node_id_to_trunk_ids_[node.id()] = {};
    node_id_to_port_id_to_singleton_port_key_[node.id()] = {};
    node_id_to_port_id_to_sdk_port_[node.id()] = {};
    node_id_to_trunk_id_to_sdk_trunk_[node.id()] = {};
    node_id_to_sdk_port_to_port_id_[node.id()] = {};
    node_id_to_sdk_trunk_to_trunk_id_[node.id()] = {};
    node_id_to_trunk_id_to_trunk_state_[node.id()] = {};
    node_id_to_trunk_id_to_members_[node.id()] = {};
    node_id_to_port_id_to_trunk_membership_info_[node.id()] = {};
  }

  // Now populate unit_to_bcm_chip_. The nodes are already in
  // applied_bcm_chassis_map_ which was updated in InitializeInternalState().
  // The nodes in applied_bcm_chassis_map_ cannot be changed after the first
  // config push.
  for (const auto& bcm_chip : applied_bcm_chassis_map_->bcm_chips()) {
    unit_to_bcm_chip_[bcm_chip.unit()] = new BcmChip(bcm_chip);
  }

  // Now populate port-related maps.

  // Temporary maps to hold the port state, admin state, and health state.
  std::map<uint64, std::map<uint32, PortState>>
      tmp_node_id_to_port_id_to_port_state;
  std::map<uint64, std::map<uint32, AdminState>>
      tmp_node_id_to_port_id_to_admin_state;
  std::map<uint64, std::map<uint32, HealthState>>
      tmp_node_id_to_port_id_to_health_state;
  std::map<uint64, std::map<uint32, LoopbackState>>
      tmp_node_id_to_port_id_to_loopback_state;
  ::util::Status error = ::util::OkStatus();  // errors to keep track of.
  for (const auto& singleton_port : config.singleton_ports()) {
    for (const auto& bcm_port : base_bcm_chassis_map_->bcm_ports()) {
      if (IsSingletonPortMatchesBcmPort(singleton_port, bcm_port)) {
        PortKey singleton_port_key(singleton_port.slot(), singleton_port.port(),
                                   singleton_port.channel());
        CHECK_RETURN_IF_FALSE(
            !singleton_port_key_to_bcm_port_.count(singleton_port_key))
            << "The (slot, port, channel) tuple for SingletonPort "
            << PrintSingletonPort(singleton_port)
            << " already exists as a key in singleton_port_key_to_bcm_port_. "
            << "Have you called VerifyChassisConfig()?";
        auto* p = new BcmPort(bcm_port);
        // If auto_add_logical_ports=true or the port is a GE port on a T+, the
        // logical_port needs to come from applied_bcm_chassis_map_.
        if (applied_bcm_chassis_map_->auto_add_logical_ports() ||
            IsGePortOnTridentPlus(bcm_port, *applied_bcm_chassis_map_)) {
          bool found = false;
          for (const auto& q : applied_bcm_chassis_map_->bcm_ports()) {
            if (p->unit() == q.unit() &&
                p->physical_port() == q.physical_port() &&
                p->diag_port() == q.diag_port()) {
              p->set_logical_port(q.logical_port());
              found = true;
              break;
            }
          }
          CHECK_RETURN_IF_FALSE(found)
              << "Found no matching BcmPort in applied_bcm_chassis_map_ which "
              << "matches unit, physical_port and diag_port of BcmPort '"
              << p->ShortDebugString() << "'.";
        }
        singleton_port_key_to_bcm_port_[singleton_port_key] = p;
        uint64 node_id = singleton_port.node();  // already verified as known
        uint32 port_id = singleton_port.id();    // already verified as known
        node_id_to_unit_[node_id] = p->unit();
        unit_to_node_id_[p->unit()] = node_id;
        node_id_to_port_ids_[node_id].insert(port_id);
        node_id_to_port_id_to_singleton_port_key_[node_id][port_id] =
            singleton_port_key;
        SdkPort sdk_port(p->unit(), p->logical_port());
        node_id_to_port_id_to_sdk_port_[node_id][port_id] = sdk_port;
        node_id_to_sdk_port_to_port_id_[node_id][sdk_port] = port_id;
        PortKey xcvr_port_key(singleton_port.slot(), singleton_port.port());
        CHECK_RETURN_IF_FALSE(xcvr_port_key_to_xcvr_state_.count(xcvr_port_key))
            << "Something is wrong. ChassisConfig contains a (slot, port) "
            << "which we dont know about: " << xcvr_port_key.ToString() << ".";
        // The xcvr_port_key can be also used as a key to identify the
        // (slot, port) of the port group.
        if (bcm_port.flex_port()) {
          port_group_key_to_flex_bcm_ports_[xcvr_port_key].push_back(p);
        } else {
          port_group_key_to_non_flex_bcm_ports_[xcvr_port_key].push_back(p);
        }
        // If (node_id, port_id) already exists as a key in any of
        // node_id_to_port_id_to_{port,health,loopback}_state_, we keep the
        // state as is. Otherwise, we assume this is the first time we are
        // seeing this port and set the state to unknown.
        const PortState* port_state = gtl::FindOrNull(
            node_id_to_port_id_to_port_state_[node_id], port_id);
        if (port_state != nullptr) {
          tmp_node_id_to_port_id_to_port_state[node_id][port_id] = *port_state;
        } else {
          tmp_node_id_to_port_id_to_port_state[node_id][port_id] =
              PORT_STATE_UNKNOWN;
        }
        const HealthState* health_state = gtl::FindOrNull(
            node_id_to_port_id_to_health_state_[node_id], port_id);
        if (health_state != nullptr) {
          tmp_node_id_to_port_id_to_health_state[node_id][port_id] =
              *health_state;
        } else {
          tmp_node_id_to_port_id_to_health_state[node_id][port_id] =
              HEALTH_STATE_UNKNOWN;
        }
        const LoopbackState* loopback_state = gtl::FindOrNull(
            node_id_to_port_id_to_loopback_state_[node_id], port_id);
        if (loopback_state != nullptr) {
          tmp_node_id_to_port_id_to_loopback_state[node_id][port_id] =
              *loopback_state;
        } else {
          tmp_node_id_to_port_id_to_loopback_state[node_id][port_id] =
              LOOPBACK_STATE_UNKNOWN;
        }
        // For the admin state, the admin state specified in the config
        // overrides the previous admin state. But if there is no valid admin
        // state specified for the port in the confing and there is already an
        // admin state for the port in node_id_to_port_id_to_admin_state_, we
        // keep the state as is.
        AdminState new_admin_state =
            singleton_port.config_params().admin_state();
        const AdminState* old_admin_state = gtl::FindOrNull(
            node_id_to_port_id_to_admin_state_[node_id], port_id);
        if (old_admin_state != nullptr) {
          // The port already exists as a key in the map. If the new config
          // does not have a valid admin state, keep the old state. Otherwise,
          // save the new state and if there is a change in the state (old vs
          // new), enable/disable the port accordingly.
          if (new_admin_state == ADMIN_STATE_UNKNOWN) {
            tmp_node_id_to_port_id_to_admin_state[node_id][port_id] =
                *old_admin_state;
          } else {
            tmp_node_id_to_port_id_to_admin_state[node_id][port_id] =
                new_admin_state;
            if (new_admin_state != *old_admin_state) {
              APPEND_STATUS_IF_ERROR(
                  error,
                  EnablePort(sdk_port, new_admin_state == ADMIN_STATE_ENABLED));
            }
          }
        } else {
          // First time we are seeing the port. Need to honor the state
          // specified in the config and enable/disable the port accordingly.
          tmp_node_id_to_port_id_to_admin_state[node_id][port_id] =
              new_admin_state;
          if (new_admin_state != ADMIN_STATE_UNKNOWN) {
            APPEND_STATUS_IF_ERROR(
                error,
                EnablePort(sdk_port, new_admin_state == ADMIN_STATE_ENABLED));
          }
        }
        LoopbackState new_loopback_state =
            singleton_port.config_params().loopback_mode();
        const LoopbackState* old_loopback_state = gtl::FindOrNull(
            node_id_to_port_id_to_loopback_state_[node_id], port_id);
        if (old_loopback_state != nullptr) {
          // The port already exists as a key in the map. If the new config
          // does not have a valid loopback state, keep the old state.
          // Otherwise, save the new state and if there is a change in the state
          // (old vs new), configure the port accordingly.
          if (new_loopback_state == LOOPBACK_STATE_UNKNOWN) {
            tmp_node_id_to_port_id_to_loopback_state[node_id][port_id] =
                *old_loopback_state;
          } else {
            tmp_node_id_to_port_id_to_loopback_state[node_id][port_id] =
                new_loopback_state;
          }
        } else {
          // First time we are seeing the port. Need to honor the state
          // specified in the config and set the loopback mode accordingly.
          tmp_node_id_to_port_id_to_loopback_state[node_id][port_id] =
              new_loopback_state;
        }
        APPEND_STATUS_IF_ERROR(
            error, LoopbackPort(sdk_port, new_loopback_state));
      }
    }
  }
  node_id_to_port_id_to_port_state_ = tmp_node_id_to_port_id_to_port_state;
  node_id_to_port_id_to_admin_state_ = tmp_node_id_to_port_id_to_admin_state;
  node_id_to_port_id_to_health_state_ = tmp_node_id_to_port_id_to_health_state;
  node_id_to_port_id_to_loopback_state_ =
      tmp_node_id_to_port_id_to_loopback_state;

  // Finally populate trunk-related maps.
  for (const auto& trunk_port : config.trunk_ports()) {
    uint64 node_id = trunk_port.node();    // already verified as known
    uint32 trunk_id = trunk_port.id();     // already verified as known
    int unit = node_id_to_unit_[node_id];  // already verified as known
    // TODO(unknown): Populate the rest of trunk related maps. Also add support
    // for restoring trunk state/members. At the moment, we populate the maps
    // with invalid data.
    node_id_to_trunk_ids_[node_id].insert(trunk_id);
    SdkTrunk sdk_trunk(unit, /*invalid*/ -1);
    node_id_to_trunk_id_to_sdk_trunk_[node_id][trunk_id] = sdk_trunk;
    node_id_to_sdk_trunk_to_trunk_id_[node_id][sdk_trunk] = trunk_id;
    node_id_to_trunk_id_to_trunk_state_[node_id][trunk_id] =
        TRUNK_STATE_UNKNOWN;
    node_id_to_trunk_id_to_members_[node_id][trunk_id] = {};
  }

  // TODO(unknown): Update the LED of all the ports.

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::RegisterEventWriters() {
  if (initialized_) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "RegisterEventWriters() can be called only before the class is "
           << "initialized.";
  }

  // If we have not done that yet, create linkscan event Channel, register
  // Writer, and create Reader thread.
  if (linkscan_event_writer_id_ == kInvalidWriterId) {
    linkscan_event_channel_ =
        Channel<LinkscanEvent>::Create(kMaxLinkscanEventDepth);
    // Create and hand-off Writer to the BcmSdkInterface.
    auto writer = ChannelWriter<LinkscanEvent>::Create(linkscan_event_channel_);
    int priority = BcmSdkInterface::kLinkscanEventWriterPriorityHigh;
    ASSIGN_OR_RETURN(linkscan_event_writer_id_,
                     bcm_sdk_interface_->RegisterLinkscanEventWriter(
                         std::move(writer), priority));
    // Create and hand-off Reader to new reader thread.
    auto reader = ChannelReader<LinkscanEvent>::Create(linkscan_event_channel_);
    pthread_t linkscan_event_reader_tid;
    int ret = pthread_create(
        &linkscan_event_reader_tid, nullptr, LinkscanEventHandlerThreadFunc,
        new ReaderArgs<LinkscanEvent>{this, std::move(reader)});
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to create linkscan thread. Err: " << ret << ".";
    }
    // We don't care about the return value. The thread should exit following
    // the closing of the Channel in UnregisterEventWriters().
    ret = pthread_detach(linkscan_event_reader_tid);
    if (ret != 0) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to detach linkscan thread. Err: " << ret << ".";
    }
    // Start the linkscan.
    for (const auto& e : unit_to_bcm_chip_) {
      RETURN_IF_ERROR(bcm_sdk_interface_->StartLinkscan(e.first));
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

::util::Status BcmChassisManager::UnregisterEventWriters() {
  ::util::Status status = ::util::OkStatus();
  // Unregister the linkscan and transceiver module event Writers.
  if (linkscan_event_writer_id_ != kInvalidWriterId) {
    APPEND_STATUS_IF_ERROR(status,
                           bcm_sdk_interface_->UnregisterLinkscanEventWriter(
                               linkscan_event_writer_id_));
    linkscan_event_writer_id_ = kInvalidWriterId;
    // Close Channel.
    if (!linkscan_event_channel_ || !linkscan_event_channel_->Close()) {
      APPEND_ERROR(status) << "Linkscan event Channel is already closed.";
    }
    linkscan_event_channel_.reset();
  }
  if (xcvr_event_writer_id_ != kInvalidWriterId) {
    APPEND_STATUS_IF_ERROR(status,
                           phal_interface_->UnregisterTransceiverEventWriter(
                               xcvr_event_writer_id_));
    xcvr_event_writer_id_ = kInvalidWriterId;
    // Close Channel.
    if (!xcvr_event_channel_ || !xcvr_event_channel_->Close()) {
      APPEND_ERROR(status) << "Transceiver event Channel is already closed.";
    }
    xcvr_event_channel_.reset();
  }

  return status;
}

::util::Status BcmChassisManager::RegisterEventNotifyWriter(
    std::shared_ptr<WriterInterface<GnmiEventPtr>> writer) {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = writer;
  return ::util::OkStatus();
}

::util::Status BcmChassisManager::UnregisterEventNotifyWriter() {
  absl::WriterMutexLock l(&gnmi_event_lock_);
  gnmi_event_writer_ = nullptr;
  return ::util::OkStatus();
}

::util::Status BcmChassisManager::ConfigurePortGroups() {
  ::util::Status status = ::util::OkStatus();
  // Set the speed for flex port groups first.
  for (const auto& e : port_group_key_to_flex_bcm_ports_) {
    ::util::StatusOr<bool> ret = SetSpeedForFlexPortGroup(e.first);
    if (!ret.ok()) {
      APPEND_STATUS_IF_ERROR(status, ret.status());
      continue;
    }
    bool speed_changed = ret.ValueOrDie();
    // If there is a change in port speed and port is HW_STATE_READY, set it
    // to HW_STATE_PRESENT (non-configured state) so it gets configured next.
    if (speed_changed &&
        xcvr_port_key_to_xcvr_state_[e.first] == HW_STATE_READY) {
      xcvr_port_key_to_xcvr_state_[e.first] = HW_STATE_PRESENT;
    }
  }
  // Then continue with port options.
  for (auto& e : xcvr_port_key_to_xcvr_state_) {
    if (e.second != HW_STATE_READY) {
      // Set the speed for non-flex ports.
      // TODO(max): This check is not perfect since it always excludes flex
      // ports, ideally we would set the speed of non-flex ports above.
      BcmPortOptions options;
      const auto bcm_ports =
          gtl::FindOrNull(port_group_key_to_non_flex_bcm_ports_, e.first);
      if (bcm_ports != nullptr && !bcm_ports->empty()) {
        options.set_speed_bps(bcm_ports->at(0)->speed_bps());
      }
      options.set_enabled(e.second == HW_STATE_PRESENT ? TRI_STATE_TRUE
                                                       : TRI_STATE_FALSE);
      options.set_blocked(e.second != HW_STATE_PRESENT ? TRI_STATE_TRUE
                                                       : TRI_STATE_FALSE);
      ::util::Status error = SetPortOptionsForPortGroup(e.first, options);
      if (!error.ok()) {
        APPEND_STATUS_IF_ERROR(status, error);
        continue;
      }
      if (e.second == HW_STATE_PRESENT) {
        // A HW_STATE_PRESENT port group after configuration is HW_STATE_READY.
        e.second = HW_STATE_READY;
      }
    }
  }

  return status;
}

void BcmChassisManager::CleanupInternalState() {
  gtl::STLDeleteValues(&unit_to_bcm_chip_);
  gtl::STLDeleteValues(&singleton_port_key_to_bcm_port_);
  port_group_key_to_flex_bcm_ports_.clear();      // no need to delete pointer
  port_group_key_to_non_flex_bcm_ports_.clear();  // no need to delete pointer
  node_id_to_unit_.clear();
  unit_to_node_id_.clear();
  node_id_to_port_ids_.clear();
  node_id_to_trunk_ids_.clear();
  node_id_to_port_id_to_singleton_port_key_.clear();
  node_id_to_port_id_to_sdk_port_.clear();
  node_id_to_trunk_id_to_sdk_trunk_.clear();
  node_id_to_sdk_port_to_port_id_.clear();
  node_id_to_sdk_trunk_to_trunk_id_.clear();
  xcvr_port_key_to_xcvr_state_.clear();
  node_id_to_port_id_to_port_state_.clear();
  node_id_to_trunk_id_to_trunk_state_.clear();
  node_id_to_trunk_id_to_members_.clear();
  node_id_to_port_id_to_trunk_membership_info_.clear();
  node_id_to_port_id_to_admin_state_.clear();
  node_id_to_port_id_to_health_state_.clear();
  node_id_to_port_id_to_loopback_state_.clear();
  base_bcm_chassis_map_ = nullptr;
  applied_bcm_chassis_map_ = nullptr;
}

::util::Status BcmChassisManager::ReadBaseBcmChassisMapFromFile(
    const std::string& bcm_chassis_map_id,
    BcmChassisMap* base_bcm_chassis_map) const {
  if (base_bcm_chassis_map == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Did you pass a null base_bcm_chassis_map pointer?";
  }

  // Read the proto from the path given by base_bcm_chassis_map_file flag.
  BcmChassisMapList bcm_chassis_map_list;
  RETURN_IF_ERROR(ReadProtoFromTextFile(FLAGS_base_bcm_chassis_map_file,
                                        &bcm_chassis_map_list));
  base_bcm_chassis_map->Clear();
  bool found = false;
  for (const auto& bcm_chassis_map : bcm_chassis_map_list.bcm_chassis_maps()) {
    if (bcm_chassis_map_id.empty() ||
        bcm_chassis_map_id == bcm_chassis_map.id()) {
      *base_bcm_chassis_map = bcm_chassis_map;
      found = true;
      break;
    }
  }
  CHECK_RETURN_IF_FALSE(found)
      << "Did not find a BcmChassisMap with id " << bcm_chassis_map_id << " in "
      << FLAGS_base_bcm_chassis_map_file;

  // Verify the messages base_bcm_chassis_map.
  std::set<int> slots;
  std::set<int> units;
  std::set<int> modules;
  for (const auto& bcm_chip : base_bcm_chassis_map->bcm_chips()) {
    CHECK_RETURN_IF_FALSE(bcm_chip.type())
        << "Invalid type in " << bcm_chip.ShortDebugString();
    if (base_bcm_chassis_map->auto_add_slot()) {
      CHECK_RETURN_IF_FALSE(bcm_chip.slot() == 0)
          << "auto_add_slot is True and slot is non-zero for chip "
          << bcm_chip.ShortDebugString();
    } else {
      CHECK_RETURN_IF_FALSE(bcm_chip.slot() > 0)
          << "Invalid slot in " << bcm_chip.ShortDebugString();
      slots.insert(bcm_chip.slot());
    }
    CHECK_RETURN_IF_FALSE(bcm_chip.unit() >= 0 && !units.count(bcm_chip.unit()))
        << "Invalid unit in " << bcm_chip.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_chip.module() >= 0 &&
                          !modules.count(bcm_chip.module()))
        << "Invalid module in " << bcm_chip.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_chip.pci_bus() >= 0)
        << "Invalid pci_bus in " << bcm_chip.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_chip.pci_slot() >= 0)
        << "Invalid pci_slot in " << bcm_chip.ShortDebugString();
    units.insert(bcm_chip.unit());
    modules.insert(bcm_chip.module());
  }
  for (const auto& bcm_port : base_bcm_chassis_map->bcm_ports()) {
    CHECK_RETURN_IF_FALSE(bcm_port.type())
        << "Invalid type in " << bcm_port.ShortDebugString();
    if (base_bcm_chassis_map->auto_add_slot()) {
      CHECK_RETURN_IF_FALSE(bcm_port.slot() == 0)
          << "auto_add_slot is True and slot is non-zero for port "
          << bcm_port.ShortDebugString();
    } else {
      CHECK_RETURN_IF_FALSE(bcm_port.slot() > 0 && slots.count(bcm_port.slot()))
          << "Invalid slot in " << bcm_port.ShortDebugString();
    }
    CHECK_RETURN_IF_FALSE(bcm_port.port() > 0)
        << "Invalid port in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.channel() >= 0 && bcm_port.channel() <= 4)
        << "Invalid channel in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.unit() >= 0 && units.count(bcm_port.unit()))
        << "Invalid unit in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.speed_bps() > 0 &&
                          bcm_port.speed_bps() % kBitsPerGigabit == 0)
        << "Invalid speed_bps in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.physical_port() >= 0)
        << "Invalid physical_port in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.diag_port() >= 0)
        << "Invalid diag_port in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.module() >= 0 &&
                          modules.count(bcm_port.module()))
        << "Invalid module in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.serdes_core() >= 0)
        << "Invalid serdes_core in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.serdes_lane() >= 0 &&
                          bcm_port.serdes_lane() <= 3)
        << "Invalid serdes_lane in " << bcm_port.ShortDebugString();
    if (bcm_port.type() != BcmPort::MGMT) {
      CHECK_RETURN_IF_FALSE(bcm_port.num_serdes_lanes() >= 1 &&
                            bcm_port.num_serdes_lanes() <= 4)
          << "Invalid num_serdes_lanes in " << bcm_port.ShortDebugString();
    }
    CHECK_RETURN_IF_FALSE(bcm_port.tx_lane_map() >= 0)
        << "Invalid tx_lane_map in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.rx_lane_map() >= 0)
        << "Invalid rx_lane_map in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.tx_polarity_flip() >= 0)
        << "Invalid tx_polarity_flip in " << bcm_port.ShortDebugString();
    CHECK_RETURN_IF_FALSE(bcm_port.rx_polarity_flip() >= 0)
        << "Invalid rx_polarity_flip in " << bcm_port.ShortDebugString();
    if (base_bcm_chassis_map->auto_add_logical_ports() ||
        IsGePortOnTridentPlus(bcm_port, *base_bcm_chassis_map)) {
      CHECK_RETURN_IF_FALSE(bcm_port.logical_port() == 0)
          << "auto_add_logical_ports is True and logical_port is non-zero: "
          << bcm_port.ShortDebugString();
    } else {
      CHECK_RETURN_IF_FALSE(bcm_port.logical_port() > 0)
          << "auto_add_logical_ports is False and port is not a GE port, yet "
          << "logical_port is not positive: " << bcm_port.ShortDebugString();
    }
  }

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::PopulateSlotFromPushedChassisConfig(
    const ChassisConfig& config, BcmChassisMap* base_bcm_chassis_map) const {
  std::set<int> slots;
  for (const auto& node : config.nodes()) {
    slots.insert(node.slot());
  }
  for (const auto& singleton_port : config.singleton_ports()) {
    slots.insert(singleton_port.slot());
  }
  CHECK_RETURN_IF_FALSE(slots.size() == 1U)
      << "Cannot support a case where auto_add_slot is true and we have more "
      << "than one slot number specified in the ChassisConfig.";
  int slot = *slots.begin();
  for (int i = 0; i < base_bcm_chassis_map->bcm_chips_size(); ++i) {
    base_bcm_chassis_map->mutable_bcm_chips(i)->set_slot(slot);
  }
  for (int i = 0; i < base_bcm_chassis_map->bcm_ports_size(); ++i) {
    base_bcm_chassis_map->mutable_bcm_ports(i)->set_slot(slot);
  }
  VLOG(1) << "Automatically added slot " << slot << " to all the BcmChips & "
          << "BcmPorts in the base BcmChassisMap.";

  return ::util::OkStatus();
}

bool BcmChassisManager::IsSingletonPortMatchesBcmPort(
    const SingletonPort& singleton_port, const BcmPort& bcm_port) const {
  if (bcm_port.type() != BcmPort::XE && bcm_port.type() != BcmPort::CE &&
      bcm_port.type() != BcmPort::GE) {
    return false;
  }

  bool result = (singleton_port.slot() == bcm_port.slot() &&
                 singleton_port.port() == bcm_port.port() &&
                 singleton_port.channel() == bcm_port.channel() &&
                 singleton_port.speed_bps() == bcm_port.speed_bps());

  return result;
}

::util::Status BcmChassisManager::WriteBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map) const {
  ASSIGN_OR_RETURN(auto config,
                   bcm_sdk_interface_->GenerateBcmConfigFile(
                       base_bcm_chassis_map, target_bcm_chassis_map, mode_));
  return WriteStringToFile(config, FLAGS_bcm_sdk_config_file);
}

void* BcmChassisManager::LinkscanEventHandlerThreadFunc(void* arg) {
  CHECK(arg != nullptr);
  // Retrieve arguments.
  auto* args = reinterpret_cast<ReaderArgs<LinkscanEvent>*>(arg);
  auto* manager = args->manager;
  std::unique_ptr<ChannelReader<LinkscanEvent>> reader =
      std::move(args->reader);
  delete args;
  return manager->ReadLinkscanEvents(reader);
}

void* BcmChassisManager::ReadLinkscanEvents(
    const std::unique_ptr<ChannelReader<LinkscanEvent>>& reader) {
  do {
    // Check switch shutdown.
    {
      absl::ReaderMutexLock l(&chassis_lock);
      if (shutdown) break;
    }
    LinkscanEvent event;
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
    LinkscanEventHandler(event.unit, event.port, event.state);
  } while (true);
  return nullptr;
}

void BcmChassisManager::LinkscanEventHandler(int unit, int logical_port,
                                             PortState new_state) {
  absl::WriterMutexLock l(&chassis_lock);
  if (shutdown) {
    VLOG(1) << "The class is already shutdown. Exiting.";
    return;
  }

  // Update the state.
  const uint64* node_id = gtl::FindOrNull(unit_to_node_id_, unit);
  if (node_id == nullptr) {
    LOG(ERROR) << "Inconsistent state. Unit " << unit << " is not known!";
    return;
  }
  const std::map<SdkPort, uint32>* sdk_port_to_port_id =
      gtl::FindOrNull(node_id_to_sdk_port_to_port_id_, *node_id);
  if (sdk_port_to_port_id == nullptr) {
    LOG(ERROR) << "Inconsistent state. Node " << *node_id
               << " is not found as key in node_id_to_sdk_port_to_port_id_!";
    return;
  }
  SdkPort sdk_port(unit, logical_port);
  const uint32* port_id = gtl::FindOrNull(*sdk_port_to_port_id, sdk_port);
  if (port_id == nullptr) {
    LOG(WARNING)
        << "Ignored an unknown SdkPort " << sdk_port.ToString() << " on node "
        << *node_id
        << ". Most probably this is a non-configured channel of a flex port.";
    return;
  }
  node_id_to_port_id_to_port_state_[*node_id][*port_id] = new_state;

  // Notify the managers about the change of port state.
  BcmNode* bcm_node = gtl::FindPtrOrNull(unit_to_bcm_node_, unit);
  if (!bcm_node) {
    LOG(ERROR) << "Inconsistent state. BcmNode* for unit " << unit
               << " does not exist!";
    return;
  }
  auto status = bcm_node->UpdatePortState(*port_id);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to update managers on node " << *node_id
               << " on port " << *port_id << " state change to "
               << PortState_Name(new_state) << " with error: " << status << ".";
  }
  // Notify gNMI about the change of logical port state.
  SendPortOperStateGnmiEvent(*node_id, *port_id, new_state);

  // Log details about the port state change for debugging purposes.
  // TODO(unknown): The extra map lookups here are only for debugging and
  // pretty printing the ports. We may not need them. If not, simplify the
  // state reporting.
  const std::map<uint32, PortKey>* port_id_to_singleton_port_key =
      gtl::FindOrNull(node_id_to_port_id_to_singleton_port_key_, *node_id);
  if (port_id_to_singleton_port_key == nullptr) {
    LOG(ERROR)
        << "Inconsistent state. Node " << *node_id
        << " is not found as key in node_id_to_port_id_to_singleton_port_key_!";
    return;
  }
  const PortKey* singleton_port_key =
      gtl::FindOrNull(*port_id_to_singleton_port_key, *port_id);
  if (singleton_port_key == nullptr) {
    LOG(ERROR) << "Inconsistent state. No PortKey for port " << *port_id
               << " on node " << *node_id << ".";
    return;
  }
  const BcmPort* bcm_port =
      gtl::FindPtrOrNull(singleton_port_key_to_bcm_port_, *singleton_port_key);
  if (bcm_port == nullptr) {
    LOG(ERROR) << "Inconsistent state. " << singleton_port_key->ToString()
               << " is not found as key in singleton_port_key_to_bcm_port_!";
    return;
  }

  LOG(INFO) << "State of SingletonPort "
            << PrintPortProperties(*node_id, *port_id, bcm_port->slot(),
                                   bcm_port->port(), bcm_port->channel(), unit,
                                   logical_port, bcm_port->speed_bps())
            << ": " << PrintPortState(new_state);
}

void BcmChassisManager::SendPortOperStateGnmiEvent(uint64 node_id,
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

void* BcmChassisManager::TransceiverEventHandlerThreadFunc(void* arg) {
  CHECK(arg != nullptr);
  // Retrieve arguments.
  auto* args = reinterpret_cast<ReaderArgs<TransceiverEvent>*>(arg);
  auto* manager = args->manager;
  std::unique_ptr<ChannelReader<TransceiverEvent>> reader =
      std::move(args->reader);
  delete args;
  return manager->ReadTransceiverEvents(reader);
}

void* BcmChassisManager::ReadTransceiverEvents(
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
  return nullptr;
}

void BcmChassisManager::TransceiverEventHandler(int slot, int port,
                                                HwState new_state) {
  absl::WriterMutexLock l(&chassis_lock);
  if (shutdown) {
    VLOG(1) << "The class is already shutdown. Exiting.";
    return;
  }

  PortKey xcvr_port_key(slot, port);
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
  // the transceiver modules. Other values do not make sense.
  if (new_state != HW_STATE_PRESENT && new_state != HW_STATE_NOT_PRESENT) {
    LOG(ERROR) << "Invalid state for transceiver " << xcvr_port_key.ToString()
               << " in TransceiverEventHandler: " << HwState_Name(new_state)
               << ".";
    return;
  }

  // Discard some invalid situations and report the error. Then save the new
  // state
  if (old_state == HW_STATE_READY && new_state == HW_STATE_PRESENT) {
    if (!IsInternalPort(xcvr_port_key)) {
      LOG(ERROR) << "Got present for a ready transceiver "
                 << xcvr_port_key.ToString() << " in TransceiverEventHandler.";
    } else {
      VLOG(1) << "Got present for a internal (e.g. BP) transceiver "
              << xcvr_port_key.ToString() << " in TransceiverEventHandler.";
    }
    return;
  }
  if (old_state == HW_STATE_UNKNOWN && new_state == HW_STATE_NOT_PRESENT) {
    LOG(ERROR) << "Got not-present for an unknown transceiver "
               << xcvr_port_key.ToString() << " in TransceiverEventHandler.";
    return;
  }
  *mutable_state = new_state;

  // Set the port options based on new_state.
  BcmPortOptions options;
  options.set_enabled(new_state == HW_STATE_PRESENT ? TRI_STATE_TRUE
                                                    : TRI_STATE_FALSE);
  if (old_state == HW_STATE_UNKNOWN) {
    // First time we are seeing this transceiver module. Need to set the block
    // state too. Otherwise, we do not touch the blocked state.
    options.set_blocked(TRI_STATE_FALSE);
  }
  ::util::Status status = SetPortOptionsForPortGroup(xcvr_port_key, options);
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

::util::StatusOr<bool> BcmChassisManager::SetSpeedForFlexPortGroup(
    const PortKey& port_group_key) const {
  // First check to see if this is a flex port group.
  const std::vector<BcmPort*>* bcm_ports =
      gtl::FindOrNull(port_group_key_to_flex_bcm_ports_, port_group_key);
  CHECK_RETURN_IF_FALSE(bcm_ports != nullptr)
      << "Ports with (slot, port) = (" << port_group_key.slot << ", "
      << port_group_key.port << ") is not a flex port.";

  // Find info on this flex port group.
  std::set<int> units_set;
  std::set<int> min_speed_logical_ports_set;
  std::set<int> config_speed_logical_ports_set;
  std::set<int> config_num_serdes_lanes_set;
  std::set<uint64> config_speed_bps_set;
  for (const auto& bcm_port : applied_bcm_chassis_map_->bcm_ports()) {
    if (bcm_port.slot() == port_group_key.slot &&
        bcm_port.port() == port_group_key.port) {
      CHECK_RETURN_IF_FALSE(bcm_port.flex_port())
          << "Detected unexpected non-flex SingletonPort: "
          << PrintBcmPort(bcm_port);
      units_set.insert(bcm_port.unit());
      min_speed_logical_ports_set.insert(bcm_port.logical_port());
    }
  }
  for (const auto* bcm_port : *bcm_ports) {
    units_set.insert(bcm_port->unit());
    config_speed_logical_ports_set.insert(bcm_port->logical_port());
    config_num_serdes_lanes_set.insert(bcm_port->num_serdes_lanes());
    config_speed_bps_set.insert(bcm_port->speed_bps());
  }

  // Check to see everythin makes sense.
  CHECK_RETURN_IF_FALSE(units_set.size() == 1U)
      << "Found ports with (slot, port) = (" << port_group_key.slot << ", "
      << port_group_key.port << ") are on different chips.";
  CHECK_RETURN_IF_FALSE(config_num_serdes_lanes_set.size() == 1U)
      << "Found ports with (slot, port) = (" << port_group_key.slot << ", "
      << port_group_key.port << ") have different num_serdes_lanes.";
  CHECK_RETURN_IF_FALSE(config_speed_bps_set.size() == 1U)
      << "Found ports with (slot, port) = (" << port_group_key.slot << ", "
      << port_group_key.port << ") have different speed_bps.";
  int unit = *units_set.begin();
  int control_logical_port = *min_speed_logical_ports_set.begin();
  int config_num_serdes_lanes = *config_num_serdes_lanes_set.begin();
  uint64 config_speed_bps = *config_speed_bps_set.begin();
  CHECK_RETURN_IF_FALSE(*config_speed_logical_ports_set.begin() ==
                        control_logical_port)
      << "Control logical port mismatch: " << control_logical_port
      << " != " << *config_speed_logical_ports_set.begin() << ".";

  // Now try to get the current speed_bps from the control port
  BcmPortOptions options;
  RETURN_IF_ERROR(
      bcm_sdk_interface_->GetPortOptions(unit, control_logical_port, &options));

  // If no change in the speed, nothing to do. Just return. There will be no
  // serdes setting either.
  if (options.speed_bps() == config_speed_bps) {
    return false;
  }

  // Now that Fist disable all the channelized ports of the min speed.
  options.Clear();
  options.set_enabled(TRI_STATE_FALSE);
  options.set_blocked(TRI_STATE_TRUE);
  for (const int logical_port : min_speed_logical_ports_set) {
    RETURN_IF_ERROR(
        bcm_sdk_interface_->SetPortOptions(unit, logical_port, options));
  }

  // Now set the number of serdes lanes just for control logical ports.
  options.Clear();
  options.set_num_serdes_lanes(config_num_serdes_lanes);
  RETURN_IF_ERROR(
      bcm_sdk_interface_->SetPortOptions(unit, control_logical_port, options));

  // Finally, set the speed_bps. Note that we do not enable/unblock the port
  // now, this will be done later in SetPortOptionsForPortGroup() called
  // in ConfigurePortGroups().
  options.Clear();
  options.set_speed_bps(config_speed_bps);
  for (const int logical_port : config_speed_logical_ports_set) {
    RETURN_IF_ERROR(
        bcm_sdk_interface_->SetPortOptions(unit, logical_port, options));
  }

  LOG(INFO) << "Successfully set speed for flex port group "
            << port_group_key.ToString() << " to "
            << config_speed_bps / kBitsPerGigabit << "G.";

  return true;
}

::util::Status BcmChassisManager::SetPortOptionsForPortGroup(
    const PortKey& port_group_key, const BcmPortOptions& options) const {
  std::vector<BcmPort*> bcm_ports = {};
  if (port_group_key_to_flex_bcm_ports_.count(port_group_key)) {
    bcm_ports = port_group_key_to_flex_bcm_ports_.at(port_group_key);
  } else if (port_group_key_to_non_flex_bcm_ports_.count(port_group_key)) {
    bcm_ports = port_group_key_to_non_flex_bcm_ports_.at(port_group_key);
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unknown port group " << port_group_key.ToString() << ".";
  }

  if (options.enabled() == TRI_STATE_TRUE &&
      mode_ == OPERATION_MODE_STANDALONE) {
    // We need to configure serdes for this port now. We reach to this point
    // in the following situations:
    // 1- When push config for the first time and there are some BP ports, we
    //    immediately set the serdes settings for these ports here.
    // 2- When we receive a presence detect signal for a front panel port (
    //    after stack comes up for the first time or after transceiver modules
    //    are inserted).
    // 3- When a config push changes the speed for a flex port.
    // We first get the front panel port info from PHAL. Then using this info
    // (read and parsed from the transceiver module EEPROM) we configure serdes
    // for all BCM ports.
    FrontPanelPortInfo fp_port_info;
    RETURN_IF_ERROR(phal_interface_->GetFrontPanelPortInfo(
        port_group_key.slot, port_group_key.port, &fp_port_info));
    for (const auto* bcm_port : bcm_ports) {
      // Get the serdes config from serdes db for the given BCM port.
      BcmSerdesLaneConfig bcm_serdes_lane_config;
      if (bcm_serdes_db_manager_
              ->LookupSerdesConfigForPort(*bcm_port, fp_port_info,
                                          &bcm_serdes_lane_config)
              .ok()) {
        // Find the map from serdes register names to their values for this BCM
        // port.
        std::map<uint32, uint32> serdes_register_configs(
            bcm_serdes_lane_config.bcm_serdes_register_configs().begin(),
            bcm_serdes_lane_config.bcm_serdes_register_configs().end());
        std::map<std::string, uint32> serdes_attr_configs(
            bcm_serdes_lane_config.bcm_serdes_attribute_configs().begin(),
            bcm_serdes_lane_config.bcm_serdes_attribute_configs().end());
        // Config serdes for this BCM port.
        RETURN_IF_ERROR(bcm_sdk_interface_->ConfigSerdesForPort(
            bcm_port->unit(), bcm_port->logical_port(), bcm_port->speed_bps(),
            bcm_port->serdes_core(), bcm_port->serdes_lane(),
            bcm_port->num_serdes_lanes(), bcm_serdes_lane_config.intf_type(),
            serdes_register_configs, serdes_attr_configs));
        // TODO(unknown): For some transceivers (e.g. 100G cSR4 QSFPs) we also
        // need to write some control values to the QSFP module control
        // registers. Take care of that part too.
        VLOG(1) << "Serdes setting done for SingletonPort "
                << PrintBcmPort(*bcm_port) << ".";
      } else {
        LOG(WARNING) << "No SerDes setting found for SingletonPort "
                     << PrintBcmPort(*bcm_port) << ".";
      }
    }
  }
  // The option applies to all the ports.
  for (const auto* bcm_port : bcm_ports) {
    BcmPortOptions applied_options = options;
    // Check if AdminState is set and override options.
    auto* node_id = gtl::FindOrNull(unit_to_node_id_, bcm_port->unit());
    CHECK_RETURN_IF_FALSE(node_id)
        << "Unable to find unit " << bcm_port->unit() << ".";
    auto* sdk_port_to_port_id =
        gtl::FindOrNull(node_id_to_sdk_port_to_port_id_, *node_id);
    CHECK_RETURN_IF_FALSE(sdk_port_to_port_id)
        << "Unable to find node " << *node_id << ".";
    SdkPort sdk_port(bcm_port->unit(), bcm_port->logical_port());
    auto* port_id = gtl::FindOrNull(*sdk_port_to_port_id, sdk_port);
    CHECK_RETURN_IF_FALSE(port_id)
        << "Unable to find SdkPort " << sdk_port.ToString() << ".";
    const auto* port_id_to_admin_state =
        gtl::FindOrNull(node_id_to_port_id_to_admin_state_, *node_id);
    CHECK_RETURN_IF_FALSE(port_id_to_admin_state != nullptr)
        << "Unknown node " << node_id << ".";
    const auto* admin_state =
        gtl::FindOrNull(*port_id_to_admin_state, *port_id);
    CHECK_RETURN_IF_FALSE(admin_state != nullptr)
        << "Unknown port " << port_id << " on node " << node_id << ".";
    if (*admin_state == ADMIN_STATE_DISABLED) {
      applied_options.set_enabled(TRI_STATE_FALSE);
      applied_options.set_blocked(TRI_STATE_TRUE);
    } else if (*admin_state == ADMIN_STATE_ENABLED) {
      applied_options.set_enabled(TRI_STATE_TRUE);
      applied_options.set_blocked(TRI_STATE_FALSE);
    }

    RETURN_IF_ERROR(bcm_sdk_interface_->SetPortOptions(
        bcm_port->unit(), bcm_port->logical_port(), applied_options));
    VLOG(1) << "Successfully set the following options for SingletonPort "
            << PrintBcmPort(*bcm_port) << ": "
            << PrintBcmPortOptions(applied_options);
  }

  return ::util::OkStatus();
}

bool BcmChassisManager::IsInternalPort(const PortKey& port_key) const {
  // Note that we have alreay verified that all the port that are part of a
  // flex/non-flex port groups are all internal or non internal. So we need to
  // check one port only.
  const std::vector<BcmPort*>* non_flex_ports =
      gtl::FindOrNull(port_group_key_to_non_flex_bcm_ports_, port_key);
  if (non_flex_ports != nullptr && !non_flex_ports->empty()) {
    return non_flex_ports->front()->internal();
  }
  const std::vector<BcmPort*>* flex_ports =
      gtl::FindOrNull(port_group_key_to_flex_bcm_ports_, port_key);
  if (flex_ports != nullptr && !flex_ports->empty()) {
    return flex_ports->front()->internal();
  }
  return false;
}

::util::Status BcmChassisManager::EnablePort(const SdkPort& sdk_port,
                                             bool enable) const {
  BcmPortOptions options;
  options.set_enabled(enable ? TRI_STATE_TRUE : TRI_STATE_FALSE);
  RETURN_IF_ERROR(bcm_sdk_interface_->SetPortOptions(
        sdk_port.unit, sdk_port.logical_port, options));

  return ::util::OkStatus();
}

::util::Status BcmChassisManager::LoopbackPort(const SdkPort& sdk_port,
                                               LoopbackState state) const {
  if (state == LoopbackState::LOOPBACK_STATE_UNKNOWN) {
    return ::util::OkStatus();
  }
  BcmPortOptions options;
  options.set_loopback_mode(state);
  RETURN_IF_ERROR(bcm_sdk_interface_->SetPortOptions(
      sdk_port.unit, sdk_port.logical_port, options));

  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
