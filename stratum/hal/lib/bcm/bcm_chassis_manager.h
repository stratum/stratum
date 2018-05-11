/*
 * Copyright 2018 Google LLC
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

#include "third_party/stratum/glue/status/status.h"
#include "third_party/stratum/glue/status/statusor.h"
#include "third_party/stratum/hal/lib/bcm/bcm.pb.h"
#include "third_party/stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "third_party/stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "third_party/stratum/hal/lib/common/common.pb.h"
#include "third_party/stratum/hal/lib/common/gnmi_events.h"
#include "third_party/stratum/hal/lib/common/phal_interface.h"
#include "third_party/stratum/hal/lib/common/writer_interface.h"
#include "third_party/stratum/lib/channel/channel.h"
#include "third_party/stratum/public/proto/hal.grpc.pb.h"
#include "third_party/absl/base/integral_types.h"
#include "third_party/absl/base/thread_annotations.h"
#include "third_party/absl/synchronization/mutex.h"

namespace stratum {
namespace hal {
namespace bcm {

// Lock which governs chassis state (ports, etc.) across the entire switch.
// Initialized in bcm_chassis_manager.cc.
extern absl::Mutex chassis_lock;

// Flag indicating if the switch has been shut down. Initialized to false in
// bcm_chassis_manager.cc.
extern bool shutdown;

// The "BcmChassisManager" class encapsulates all the chassis-related
// functionalities needed in BcmSwitch class.
// NOTE: The maps in this class may be accessed in such a way where the order of
// the keys are important. That is why we chose to use std::map as opposed to
// std::unordered_map or gtl::flat_hash_map and accept a little bit of
// performance hit when doing lookup.
class BcmChassisManager {
 public:
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

  // Gets BcmChip given its unit.
  virtual ::util::StatusOr<BcmChip> GetBcmChip(int unit) const;

  // Gets a BcmPort given its slot, port, and channel.
  virtual ::util::StatusOr<BcmPort> GetBcmPort(int slot, int port,
                                               int channel) const;

  // Gets a copy of internal node_id_to_unit_ for external managers. We do not
  // return a ref/pointer to the map as it gets cleared out and regenerated as
  // part of each chassis config push.
  virtual ::util::StatusOr<std::map<uint64, int>> GetNodeIdToUnitMap() const;

  // Gets the unit represented by the given node_id.
  virtual ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const;

  // Gets a map of singleton port IDs to their corresponding
  // (unit, logical_port) pair.
  virtual ::util::StatusOr<std::map<uint64, std::pair<int, int>>>
  GetPortIdToUnitLogicalPortMap() const;

  // Gets a map of trunk port IDs to their corresponding (unit, trunk_port)
  // pair.
  virtual ::util::StatusOr<std::map<uint64, std::pair<int, int>>>
  GetTrunkIdToUnitTrunkPortMap() const;

  // Returns a state of the port given its ID.
  virtual ::util::StatusOr<PortState> GetPortState(uint64 port_id) const;

  // Registers a WriterInterface<GnmiEventPtr> for sending gNMI events.
  virtual ::util::Status RegisterEventNotifyWriter(
      const std::shared_ptr<WriterInterface<GnmiEventPtr>>& writer)
      LOCKS_EXCLUDED(gnmi_event_lock_);

  virtual ::util::Status UnregisterEventNotifyWriter()
      LOCKS_EXCLUDED(gnmi_event_lock_);

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
  ::util::Status SyncInternalState(const ChassisConfig& config);

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
  void SendPortOperStateGnmiEvent(uint64 node_id, uint64 port_id,
                                  PortState new_state)
      SHARED_LOCKS_REQUIRED(chassis_lock) LOCKS_EXCLUDED(gnmi_event_lock_);

  // Sets the speed for a flex port group after a chassis config is pushed. The
  // input is a (slot, port) corresponding to the port group. The function
  // determines if there is a change in the speed based on the pushed chassis
  // config. If so, it configures the speed for the given port group.
  ::util::StatusOr<bool> SetSpeedForFlexPortGroup(
      std::pair<int, int> slot_port_pair) const;

  // Sets port options for ports in a flex or non-flex port group. The inputs
  // are a (slot, port) corresponding to all the ports in the port group and a
  // BcmPortOptions message determining the options that need to be applied to
  // all the ports in the port group.
  ::util::Status SetPortOptionsForPortGroup(
      std::pair<int, int> slot_port_pair, const BcmPortOptions& options) const;

  // A boolean which determines whether a (slot, port) pair belong to an
  // internal port (e.g. BP port in BG or SPICA).
  bool IsInternalPort(std::pair<int, int> slot_port_pair) const;

  // Determines the mode of operation:
  // - OPERATION_MODE_STANDALONE: when Hercules stack runs independently and
  // therefore needs to do all the SDK initialization itself.
  // - OPERATION_MODE_COUPLED: when Hercules stack runs as part of Sandcastle
  // stack, coupled with the rest of stack processes.
  // - OPERATION_MODE_SIM: when Hercules stack runs in simulation mode.
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
  int transceiver_event_writer_id_;

  // base_bcm_chassis_map_ includes all the possible slot, port, channel, and
  // speed_bps for all the front panel ports. This map is fixed for any chassis,
  // and is read from a file in ReadBaseBcmChassisMapFromFile().
  std::unique_ptr<BcmChassisMap> base_bcm_chassis_map_;

  // Pointer to a copy of bcm_chassis_map in bcm_init_config given when calling
  // Initialize(). This is fixed and will not change while the stack is up.
  // In the VerifyChassisConfig(), we make sure if the class is initialized, the
  // bcm_chassis_map found based on the config stays the same as
  // applied_bcm_chassis_map_ or we report "reboot required".
  std::unique_ptr<BcmChassisMap> applied_bcm_chassis_map_;

  // Map from 0-based unit to a pointer to its corresponding BcmChip.
  std::map<int, BcmChip*> unit_to_bcm_chip_;

  // Map from std::tuple<int, int, int> representing (slot, port, channel) of
  // a singleton port to a pointer to its corresponding BcmPort. This map is
  // populated in PushChassisConfig(). For each (slot, port, channel), the value
  // follows the following rules:
  // 1- For non-flex ports, this is a pointer to a BcmPort which is a copy of
  //    the corresponding entry in bcm_chassis_map_.
  // 2- For flex ports, this is a pointer to a BcmPort which is matching the
  //    config and may change after each chassis config push.
  std::map<std::tuple<int, int, int>, BcmPort*> slot_port_channel_to_bcm_port_;

  // Map from std::pair<int, int> representing (slot, port) of singleton port
  // to the vector of BcmPort pointers configured for that specific port. We
  // keep two maps, one for flex and one for non-flex ports. Note that the data
  // in these maps is already available in slot_port_channel_to_bcm_port_.
  // These maps are kept for faster and easier access to the BcmPorts given
  // (slot, port) only.
  std::map<std::pair<int, int>, std::vector<BcmPort*>>
      slot_port_to_flex_bcm_ports_;
  std::map<std::pair<int, int>, std::vector<BcmPort*>>
      slot_port_to_non_flex_bcm_ports_;

  // Map from std::pair<int, int> representing (slot, port) of singleton port
  // to the state of the transceiver module used in that (slot, port).
  std::map<std::pair<int, int>, HwState> slot_port_to_transceiver_state_;

  // Map from the unit to set of logical ports. Use for fast accessing all the
  // ports on a unit for BCM calls.
  std::map<int, std::set<int>> unit_to_logical_ports_;

  // Map from node ID to the unit number as specified by the config.
  std::map<uint64, int> node_id_to_unit_;

  // Map from unit number to the node ID as specified by the config.
  std::map<int, uint64> unit_to_node_id_;

  // Map from node ID to the set of port IDs that belong to that node.
  std::map<uint64, std::set<uint64>> node_id_to_port_ids_;

  // Map from singleton port ID to its corresponding (slot, port, channel) as
  // specified by the config.
  std::map<uint64, std::tuple<int, int, int>> port_id_to_slot_port_channel_;

  // Map from std::pair<int, int> representing (unit, logical_port) of a
  // singleton port to its corresponding port_id specified through the config.
  // Will be updated after each chassis config push.
  std::map<std::pair<int, int>, uint64> unit_logical_port_to_port_id_;

  // Map from std::tuple<int, int, int> representing (slot, port, channel) of
  // a singleton port to its most updated port state. After chassis config push,
  // if there is already a state for a port in this map, we keep the state,
  // otherwise we initialize the state to PORT_STATE_UNKNOWN and let the next
  // linkscan event update the state.
  std::map<std::tuple<int, int, int>, PortState>
      slot_port_channel_to_port_state_;

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

  friend class BcmChassisManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_MANAGER_H_
