/*
 * Copyright 2018 Google LLC
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_chassis_ro_interface.h"
#include "stratum/hal/lib/bcm/bcm_global_vars.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "stratum/hal/lib/bcm/bcm_node.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/gnmi_events.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/common/writer_interface.h"
#include "stratum/lib/channel/channel.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// The "BcmChassisManager" class encapsulates all the chassis-related
// functionalities needed in BcmSwitch class. This class is in charge of
// maintaining and updating all the port/node/chassis related datastructures, as
// well as all the one-time initializations of the platform and switching ASICs.
// NOTE: The maps in this class may be accessed in such a way where the order of
// the keys are important. That is why we chose to use std::map as opposed to
// std::unordered_map or absl::flat_hash_map and accept a little bit of
// performance hit when doing lookup.
class BcmChassisManager : public BcmChassisRoInterface {
 public:
  // Encapsulates trunk membership info of a singleton port that are part of a
  // trunk.
  struct TrunkMembershipInfo {
    TrunkMembershipInfo()
        : parent_trunk_id(0), block_state(TRUNK_MEMBER_BLOCK_STATE_UNKNOWN) {}
    // Parent trunk ID.
    uint32 parent_trunk_id;
    // The last block state set for this trunk member.
    TrunkMemberBlockState block_state;
  };

  virtual ~BcmChassisManager();

  // Pushes the chassis config. If the class is not initialized, this function
  // calls Initialize() to initialize the SDK and attach all the units. Then it
  // applies the parts of the ChassisConfig proto that do not need SDK
  // re-initialization. Overall this function performs the following:
  // 1- Initializes the SDK and attaches all the unit if initialized_ is false.
  // 2- Creates the internal port maps.
  // 3- Starts the link scan.
  // 4- Sets up the port options for the flex and non-flex ports.
  // 5- Saves or updates an internal copy of the ChassisConfig proto which has
  //    the most updated configuration of all the chassis/nodes/ports.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Verifies the part of ChassisConfig proto that this class cares about:
  // 1- Calls GenerateBcmChassisMapFromConfig() to make sure we can generate
  //    bcm_chassis_map based on the pushed ChassisConfig proto, which itself
  //    performs all the validations.
  // 2- If the class is initialized, it also makes sure the resulting
  //    bcm_chassis_map matches the applied_bcm_chassis_map_, otherwise return
  //    'reboot required'.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config)
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Performs coldboot shutdown sequence (detaching all attached unit and
  // clearing the maps). Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized when we push the chassis config.
  virtual ::util::Status Shutdown() LOCKS_EXCLUDED(chassis_lock);

  // Initializes the unit -> BcmNode* map. This is not part of the constructor
  // as a pointer to the BcmChassisManager instance is given to all of the
  // BcmNode instances on creation.
  virtual void SetUnitToBcmNodeMap(
      const std::map<int, BcmNode*>& unit_to_bcm_node)
      LOCKS_EXCLUDED(chassis_lock);

  // Registers a WriterInterface<GnmiEventPtr> for sending gNMI events.
  virtual ::util::Status RegisterEventNotifyWriter(
      std::shared_ptr<WriterInterface<GnmiEventPtr>> writer)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  // Unregisters a previously registered WriterInterface<GnmiEventPtr>.
  virtual ::util::Status UnregisterEventNotifyWriter()
      LOCKS_EXCLUDED(gnmi_event_lock_);

  // BcmChassisRoInterface functions.
  ::util::StatusOr<BcmChip> GetBcmChip(int unit) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<BcmPort> GetBcmPort(int slot, int port,
                                       int channel) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<BcmPort> GetBcmPort(uint64 node_id,
                                       uint32 port_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<std::map<uint64, int>> GetNodeIdToUnitMap() const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<std::map<uint32, SdkPort>> GetPortIdToSdkPortMap(
      uint64 node_id) const override SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<std::map<uint32, SdkTrunk>> GetTrunkIdToSdkTrunkMap(
      uint64 node_id) const override SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<PortState> GetPortState(uint64 node_id,
                                           uint32 port_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<PortState> GetPortState(const SdkPort& sdk_port)
      const override SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<TrunkState> GetTrunkState(uint64 node_id,
                                             uint32 trunk_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<std::set<uint32>> GetTrunkMembers(
      uint64 node_id, uint32 trunk_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<uint32> GetParentTrunkId(uint64 node_id,
                                            uint32 port_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<AdminState> GetPortAdminState(uint64 node_id,
                                                 uint32 port_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::StatusOr<LoopbackState> GetPortLoopbackState(
      uint64 node_id, uint32 port_id) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);
  ::util::Status GetPortCounters(uint64 node_id, uint32 port_id,
                                 PortCounters* pc) const override
      SHARED_LOCKS_REQUIRED(chassis_lock);

  // Sets the block state of a trunk member on a node specified by node_id. The
  // id of the member is given by port_id. The ID of the trunk which the port is
  // part of is also given by trunk_id. Note that trunk_id input arg is
  // optional. If trunk_id is given, we make sure the parent trunk ID for the
  // given port_id matches value. The function:
  // 1- Sends a request to SDK to (un)block the given port, if and only if it
  //    is part of a trunk.
  // 2- Mutates node_id_to_port_id_to_trunk_membership_info_ for the given pair
  //    of (node_id, port_id) and adjust the blocking state of the trunk member,
  //    only if step 1 is successful.
  // This function is expected to return error in the following cases:
  // 1- If (node_id, port_id) is not found as a key in
  //    node_id_to_port_id_to_trunk_membership_info_.
  // 2- If the given trunk_id does not match the parent trunk ID found in
  //    node_id_to_port_id_to_trunk_membership_info_ for the port.
  // 3- If SDK operation fails.
  virtual ::util::Status SetTrunkMemberBlockState(uint64 node_id,
                                                  uint32 trunk_id,
                                                  uint32 port_id,
                                                  TrunkMemberBlockState state)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Sets the admin state of a port, as requested by the SDN controller. This
  // method:
  // 1- Sends a request to SDK to enable/disable the port.
  // 2- Mutates node_id_to_port_id_to_admin_state_ for the given pair of
  //    (node_id, port_id), only if step 1 is successful.
  virtual ::util::Status SetPortAdminState(uint64 node_id, uint32 port_id,
                                           AdminState state)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Sets the health state of a port, as requested by the SDN controller. This
  // method:
  // 1- Mutates node_id_to_port_id_to_health_state_ for the given pair of
  //    (node_id, port_id).
  // 2- Translates the requested health state to a (LED color, LED state)
  //    for the corresponding (slot, port, channel) and sends it to PHAL to
  //    change the LED color/state. Note that we mutate the internal state even
  //    if setting LED color/state fails.
  virtual ::util::Status SetPortHealthState(uint64 node_id, uint32 port_id,
                                            HealthState state)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Sets the loopback state of a port, as requested by the SDN controller.
  virtual ::util::Status SetPortLoopbackState(uint64 node_id, uint32 port_id,
                                              LoopbackState state)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmChassisManager> CreateInstance(
      OperationMode mode, PhalInterface* phal_interface,
      BcmSdkInterface* bcm_sdk_interface,
      BcmSerdesDbManager* bcm_serdes_db_manager);

  // BcmChassisManager is neither copyable nor movable.
  BcmChassisManager(const BcmChassisManager&) = delete;
  BcmChassisManager& operator=(const BcmChassisManager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmChassisManager();

 private:
  // ReaderArgs encapsulates the arguments for a Channel reader thread.
  template <typename T>
  struct ReaderArgs {
    BcmChassisManager* manager;
    std::unique_ptr<ChannelReader<T>> reader;
  };

  static constexpr int kTridentPlusMaxBcmPortsPerChip = 64;
  static constexpr int kTridentPlusMaxBcmPortsInXPipeline = 32;
  static constexpr int kTrident2MaxBcmPortsPerChip = 104;
  static constexpr int kTomahawkMaxBcmPortsPerChip = 128;
  // Maximum depth of transceiver event Channel.
  static constexpr int kMaxXcvrEventDepth = 256;
  // Maximum depth of linkscan event channel.
  static constexpr int kMaxLinkscanEventDepth = 256;

  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmChassisManager(OperationMode mode, PhalInterface* phal_interface,
                    BcmSdkInterface* bcm_sdk_interface,
                    BcmSerdesDbManager* bcm_serdes_db_manager);

  // Generates both base_bcm_chassis_map and target_bcm_chassis_map given a
  // ChassisConfig proto. target_bcm_chassis_map will be a pruned version of
  // base_bcm_chassis_map where:
  // 1- The ports that are not part of the chassis config are all removed.
  // 2- The ports that are part of the chassis config and are not set as flex
  //    in the base_bcm_chassis_map exist in this map and we specifiy their
  //    speed, channels, etc. Such ports cannot be changed later using a chassis
  //    config push.
  // 3- The ports that are part of the chassis config and are set as flex in the
  //    base_bcm_chassis_map exist in this map as well. These ports appear as
  //    fully channelized ports in the bcm_chassis_map. The exact speed and
  //    channels for these ports are specified later when the chassis config is
  //    pushed.
  //    This helper is called in FillBcmInitConfig() of BcmSwitch class.
  // Note that this method MUST NOT change any state of the class, which is why
  // it is declared const.
  ::util::Status GenerateBcmChassisMapFromConfig(
      const ChassisConfig& config, BcmChassisMap* base_bcm_chassis_map,
      BcmChassisMap* target_bcm_chassis_map) const;

  // One time coldboot initialization of all BCM chips. Initializes the SDK
  // and attaches to all the units.
  ::util::Status InitializeBcmChips(
      const BcmChassisMap& base_bcm_chassis_map,
      const BcmChassisMap& target_bcm_chassis_map);

  // One time initialization of the internal state. Need to be called after
  // InitializeBcmChips() completes successfully.
  ::util::Status InitializeInternalState(
      const BcmChassisMap& base_bcm_chassis_map,
      const BcmChassisMap& target_bcm_chassis_map);

  // (Re-)syncs the internal state based the pushed chassis config. Called as
  // part of each chassis config push to regenerate all the internal port maps.
  ::util::Status SyncInternalState(const ChassisConfig& config)
      EXCLUSIVE_LOCKS_REQUIRED(chassis_lock);

  // Registers/Unregisters all the event Writers (if not done yet).
  ::util::Status RegisterEventWriters();
  ::util::Status UnregisterEventWriters();

  // Configures all the flex and non-flex port groups. This method is called
  // as part of each config:
  // 1- Sets the speed for the flex ports if we detect a speed change based on
  //    the pushed chassis config.
  // 2- Set the port options for the all the flex and non-flex ports based on
  //    the pushed chassis config.
  ::util::Status ConfigurePortGroups();

  // Cleans up the internal state. Resets all the internal port maps and
  // deletes the pointers.
  void CleanupInternalState();

  // Loads the base_bcm_chassis_map from a file. We read the list of supported
  // profiles from a file, picks the one whose ID matches bcm_chassis_map_id
  // or the first profile if bcm_chassis_map_id is empty.
  ::util::Status ReadBaseBcmChassisMapFromFile(
      const std::string& bcm_chassis_map_id,
      BcmChassisMap* base_bcm_chassis_map) const;

  // Reads the given ChassisConfig and populates the slot field for all the
  // BcmPorts and BcmChips in the given BcmChassisMap based on that. Note that
  // ChassisConfig can only have one slot number for all the nodes and
  // singleton ports. Otherwise the function will return error.
  ::util::Status PopulateSlotFromPushedChassisConfig(
      const ChassisConfig& config, BcmChassisMap* base_bcm_chassis_map) const;

  // Helper function that returns true of a given SingletonPort matches
  // a given BcmPort.
  bool IsSingletonPortMatchesBcmPort(const SingletonPort& singleton_port,
                                     const BcmPort& bcm_port) const;

  // Generates the config.bcm file given the base_bcm_chassis_map and
  // target_bcm_chassis_map writes it to the path given by
  // FLAGS_bcm_config_file.
  ::util::Status WriteBcmConfigFile(
      const BcmChassisMap& base_bcm_chassis_map,
      const BcmChassisMap& target_bcm_chassis_map) const;

  // Linkscan event handler. This method is executed by a ChannelReader thread
  // which processes SDK linkscan events. Port is the logical port number used
  // by the SDK.
  // NOTE: This method should never be executed directly from a context which
  // first accesses the internal structures of a class below BcmChassisManager
  // as this may result in deadlock.
  void LinkscanEventHandler(int unit, int logical_port, PortState new_state)
      LOCKS_EXCLUDED(chassis_lock);

  // Transceiver module insert/removal event handler. This method is executed by
  // a ChannelReader thread which processes transceiver module insert/removal
  // events. Port is the 1-based frontpanel port number.
  // NOTE: This method should never be executed directly from a context which
  // first accesses the internal structures of a class below BcmChassisManager
  // as this may result in deadlock.
  void TransceiverEventHandler(int slot, int port, HwState new_state)
      LOCKS_EXCLUDED(chassis_lock);

  // Thread function for reading transceiver events from xcvr_event_channel_.
  // Invoked with "this" as the argument in pthread_create.
  static void* TransceiverEventHandlerThreadFunc(void* arg)
      LOCKS_EXCLUDED(chassis_lock, gnmi_event_lock_);

  // Reads and processes transceiver events using the given ChannelReader.
  // Called by TransceiverEventHandlerThreadFunc.
  void* ReadTransceiverEvents(
      const std::unique_ptr<ChannelReader<PhalInterface::TransceiverEvent>>&
          reader) LOCKS_EXCLUDED(chassis_lock, gnmi_event_lock_);

  // Thread function for reading linkscan events from linkscan_event_channel_.
  // Invoked with "this" as the argument in pthread_create.
  static void* LinkscanEventHandlerThreadFunc(void* arg)
      LOCKS_EXCLUDED(chassis_lock);

  // Reads and processes linkscan events using the given ChannelReader. Called
  // by LinkscanEventHandlerThreadFunc.
  void* ReadLinkscanEvents(
      const std::unique_ptr<ChannelReader<BcmSdkInterface::LinkscanEvent>>&
          reader) LOCKS_EXCLUDED(chassis_lock);

  // Forward PortStatus changed events through the appropriate node's registered
  // ChannelWriter<GnmiEventPtr> object. Called by LinkscanEventHandler and
  // expects chassis_lock to be held.
  void SendPortOperStateGnmiEvent(uint64 node_id, uint32 port_id,
                                  PortState new_state)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(gnmi_event_lock_);

  // Sets the speed for a flex port group after a chassis config is pushed. The
  // input is a PortKey encapsulating (slot, port) of the port group. The
  // function determines if there is a change in the speed based on the pushed
  // chassis config. If so, it configures the speed for all the ports in the
  // given port group.
  ::util::StatusOr<bool> SetSpeedForFlexPortGroup(
      const PortKey& port_group_key) const;

  // Sets port options for ports in a flex or non-flex port group. The inputs
  // are a PortKey encapsulating (slot, port) of the port group and a
  // BcmPortOptions message determining the options that need to be applied to
  // all the ports in the port group.
  ::util::Status SetPortOptionsForPortGroup(
      const PortKey& port_group_key, const BcmPortOptions& options) const;

  // A boolean which determines whether a (slot, port) encapsulated in a
  // PortKey belongs to an internal port (e.g. BP port in BG or SPICA).
  bool IsInternalPort(const PortKey& port_key) const;

  // A helper method to enable/disable a port by calling SDK. The unit and
  // logical_port number for the port are given through an SdkPort object.
  ::util::Status EnablePort(const SdkPort& sdk_port, bool enable) const;

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Stratum stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Stratum stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Stratum stack runs in simulation mode.
  // Note that this variable is set upon initialization and is never changed
  // afterwards.
  OperationMode mode_;

  // Determines if the manager has been initialized (coldboot or warmboot).
  bool initialized_;

  // The id of the link scan event ChannelWriter, as returned by
  // BcmSdkInterface::RegisterLinkscanEventWriter(). Used to remove the Writer.
  int linkscan_event_writer_id_;

  // The id of the transceiver module insert/removal event ChannelWriter, as
  // returned by PhalInterface::RegisterTransceiverEventChannelWriter(). Used to
  // remove the handler later if needed.
  int xcvr_event_writer_id_;

  // base_bcm_chassis_map_ includes all the possible slot, port, channel, and
  // speed_bps for all the front panel ports. This map is fixed for any chassis,
  // and is read from a file in ReadBaseBcmChassisMapFromFile().
  std::unique_ptr<BcmChassisMap> base_bcm_chassis_map_;

  // applied_bcm_chassis_map_ is a pruned and possibly modified version of
  // base_bcm_chassis_map_ created as part of initial config push. This is
  // fixed and will not change while the stack is up. As part of config verify
  // in VerifyChassisConfig(), we make sure if the class is initialized, the
  // target_bcm_chassis_map found based on the config stays the same as
  // applied_bcm_chassis_map_ or we report "reboot required".
  std::unique_ptr<BcmChassisMap> applied_bcm_chassis_map_;

  // Map from 0-based unit to a pointer to its corresponding BcmChip.
  std::map<int, BcmChip*> unit_to_bcm_chip_;

  // Map from PortKey representing (slot, port, channel) of a singleton port
  // to a pointer to its corresponding BcmPort. This map is updated as part of
  // each config push. For each (slot, port, channel), the value follows the
  // these rules:
  // 1- For non-flex ports, this is a pointer to a BcmPort which is a copy of
  //    the corresponding entry in bcm_chassis_map_.
  // 2- For flex ports, this is a pointer to a BcmPort which is matching the
  //    config and may change after each chassis config push.
  std::map<PortKey, BcmPort*> singleton_port_key_to_bcm_port_;

  // Map from PortKey representing (slot, port) of a port group to a vector of
  // BcmPort pointers corresponding to all the channels of the singleton ports
  // in the group. Port group in this class refers to a group of ports that
  // share the same (slot, port), which can be flex or non-flex. We keep two
  // maps, one for flex and one for non-flex ports. Note that the data in these
  // maps is also available in singleton_port_key_to_bcm_port_. These maps are
  // kept for faster and easier access to the BcmPorts given the (slot, port)
  // of a port group, and are updated as part of each config push.
  std::map<PortKey, std::vector<BcmPort*>> port_group_key_to_flex_bcm_ports_;
  std::map<PortKey, std::vector<BcmPort*>>
      port_group_key_to_non_flex_bcm_ports_;

  // Map from node ID to unit number. This map is updated as part of each config
  // push.
  std::map<uint64, int> node_id_to_unit_;

  // Map from unit number to node ID. This map is updated as part of each config
  // push.
  std::map<int, uint64> unit_to_node_id_;

  // Map from node ID to the set of port IDs corresponding to the singleton
  // ports that belong to that node. This map is updated as part of each config
  // push.
  std::map<uint64, std::set<uint32>> node_id_to_port_ids_;

  // Map from node ID to the set of trunk IDs corresponding to the trunks that
  // belong to that node. This map is updated as part of each config push.
  std::map<uint64, std::set<uint32>> node_id_to_trunk_ids_;

  // Map from node ID to another map from port ID to PortKey corresponding
  // to the singleton port uniquely identified by (node ID, port ID). This map
  // is updated as part of each config push.
  std::map<uint64, std::map<uint32, PortKey>>
      node_id_to_port_id_to_singleton_port_key_;

  // Map from node ID to another map from port ID to SdkPort encapsulating the
  // (unit, logical_port) of the singleton port uniquely identified by
  // (node ID, port ID). This map is updated as part of each config push.
  std::map<uint64, std::map<uint32, SdkPort>> node_id_to_port_id_to_sdk_port_;

  // Map from node ID to another map from trunk ID to SdkTrunk representing
  // (unit, trunk_port) of the trunk uniquely identified by (node ID, trunk ID).
  // This map is updated as part of each config push, as part of which we may
  // add/delete trunks in hardware.
  std::map<uint64, std::map<uint32, SdkTrunk>>
      node_id_to_trunk_id_to_sdk_trunk_;

  // Map from node ID to another map from SdkPort to port ID, with
  // SdkPort representing (unit, logical_port) of the singleton port
  // uniquely identified by (node ID, port ID). This map is updated as part of
  // each config push.
  std::map<uint64, std::map<SdkPort, uint32>> node_id_to_sdk_port_to_port_id_;

  // Map from node ID to another map from SdkTrunk to trunk ID, with
  // SdkTrunk representing (unit, trunk_port) of the trunk uniquely
  // identified by (node ID, trunk ID). This map is updated as part of
  // each config push, as part of which we may add/delete trunks in hardware.
  std::map<uint64, std::map<SdkTrunk, uint32>>
      node_id_to_sdk_trunk_to_trunk_id_;

  // Map from PortKey representing (slot, port) of a transceiver port to the
  // state of the transceiver module plugged into that (slot, port).
  std::map<PortKey, HwState> xcvr_port_key_to_xcvr_state_;

  // Map from node ID to another map from port ID to PortState representing
  // the state of the singleton port uniquely identified by (node ID, port ID).
  // After chassis config push, if there is already a state for a port in this
  // map, we keep the state, otherwise we initialize the state to
  // PORT_STATE_UNKNOWN and let the next linkscan event update the state.
  std::map<uint64, std::map<uint32, PortState>>
      node_id_to_port_id_to_port_state_;

  // Map from node ID to another map from trunk ID to TrunkState representing
  // the state of the trunk port uniquely identified by (node ID, trunk ID).
  // This map is updated as part of any "trunk event". After each config push
  // for simplicity we set the state of each trunk to to
  std::map<uint64, std::map<uint32, TrunkState>>
      node_id_to_trunk_id_to_trunk_state_;

  // Map from node ID to another map from trunk ID to set of port IDs
  // corresponding to singleton ports that are memebers of the trunk. The
  // members of a trunk are initialized based on the first config and then
  // updated as part of each "trunk event". As part of any subsequent config
  // push, we may add/remove members.
  std::map<uint64, std::map<uint32, std::set<uint32>>>
      node_id_to_trunk_id_to_members_;

  // Map from node ID to another map from port ID to its trunk membership info,
  // when the port is part of a trunk. Trunk membership of the ports is updated
  // as part of each "trunk event".
  // TODO(unknown): The assumption here is that each port can be part of one
  // trunk only. If this assumption is not correct, change the map.
  std::map<uint64, std::map<uint32, TrunkMembershipInfo>>
      node_id_to_port_id_to_trunk_membership_info_;

  // Map from node ID to another map from port ID to AdminState representing
  // the admin state of the port as set by the SDN controller or the config.
  // This map is updated as part of each config push or per request from the
  // SDN controller by calling SetPortAdminState(). After each chassis config
  // push, we honor the valid admin state of the ports specified in the config.
  // If no admin state is specified for a port in the config, we either keep the
  // state (if there is already a state for the port in this map), or initialize
  // the state to ADMIN_STATE_UNKNOWN (if the port is not found as a key in the
  // map). A port with admin state ADMIN_STATE_UNKNOWN will use the default
  // admin state and will not be forcefully diasbled in HW. That means the port
  // can later come up (i.e., its oper state can be up).
  std::map<uint64, std::map<uint32, AdminState>>
      node_id_to_port_id_to_admin_state_;

  // Map from node ID to another map from port ID to the health state of the
  // port, encoded by an enum of type HealthState. The health state of a port
  // is usually updated by a remote SDN controller. Any change to this map will
  // be translated to a change into the color/state of the frontpanel port LED
  // corresponding to the (slot, port, channel) that this port ID points to.
  // Please note the following:
  // 1- Not all platforms support frontpanel port LEDs. If a chassis does
  //    not support port LEDs, a change in this map will not trigger an LED
  //    color/state update.
  // 2- Some platforms do not have per-channel LEDs on each transceiver port.
  //    We assume the lower-level software will aggregate the per-channel
  //    LED colors/states into one LED color/state for that trasceiver.
  // After each chassis config push, if there is already a health state for a
  // port in this map, we keep it as is, otherwise we initialize the state to
  // HEALTH_STATE_UNKNOWN, update the LEDs accordingly, and let a function call
  // to SetPortHealthState() update the health state by the SDN controller
  // later.
  std::map<uint64, std::map<uint32, HealthState>>
      node_id_to_port_id_to_health_state_;

  // Channel for receiving transceiver events from the Phal.
  std::shared_ptr<Channel<PhalInterface::TransceiverEvent>> xcvr_event_channel_;

  // Channel for receiving linkscan events from the BcmSdkInterface.
  std::shared_ptr<Channel<BcmSdkInterface::LinkscanEvent>>
      linkscan_event_channel_;

  // WriterInterface<GnmiEventPtr> object for sending event notifications.
  mutable absl::Mutex gnmi_event_lock_;
  std::shared_ptr<WriterInterface<GnmiEventPtr>> gnmi_event_writer_
      GUARDED_BY(gnmi_event_lock_);

  // Pointer to a PhalInterface implementation.
  PhalInterface* phal_interface_;  // not owned by this class.

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // not owned by this class.

  // Pointer to an instance of BcmSerdesDbManager for accessing serdes database.
  BcmSerdesDbManager* bcm_serdes_db_manager_;  // not owned by this class.

  // Map from unit to BcmNode instance.
  std::map<int, BcmNode*> unit_to_bcm_node_;  // not owned by this class.

  friend class BcmChassisManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_H_
