// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// BcmTunnelManager stubs.

#include "stratum/hal/lib/bcm/bcm_tunnel_manager.h"

namespace stratum {

namespace hal {
namespace bcm {

BcmTunnelManager::BcmTunnelManager()
    : bcm_sdk_interface_(nullptr),
      bcm_table_manager_(nullptr),
      node_id_(0),
      unit_(-1) {}

BcmTunnelManager::BcmTunnelManager(BcmSdkInterface* bcm_sdk_interface,
                                   BcmTableManager* bcm_table_manager, int unit)
    : bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      bcm_table_manager_(ABSL_DIE_IF_NULL(bcm_table_manager)),
      node_id_(0),
      unit_(unit) {}

BcmTunnelManager::~BcmTunnelManager() {}

::util::Status BcmTunnelManager::PushChassisConfig(const ChassisConfig& config,
                                                   uint64 node_id) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::VerifyChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::PushForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::VerifyForwardingPipelineConfig(
    const ::p4::v1::ForwardingPipelineConfig& config) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::Shutdown() {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::InsertTableEntry(
    const ::p4::v1::TableEntry& entry) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::ModifyTableEntry(
    const ::p4::v1::TableEntry& entry) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

::util::Status BcmTunnelManager::DeleteTableEntry(
    const ::p4::v1::TableEntry& entry) {
  // TODO(teverman): Add implementation.
  return ::util::OkStatus();
}

std::unique_ptr<BcmTunnelManager> BcmTunnelManager::CreateInstance(
    BcmSdkInterface* bcm_sdk_interface, BcmTableManager* bcm_table_manager,
    int unit) {
  return absl::WrapUnique(
      new BcmTunnelManager(bcm_sdk_interface, bcm_table_manager, unit));
}

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
