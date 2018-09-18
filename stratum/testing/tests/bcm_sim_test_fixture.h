#ifndef PLATFORMS_NETWORKING_HERCULES_TESTING_TESTS_BCM_SIM_TEST_FIXTURE_H_
#define PLATFORMS_NETWORKING_HERCULES_TESTING_TESTS_BCM_SIM_TEST_FIXTURE_H_

#include "platforms/networking/hercules/hal/lib/bcm/bcm_acl_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_chassis_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_l2_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_l3_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_node.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_packetio_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_sdk_sim.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_switch.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_table_manager.h"
#include "platforms/networking/hercules/hal/lib/bcm/bcm_tunnel_manager.h"
#include "platforms/networking/hercules/hal/lib/common/common.pb.h"
#include "platforms/networking/hercules/hal/lib/p4/p4_table_mapper.h"
#include "platforms/networking/hercules/hal/lib/phal/phal_sim.h"
#include "platforms/networking/hercules/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "sandblaze/p4lang/p4/v1/p4runtime.pb.h"

namespace google {
namespace hercules {
namespace hal {
namespace bcm {

class BcmSimTestFixture : public testing::Test {
 protected:
  BcmSimTestFixture() {}
  ~BcmSimTestFixture() override {}

  void SetUp() override;
  void TearDown() override;

  // The fix node Id and unit for the node tested by this class. This class
  // only tests one node with ID 1 and unit 0.
  static constexpr uint64 kNodeId = 1;
  static constexpr int kUnit = 0;

  ChassisConfig chassis_config_;
  ::p4::v1::ForwardingPipelineConfig forwarding_pipeline_config_;
  ::p4::v1::WriteRequest write_request_;
  std::unique_ptr<BcmAclManager> bcm_acl_manager_;
  std::unique_ptr<BcmChassisManager> bcm_chassis_manager_;
  std::unique_ptr<BcmL2Manager> bcm_l2_manager_;
  std::unique_ptr<BcmL3Manager> bcm_l3_manager_;
  std::unique_ptr<BcmNode> bcm_node_;
  std::unique_ptr<BcmPacketioManager> bcm_packetio_manager_;
  std::unique_ptr<BcmSdkSim> bcm_sdk_sim_;
  std::unique_ptr<BcmSerdesDbManager> bcm_serdes_db_manager_;
  std::unique_ptr<BcmSwitch> bcm_switch_;
  std::unique_ptr<BcmTableManager> bcm_table_manager_;
  std::unique_ptr<BcmTunnelManager> bcm_tunnel_manager_;
  std::unique_ptr<P4TableMapper> p4_table_mapper_;
  std::unique_ptr<PhalSim> phal_sim_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace hercules
}  // namespace google

#endif  // PLATFORMS_NETWORKING_HERCULES_TESTING_TESTS_BCM_SIM_TEST_FIXTURE_H_
