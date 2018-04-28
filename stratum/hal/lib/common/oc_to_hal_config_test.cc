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


#include "third_party/stratum/hal/lib/common/oc_to_hal_config.h"

#include "third_party/stratum/glue/status/status_test_util.h"
#include "third_party/stratum/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace stratum {
namespace hal {

// A test fixture is used because some common initialization is needed.
class OcToHalConfigTest : public ::testing::Test {
 protected:
  // Called before every TEST_F using this fixture.
  OcToHalConfigTest() {}

  void SetUp() override {
    constexpr char kDeviceProto[] = R"proto(
      components {
        component {
          key: "chassis"
          value: {
            type: HW_BCM_BASED_CHASSIS
            chassis {
              config {
                name: { value: "chassis" }
                vendor_specific {
                  [type.googleapis.com/oc.Bcm.Chassis.Config] {
                    bcm_chassis_map_id: { value: "first" }
                    node_id_to_tx_config {
                      key: 1
                      value: {
                      }
                    }
                    node_id_to_rate_limit_config {
                      key: 1
                      value: {
                      }
                    }
                    node_id_to_knet_config {
                      key: 1
                      value: {
                        node_uid: { value: 1 }
                        knet_intf_configs {
                          key: 1
                          value: {
                            vlan: { value: 1 }
                            purpose: BCM_KNET_IF_PURPOSE_CONTROLLER
                            id: { value: 1 }
                            mtu: { value: 1500 }
                            cpu_queue: { value: 8 }
                          }
                        }
                      }
                    }
                    node_id_to_rx_config {
                      key: 1
                      value: {
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    )proto";
    ASSERT_OK(ParseProtoFromString(kDeviceProto, &oc_proto_));
  }

  // 'protected' members can be used by all TEST_Fs using this fixture.

  oc::Device oc_proto_;

  OpenConfigToHalConfigProtoConverter converter_;
};

// This test verifies that OcToHalConfig correctly handles test Generic Tomahawk 40G/100G
// configuration.
TEST_F(OcToHalConfigTest, Generic Tomahawk40g100g) {
  ASSERT_OK(
      ReadProtoFromTextFile("third_party/stratum/hal/lib/common/"
                            "testdata/test_oc_config_generic_tomahawk_100g_hercules.pb.txt",
                            &oc_proto_));
  ASSERT_TRUE(converter_.IsCorrectProtoDevice(oc_proto_));
  ::util::StatusOr<google::hercules::ChassisConfig> hal_config =
      converter_.DeviceToChassisConfig(oc_proto_);
}

// This test verifies that OcToHalConfig correctly handles test Generic Tomahawk 100G
// configuration.
TEST_F(OcToHalConfigTest, Generic Tomahawk100g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "third_party/stratum/hal/lib/common/testdata/"
      "test_oc_config_generic_tomahawk_40g_100g_hercules.pb.txt",
      &oc_proto_));
  ASSERT_TRUE(converter_.IsCorrectProtoDevice(oc_proto_));
  ::util::StatusOr<google::hercules::ChassisConfig> hal_config =
      converter_.DeviceToChassisConfig(oc_proto_);
}

// This test verifies that OcToHalConfig correctly handles test Generic Trident2 40G
// configuration.
TEST_F(OcToHalConfigTest, Generic Trident240g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "third_party/stratum/hal/lib/common/testdata/"
      "test_oc_config_generic_trident2_40g_hercules.pb.txt",
      &oc_proto_));
  ASSERT_TRUE(converter_.IsCorrectProtoDevice(oc_proto_));
  ::util::StatusOr<google::hercules::ChassisConfig> hal_config =
      converter_.DeviceToChassisConfig(oc_proto_);
}

// This test verifies that OcToHalConfig correctly handles test Generic Trident2 40G
// configuration.
TEST_F(OcToHalConfigTest, Generic Trident240g) {
  ASSERT_OK(ReadProtoFromTextFile(
      "third_party/stratum/hal/lib/common/testdata/"
      "test_oc_config_generic_trident2_40g_hercules.pb.txt",
      &oc_proto_));
  ASSERT_TRUE(converter_.IsCorrectProtoDevice(oc_proto_));
  ::util::StatusOr<google::hercules::ChassisConfig> hal_config =
      converter_.DeviceToChassisConfig(oc_proto_);
}

}  // namespace hal
}  // namespace stratum
