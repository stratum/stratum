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


#ifndef STRATUM_HAL_LIB_BCM_BCM_SIM_TEST_FIXTURE_H_
#define STRATUM_HAL_LIB_BCM_BCM_SIM_TEST_FIXTURE_H_

#include "third_party/stratum/hal/lib/bcm/bcm_acl_manager.h"
#include "third_party/stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "third_party/stratum/hal/lib/bcm/bcm_sdk_sim.h"
#include "third_party/stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "third_party/stratum/hal/lib/bcm/bcm_table_manager.h"
#include "third_party/stratum/hal/lib/p4/p4_table_mapper.h"
#include "third_party/stratum/hal/lib/phal/phal_sim.h"
#include "third_party/stratum/lib/utils.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {
namespace bcm {

class BcmSimTestFixture : public testing::Test {
 protected:
  BcmSimTestFixture() {}
  ~BcmSimTestFixture() override {}

  void SetUp() override;
  void TearDown() override;

  const ChassisConfig& chassis_config() const { return chassis_config_; }
  P4TableMapper& p4_table_mapper() const { return *p4_table_mapper_; }
  BcmAclManager& bcm_acl_manager() const { return *bcm_acl_manager_; }

  // The fix node Id for the node tested by this class. This class only tests
  // the node with ID 1.
  static constexpr uint64 kNodeId = 1;

 private:
  ChassisConfig chassis_config_;

  std::unique_ptr<BcmSdkSim> bcm_sdk_sim_;
  std::unique_ptr<BcmAclManager> bcm_acl_manager_;
  std::unique_ptr<BcmChassisManager> bcm_chassis_manager_;
  std::unique_ptr<BcmSerdesDbManager> bcm_serdes_db_manager_;
  std::unique_ptr<BcmTableManager> bcm_table_manager_;
  std::unique_ptr<P4TableMapper> p4_table_mapper_;
  std::unique_ptr<PhalSim> phal_sim_;
};

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_BCM_BCM_SIM_TEST_FIXTURE_H_
