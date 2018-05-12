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


#include "stratum/hal/lib/common/hal_config_to_oc.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {

// A test fixture is used because some common initialization is needed.
class HalToOcConfigTest : public ::testing::Test {
 protected:
  // Called before every TEST_F using this fixture.
  HalToOcConfigTest() {}

  void SetUp() override {
    constexpr char kHalProto[] = R"proto(
 chassis {
   platform: PLT_GENERIC_TRIDENT2
   name: "chassis #1"
   config_params { }
 }
 nodes {
   id: 1
   name: "node #1"
   slot: 3
   index: 1
   flow_params { }
   config_params {
     vlan_configs {
       vlan_id: 1
       block_broadcast: false
       block_unknown_multicast: true
       block_unknown_unicast: true
       disable_l2_learning: true
     }
   }
 }
 singleton_ports {
   id: 1
   name: "singleton #1"
   slot: 3
   port: 1
   channel: 1
   speed_bps: 10000000000
   flow_params { }
   config_params { }
 }
 trunk_ports {
   id: 100
   name: "trunk #1"
   type: LACP_TRUNK
   members: 1
   flow_params { }
   config_params { }
 }
 vendor_config {
   google_config {
     bcm_chassis_map_id: "first"
     node_id_to_knet_config {
       key: 1
       value {
         knet_intf_configs {
           mtu: 1500
           cpu_queue: 8
           vlan: 1
           purpose: BCM_KNET_INTF_PURPOSE_CONTROLLER
         }
       }
     }
     node_id_to_rx_config {
       key: 1
       value { }
     }
     node_id_to_tx_config {
       key: 1
       value { }
     }
     node_id_to_rate_limit_config {
       key: 1
       value { }
     }
   }
 }
    )proto";
    ASSERT_OK(ParseProtoFromString(kHalProto, &hal_config_proto_));
  }

  // 'protected' members can be used by all TEST_Fs using this fixture.

  google::hercules::ChassisConfig hal_config_proto_;

  HalConfigToOpenConfigProtoConverter converter_;
};

// This test verifies that HalToOcConfig correctly handles Generic Tomahawk 100G config.
TEST_F(HalToOcConfigTest, Generic Tomahawk100g) {
  ASSERT_OK(
      ReadProtoFromTextFile("stratum/hal/lib/common/"
                            "testdata/test_config_generic_tomahawk_100g_hercules.pb.txt",
                            &hal_config_proto_));
  ::util::StatusOr<oc::Device> oc_config =
        converter_.ChassisConfigToDevice(hal_config_proto_);
}

// This test verifies that HalToOcConfig correctly handles Generic Tomahawk 40G/100G config.
TEST_F(HalToOcConfigTest, Generic Tomahawk40g100g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/"
      "test_config_generic_tomahawk_40g_100g_hercules.pb.txt",
      &hal_config_proto_));
  ::util::StatusOr<oc::Device> oc_config =
        converter_.ChassisConfigToDevice(hal_config_proto_);
}

// This test verifies that HalToOcConfig correctly handles Generic Trident2 40G config.
TEST_F(HalToOcConfigTest, Generic Trident240g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/"
      "test_config_generic_trident2_40g_hercules.pb.txt",
      &hal_config_proto_));
  ::util::StatusOr<oc::Device> oc_config =
        converter_.ChassisConfigToDevice(hal_config_proto_);
}

// This test verifies that HalToOcConfig correctly handles Generic Trident2 40G config.
TEST_F(HalToOcConfigTest, Generic Trident240g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/"
      "test_config_generic_trident2_40g_hercules.pb.txt",
      &hal_config_proto_));
  ::util::StatusOr<oc::Device> oc_config =
        converter_.ChassisConfigToDevice(hal_config_proto_);
}

}  // namespace hal
}  // namespace stratum
