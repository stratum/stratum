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

#ifndef STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_INTERFACE_H_
#define STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_INTERFACE_H_

#include <map>
#include <string>
#include <set>

#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/utils.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/utils.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {

namespace hal {
namespace bcm {

// This class exists to provide a read-only interface to chassis state to all
// classes which rely on that state but do not mutate it.
class BcmChassisRoInterface {
 public:
  virtual ~BcmChassisRoInterface() {}

  // Returns the BcmChip corresponding to the given BCM unit.
  virtual ::util::StatusOr<BcmChip> GetBcmChip(int unit) const = 0;

  // Returns the BcmPort corresponding to the given slot, port, and channel.
  virtual ::util::StatusOr<BcmPort> GetBcmPort(int slot, int port,
                                               int channel) const = 0;

  // Returns the BcmPort corresponding to the given singleton port.
  virtual ::util::StatusOr<BcmPort> GetBcmPort(uint64 node_id,
                                               uint32 port_id) const = 0;

  // Returns the map from node ID to BCM unit number.
  virtual ::util::StatusOr<std::map<uint64, int>> GetNodeIdToUnitMap()
      const = 0;

  // Returns the BCM unit number corresponding to the given node ID.
  virtual ::util::StatusOr<int> GetUnitFromNodeId(uint64 node_id) const = 0;

  // Returns the map from singleton port ID to its corresponding SdkPort.
  virtual ::util::StatusOr<std::map<uint32, SdkPort>> GetPortIdToSdkPortMap(
      uint64 node_id) const = 0;

  // Returns the map from trunk port ID to its corresponding SdkTrunk.
  virtual ::util::StatusOr<std::map<uint32, SdkTrunk>> GetTrunkIdToSdkTrunkMap(
      uint64 node_id) const = 0;

  // Returns a state of a singleton port given its ID and the ID of its node.
  virtual ::util::StatusOr<PortState> GetPortState(uint64 node_id,
                                                   uint32 port_id) const = 0;

  // Returns the state of a singleton port given the unit and BCM logical port
  // number.
  virtual ::util::StatusOr<PortState> GetPortState(
      const SdkPort& sdk_port) const = 0;

  // Returns a state of a trunk port port given its ID and the ID of its node.
  virtual ::util::StatusOr<TrunkState> GetTrunkState(uint64 node_id,
                                                     uint32 trunk_id) const = 0;

  // Returns the most updated members of a trunk given its ID and the ID of its
  // node.
  virtual ::util::StatusOr<std::set<uint32>> GetTrunkMembers(
      uint64 node_id, uint32 trunk_id) const = 0;

  // Returns the ID of the parent trunk, if and only if the given port ID is
  // part of a trunk. Return error if the port is not known or if it is not
  // part of a trunk.
  virtual ::util::StatusOr<uint32> GetParentTrunkId(uint64 node_id,
                                                    uint32 port_id) const = 0;

  // Returns the admin state of the given singleton port.
  virtual ::util::StatusOr<AdminState> GetPortAdminState(
      uint64 node_id, uint32 port_id) const = 0;

  // Returns the loopback state of the given singleton port.
  virtual ::util::StatusOr<LoopbackState> GetPortLoopbackState(
      uint64 node_id, uint32 port_id) const = 0;

  // Gets the counters for a given singleton port.
  virtual ::util::Status GetPortCounters(uint64 node_id, uint32 port_id,
                                         PortCounters* pc) const = 0;

 protected:
  // Default constructor.
  BcmChassisRoInterface() {}
};

}  // namespace bcm
}  // namespace hal

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_CHASSIS_RO_INTERFACE_H_
