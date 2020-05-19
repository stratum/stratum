// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/testing/tests/bcm_sim_test_fixture.h"

#include <map>

#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "devtools/build/runtime/get_runfiles_dir.h"
#include "gflags/gflags.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/test_utils/matchers.h"

DECLARE_string(base_bcm_chassis_map_file);
DECLARE_string(bcm_hardware_specs_file);
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
    "google3/platforms/networking/stratum/hal/bin/bcm/sim/bcm_pcid_sim";

// File path to the only chassis config used for testing. We use a test Generic
// Trident2 chassis config.
constexpr char kTestChassisConfigFile[] =
    "platforms/networking/stratum/testing/protos/"
    "test_chassis_config_generic_trident2_40g_stratum.pb.txt";

// Test P4Info and P4PipelineConfig files used for testing. We test the ToR
// P4 specs only.
constexpr char kTestP4InfoFile[] =
    "platforms/networking/stratum/testing/protos/"
    "test_p4_info_stratum_tor.pb.txt";
constexpr char kTestP4PipelineConfigFile[] =
    "platforms/networking/stratum/testing/protos/"
    "test_p4_pipeline_config_stratum_tor.pb.txt";

// Test WriteRequest proto used for testing. The proto is compatible with the
// forwarding pipeline config pushed.
constexpr char kTestWriteRequestFile[] =
    "platforms/networking/stratum/testing/protos/"
    "test_write_request_stratum_tor_generic_trident2_generic_tomahawk.pb.txt";

// File path to BCM serdes db which is required for chassis manager
// initialization.
constexpr char kTestBcmSerdesDbFile[] =
    "google3/platforms/networking/stratum/hal/config/"
    "generic_trident2_bcm_serdes_db_stratum.pb.bin";

// File path to BCM chassis map which is required for chassis manager
// initialization.
constexpr char kTestBaseBcmChassisMapFile[] =
    "google3/platforms/networking/stratum/hal/config/"
    "base_bcm_chassis_map_generic_trident2_stratum.pb.txt";

// File path for chassis manager to dump the BCM config file loaded by the SDK
// simulator.
constexpr char kTestBcmSdkConfigFile[] = "/tmp/config.bcm";

// File path for chassis manager to dump the BCM config flush file loaded by the
// SDK simulator.
constexpr char kTestBcmSdkConfigFlushFile[] = "/tmp/config.bcm.tmp";

// Dir path used by SDK to save checkpoint.
constexpr char kTestBcmSdkCheckpointDir[] = "/tmp/sdk_checkpoint";

// File path to BCM hardware map which is required for acl manager
// initialization.
constexpr char kTestBcmHardwareMap[] =
    "google3/platforms/networking/stratum/hal/config/"
    "bcm_hardware_specs.pb.txt";

}  // namespace

constexpr uint64 BcmSimTestFixture::kNodeId;
constexpr int BcmSimTestFixture::kUnit;

void BcmSimTestFixture::SetUp() {
  FLAGS_base_bcm_chassis_map_file =
      devtools_build::GetDataDependencyFilepath(kTestBaseBcmChassisMapFile);
  FLAGS_bcm_hardware_specs_file =
      devtools_build::GetDataDependencyFilepath(kTestBcmHardwareMap);
  FLAGS_bcm_serdes_db_proto_file =
      devtools_build::GetDataDependencyFilepath(kTestBcmSerdesDbFile);
  FLAGS_bcm_sdk_checkpoint_dir = kTestBcmSdkCheckpointDir;
  FLAGS_bcm_sdk_config_file = kTestBcmSdkConfigFile;
  FLAGS_bcm_sdk_config_flush_file = kTestBcmSdkConfigFlushFile;

  // Setup all the managers.
  bcm_sdk_sim_ = ::absl::WrapUnique(BcmSdkSim::CreateSingleton(
      devtools_build::GetDataDependencyFilepath(kBcmSimBin)));
  phal_sim_ = ::absl::WrapUnique(PhalSim::CreateSingleton());
  bcm_serdes_db_manager_ = BcmSerdesDbManager::CreateInstance();
  bcm_chassis_manager_ = BcmChassisManager::CreateInstance(
      OPERATION_MODE_SIM, phal_sim_.get(), bcm_sdk_sim_.get(),
      bcm_serdes_db_manager_.get());
  p4_table_mapper_ = P4TableMapper::CreateInstance();
  bcm_table_manager_ = BcmTableManager::CreateInstance(
      bcm_chassis_manager_.get(), p4_table_mapper_.get(), kUnit);
  bcm_acl_manager_ = BcmAclManager::CreateInstance(
      bcm_chassis_manager_.get(), bcm_table_manager_.get(), bcm_sdk_sim_.get(),
      p4_table_mapper_.get(), kUnit);
  bcm_l2_manager_ = BcmL2Manager::CreateInstance(bcm_chassis_manager_.get(),
                                                 bcm_sdk_sim_.get(), kUnit);
  bcm_l3_manager_ = BcmL3Manager::CreateInstance(
      bcm_sdk_sim_.get(), bcm_table_manager_.get(), kUnit);
  bcm_tunnel_manager_ = BcmTunnelManager::CreateInstance(
      bcm_sdk_sim_.get(), bcm_table_manager_.get(), kUnit);
  bcm_packetio_manager_ = BcmPacketioManager::CreateInstance(
      OPERATION_MODE_SIM, bcm_chassis_manager_.get(), p4_table_mapper_.get(),
      bcm_sdk_sim_.get(), kUnit);
  bcm_node_ = BcmNode::CreateInstance(
      bcm_acl_manager_.get(), bcm_l2_manager_.get(), bcm_l3_manager_.get(),
      bcm_packetio_manager_.get(), bcm_table_manager_.get(),
      bcm_tunnel_manager_.get(), p4_table_mapper_.get(), kUnit);
  std::map<int, BcmNode *> unit_to_bcm_node;
  unit_to_bcm_node[kUnit] = bcm_node_.get();
  bcm_switch_ = BcmSwitch::CreateInstance(
      phal_sim_.get(), bcm_chassis_manager_.get(), unit_to_bcm_node);

  // Initialize the chassis config & forwarding pipeline config.
  ASSERT_OK(ReadProtoFromTextFile(kTestChassisConfigFile, &chassis_config_))
      << "Failed to read chassis config proto from file "
      << kTestChassisConfigFile << ".";
  ASSERT_OK(ReadProtoFromTextFile(kTestP4InfoFile,
                                  forwarding_pipeline_config_.mutable_p4info()))
      << "Failed to read p4 info proto from file " << kTestP4InfoFile << ".";
  P4PipelineConfig p4_pipeline_config;
  ASSERT_OK(
      ReadProtoFromTextFile(kTestP4PipelineConfigFile, &p4_pipeline_config))
      << "Failed to read p4 pipeline config proto from file "
      << kTestP4PipelineConfigFile << ".";
  ASSERT_TRUE(p4_pipeline_config.SerializeToString(
      forwarding_pipeline_config_.mutable_p4_device_config()))
      << "Failed to serialize p4_pipeline_config to string.";

  // Initialize the write request proto.
  ASSERT_OK(ReadProtoFromTextFile(kTestWriteRequestFile, &write_request_))
      << "Failed to read write request proto from file "
      << kTestWriteRequestFile << ".";
  write_request_.set_device_id(kNodeId);

  // Push the chassis config to initialize all the managers.
  {
    absl::WriterMutexLock l(&chassis_lock);
    shutdown = false;
  }
  ASSERT_OK(bcm_switch_->PushChassisConfig(chassis_config_));
}

void BcmSimTestFixture::TearDown() { ASSERT_OK(bcm_switch_->Shutdown()); }

}  // namespace bcm
}  // namespace hal

}  // namespace stratum
