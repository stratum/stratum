// Copyright 2018 Google LLC
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


#include "stratum/hal/lib/bcm/bcm_sim_test_fixture.h"

#include "gflags/gflags.h"
#include "devtools/build/runtime/get_runfiles_dir.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/p4/p4_runtime_real.h"
#include "stratum/hal/lib/phal/phal_sim.h"
#include "stratum/lib/utils.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"

DECLARE_string(base_bcm_chassis_map_file);
DECLARE_string(bcm_serdes_db_proto_file);
DECLARE_string(bcm_sdk_checkpoint_dir);
DECLARE_string(bcm_sdk_config_file);
DECLARE_string(bcm_sdk_config_flush_file);
DECLARE_bool(blaze_test);

// For simulator, we don't need the diag shell, so we overwrite the openpty with
// empty function. Without the overwriting, compiler will generate undefined
// referenece error.
extern "C" int openpty(int *amaster, int *aslave, char *name,
                       const struct termios *termp,
                       const struct winsize *winp) {
  return 0;
}

namespace stratum {
namespace hal {
namespace bcm {

namespace {

// File path to BCM SDK simulator binary used by the BcmSdkSim. Note, the
// extension ".k8" is truncated.
constexpr char kBcmSimBin[] =
    "stratum/hal/bin/bcm/sim/bcm_pcid_sim";
// File path to chassis config which is used by chassis manager to generate on
// fly the BCM config file loaded by the SDK simulator.
constexpr char kTestChassisConfigGeneric Trident2[] =
    "stratum/testing/protos/test_chassis_config_40g.pb.txt";
// File path to BCM serdes db which is required by chassis manager
// initialization.
constexpr char kTestBcmSerdesDb[] =
    "stratum/hal/config/bcm_serdes_db.pb.bin";
// File path to BCM chassis map which is required by chassis manager
// initialization.
constexpr char kTestBaseBcmChassisMapFile[] =
    "stratum/hal/config/base_bcm_chassis_map.pb.txt";
// File path for chassis manager to dump the BCM config file loaded by the SDK
// simulator.
constexpr char kTestBcmSdkConfigFile[] = "/tmp/config.bcm";
// File path for chassis manager to dump the BCM config flush file loaded by the
// SDK simulator.
constexpr char kTestBcmSdkConfigFlushFile[] = "/tmp/config.bcm.tmp";
// Dir path used by SDK to save checkpoint.
constexpr char kTestBcmSdkCheckpointDir[] = "/tmp/sdk_checkpoint";

}  // namespace

constexpr uint64 BcmSimTestFixture::kNodeId;

void BcmSimTestFixture::SetUp() {
  FLAGS_base_bcm_chassis_map_file =
      devtools_build::GetDataDependencyFilepath(kTestBaseBcmChassisMapFile);
  FLAGS_bcm_serdes_db_proto_file =
      devtools_build::GetDataDependencyFilepath(kTestBcmSerdesDb);
  FLAGS_bcm_sdk_checkpoint_dir = kTestBcmSdkCheckpointDir;
  FLAGS_bcm_sdk_config_file = kTestBcmSdkConfigFile;
  FLAGS_bcm_sdk_config_flush_file = kTestBcmSdkConfigFlushFile;

  bcm_sdk_sim_ = ::absl::WrapUnique(BcmSdkSim::CreateSingleton(
      devtools_build::GetDataDependencyFilepath(kBcmSimBin)));
  P4RuntimeReal::GetSingleton();
  phal_sim_ = ::absl::WrapUnique(PhalSim::CreateSingleton());
  bcm_serdes_db_manager_ = BcmSerdesDbManager::CreateInstance();
  bcm_chassis_manager_ = BcmChassisManager::CreateInstance(
      OPERATION_MODE_SIM, phal_sim_.get(), bcm_sdk_sim_.get(),
      bcm_serdes_db_manager_.get());
  p4_table_mapper_ = P4TableMapper::CreateInstance();
  bcm_table_manager_ = BcmTableManager::CreateInstance(
      bcm_chassis_manager_.get(), p4_table_mapper_.get(), 0);
  bcm_acl_manager_ = BcmAclManager::CreateInstance(
      bcm_chassis_manager_.get(), bcm_table_manager_.get(), bcm_sdk_sim_.get(),
      p4_table_mapper_.get(), 0);

  ASSERT_OK(ReadProtoFromTextFile(
      devtools_build::GetDataDependencyFilepath(kTestChassisConfigGeneric Trident2),
      &chassis_config_));
  {
    absl::WriterMutexLock l(&bcm::chassis_lock);
    bcm::shutdown = false;
    ASSERT_OK(p4_table_mapper_->PushChassisConfig(chassis_config_, kNodeId));
    ASSERT_OK(bcm_chassis_manager_->PushChassisConfig(chassis_config_));
    ASSERT_OK(bcm_acl_manager_->PushChassisConfig(chassis_config_, kNodeId));
    ASSERT_OK(bcm_table_manager_->PushChassisConfig(chassis_config_, kNodeId));
  }
}

void BcmSimTestFixture::TearDown() {
  {
    absl::WriterMutexLock l(&bcm::chassis_lock);
    bcm::shutdown = true;
  }
  ASSERT_OK(bcm_chassis_manager_->Shutdown());
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
