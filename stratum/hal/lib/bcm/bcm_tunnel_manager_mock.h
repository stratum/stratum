#ifndef PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_
#define PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_

#include "platforms/networking/hercules/hal/lib/bcm/bcm_tunnel_manager.h"
#include "testing/base/public/gmock.h"

namespace google {
namespace hercules {
namespace hal {
namespace bcm {

class BcmTunnelManagerMock : public BcmTunnelManager {
 public:
  MOCK_METHOD2(PushChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD2(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config, uint64 node_id));
  MOCK_METHOD1(
      PushForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD1(
      VerifyForwardingPipelineConfig,
      ::util::Status(const ::p4::v1::ForwardingPipelineConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD1(InsertTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
  MOCK_METHOD1(ModifyTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
  MOCK_METHOD1(DeleteTableEntry,
               ::util::Status(const ::p4::v1::TableEntry& entry));
};

}  // namespace bcm
}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_
