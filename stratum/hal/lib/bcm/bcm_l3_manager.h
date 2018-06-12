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


#ifndef STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_
#define STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_

#include <memory>
#include <utility>

#include "stratum/glue/status/status.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/bcm/bcm_sdk_interface.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/utils.h"
#include "stratum/public/proto/hal.grpc.pb.h"
#include "stratum/glue/net_util/integral_types.h"
#include "absl/base/thread_annotations.h"
#include "util/gtl/flat_hash_map.h"

namespace stratum {
namespace hal {
namespace bcm {

// This struct encapsulates the key for an IPv4/IPv6 LPM/host flow.
struct LpmOrHostKey {
  // The VRF. If not given, default VRF will be used.
  int vrf;
  // IPv4 subnet/mask.
  uint32 subnet_ipv4;
  uint32 mask_ipv4;
  // IPv6 subnet/mask.
  std::string subnet_ipv6;
  std::string mask_ipv6;
  LpmOrHostKey()
      : vrf(kVrfDefault),
        subnet_ipv4(0),
        mask_ipv4(0),
        subnet_ipv6(""),
        mask_ipv6("") {}
};

// This struct encapsulates the action params for an LPM/host flow.
struct LpmOrHostActionParams {
  // The value of class ID to set in the packet when it matches the LPM/host
  // flow. If non-positive, it will be ignored.
  int class_id;
  // Egress intf ID for the nexthop.
  int egress_intf_id;
  // A boolean determining whether the nexthop is an ECMP/WCMP group
  bool is_intf_multipath;
  LpmOrHostActionParams()
      : class_id(-1), egress_intf_id(-1), is_intf_multipath(false) {}
};

// The "BcmL3Manager" class implements the L3 routing functionality.
class BcmL3Manager {
 public:
  virtual ~BcmL3Manager();

  // Pushes the parts of the given ChassisConfig proto that this class cares
  // about. If the class is not initialized (i.e. if config is pushed for the
  // first time), this function also initializes class. The given node_id is
  // used to understand which part of the ChassisConfig is intended for this
  // class.
  virtual ::util::Status PushChassisConfig(const ChassisConfig& config,
                                           uint64 node_id);

  // Verifies the parts of ChassisConfig proto that this class cares about. The
  // given node_id is used to understand which part of the ChassisConfig is
  // intended for this class.
  virtual ::util::Status VerifyChassisConfig(const ChassisConfig& config,
                                             uint64 node_id);

  // Performs coldboot shutdown. Note that there is no public Initialize().
  // Initialization is done as part of PushChassisConfig() if the class is not
  // initialized by the time we push config.
  virtual ::util::Status Shutdown();

  // Finds or creates an egress non-multipath nexthop and returns its egress
  // intf ID. Note that it is perfectly OK for multiple group members to point
  // to the same egress intf ID, so we need to make sure if the egress intf
  // is already there we just return its ID without returning error.
  virtual ::util::StatusOr<int> FindOrCreateNonMultipathNexthop(
      const BcmNonMultipathNexthop& nexthop);

  // Finds or creates an egress multipath (ECMP/WCMP) nexthop and returns its
  // egress intf ID. Note that it is perfectly OK for multiple groups to point
  // to the same egress intf ID, so we need to make sure if the egress intf
  // is already there we just return its ID without returning error.
  virtual ::util::StatusOr<int> FindOrCreateMultipathNexthop(
      const BcmMultipathNexthop& nexthop);

  // Modifies an existing egress non-multipath nexthop given its ID. The same
  // egress ID will point to a new nexthop using this method.
  virtual ::util::Status ModifyNonMultipathNexthop(
      int egress_intf_id, const BcmNonMultipathNexthop& nexthop);

  // Modifies an existing egress multipath (ECMP/WCMP) nexthop given its ID
  // with a new set of members given in BcmMultipathNexthop.
  virtual ::util::Status ModifyMultipathNexthop(
      int egress_intf_id, const BcmMultipathNexthop& nexthop);

  // Deletes an egress non-multipath nexthop given its ID.
  virtual ::util::Status DeleteNonMultipathNexthop(int egress_intf_id);

  // Deletes an egress multipath (ECMP/WCMP) nexthop given its ID.
  virtual ::util::Status DeleteMultipathNexthop(int egress_intf_id);

  // Inserts an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given BcmFlowEntry.
  virtual ::util::Status InsertLpmOrHostFlow(
      const BcmFlowEntry& bcm_flow_entry);

  // Modifies an IPv4/IPv6 L3 LPM/Host flow. The function programs the
  // low level routes into the given unit based on the given BcmFlowEntry. The
  // fields populated in BcmFlowEntry are the same as the ones populated when
  // adding the flow in InsertLpmOrHostFlow().
  virtual ::util::Status ModifyLpmOrHostFlow(
      const BcmFlowEntry& bcm_flow_entry);

  // Deletes an IPv4/IPv6 L3 LPM/Host flow. The fields populated in BcmFlowEntry
  // define the key for the flow (the egress_intf_id or class_id not needed).
  virtual ::util::Status DeleteLpmOrHostFlow(
      const BcmFlowEntry& bcm_flow_entry);

  // Factory function for creating the instance of the class.
  static std::unique_ptr<BcmL3Manager> CreateInstance(
      BcmSdkInterface* bcm_sdk_interface, int unit);

  // BcmL3Manager is neither copyable nor movable.
  BcmL3Manager(const BcmL3Manager&) = delete;
  BcmL3Manager& operator=(const BcmL3Manager&) = delete;

 protected:
  // Default constructor. To be called by the Mock class instance only.
  BcmL3Manager();

 private:
  // Private constructor. Use CreateInstance() to create an instance of this
  // class.
  BcmL3Manager(BcmSdkInterface* bcm_sdk_interface, int unit);

  // Helper to extract IPv4/IPv6 L3 LPM/Host flow keys given BcmFlowEntry.
  ::util::Status ExtractLpmOrHostKey(const BcmFlowEntry& bcm_flow_entry,
                                     LpmOrHostKey* key);

  // Helper to extract IPv4/IPv6 L3 LPM/Host flow actions given BcmFlowEntry.
  ::util::Status ExtractLpmOrHostActionParams(
      const BcmFlowEntry& bcm_flow_entry, LpmOrHostActionParams* action_params);

  // Helpers for incrementing/decrementing the ref count for a router intf. In
  // case router intf has zero ref count, DecrementRefCount() will cleanup the
  // router intf from SDK as well.
  ::util::Status IncrementRefCount(int router_intf_id);
  ::util::Status DecrementRefCount(int router_intf_id);

  // Map from router_intf_id to ref counts (the number egress intfs pointing to
  // this router intf).
  // TODO: We keep this map as there is no good way to get this
  // directly from SDK. Investigate.
  gtl::flat_hash_map<int, uint32> router_intf_ref_count_;

  // Pointer to a BcmSdkInterface implementation that wraps all the SDK calls.
  BcmSdkInterface* bcm_sdk_interface_;  // not owned by this class.

  // Logical node ID corresponding to the node/ASIC managed by this class
  // instance. Assigned on PushChassisConfig() and might change during the
  // lifetime of the class.
  uint64 node_id_;

  // Fixed zero-based BCM unit number corresponding to the node/ASIC managed by
  // this class instance. Assigned in the class constructor.
  const int unit_;

  friend class BcmL3ManagerTest;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_L3_MANAGER_H_
