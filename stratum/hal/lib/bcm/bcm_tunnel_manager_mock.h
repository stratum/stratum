#ifndef STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_
#define STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_

#include "stratum/hal/lib/bcm/bcm_tunnel_manager.h"
#include "gmock/gmock.h"

namespace stratum {

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

}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_TUNNEL_MANAGER_MOCK_H_
