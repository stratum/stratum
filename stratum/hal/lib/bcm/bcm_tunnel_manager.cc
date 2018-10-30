// BcmTunnelManager stubs.

#include "stratum/hal/lib/bcm/bcm_tunnel_manager.h"

namespace google {
namespace hercules {
namespace hal {
namespace bcm {

BcmTunnelManager::BcmTunnelManager()
    : bcm_sdk_interface_(nullptr),
      bcm_table_manager_(nullptr),
      node_id_(0),
      unit_(-1) {
}

BcmTunnelManager::BcmTunnelManager(
    BcmSdkInterface* bcm_sdk_interface,
    BcmTableManager* bcm_table_manager, int unit)
    : bcm_sdk_interface_(ABSL_DIE_IF_NULL(bcm_sdk_interface)),
      bcm_table_manager_(ABSL_DIE_IF_NULL(bcm_table_manager)),
      node_id_(0),
      unit_(unit) {
}

BcmTunnelManager::~BcmTunnelManager() {
}

::util::Status BcmTunnelManager::PushChassisConfig(
    const ChassisConfig& config, uint64 node_id) {
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
}  // namespace hercules
}  // namespace google
