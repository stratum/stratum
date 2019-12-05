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

#include "stratum/hal/lib/common/openconfig_converter.h"

#include <google/protobuf/text_format.h>

#include <tuple>

#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {

namespace hal {

// This test fixture is instantiated with 2 Protobuf text file paths: the
// chassis config and the corresponding OpenConfig config. It verifies that each
// one can be converted to the other using the OpenconfigConverter.
class OpenconfigConverterSimpleTest
    : public testing::TestWithParam<std::tuple<const char*, const char*> > {
 protected:
  OpenconfigConverterSimpleTest()
      : chassis_config_path(std::get<0>(GetParam())),
        oc_config_path(std::get<1>(GetParam())) {}

  const char* chassis_config_path;
  const char* oc_config_path;
};

TEST_P(OpenconfigConverterSimpleTest, ChassisToOc) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(chassis_config_path, &chassis_config));
  ::util::StatusOr<openconfig::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);

  const openconfig::Device& device = ret.ConsumeValueOrDie();

  openconfig::Device device_from_file;
  ASSERT_OK(ReadProtoFromTextFile(oc_config_path, &device_from_file));

  // TODO(antonin): there are some nicer EXPECT_ / ASSERT_ macros available to
  // compare Protobuf messages (which can display the diff in case of mismatch),
  // we should consider using them.
  // See https://github.com/google/googletest/issues/1761
  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      device, device_from_file));
}

TEST_P(OpenconfigConverterSimpleTest, OcToChassis) {
  openconfig::Device device;
  ASSERT_OK(ReadProtoFromTextFile(oc_config_path, &device));

  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(device);
  ASSERT_OK(ret);

  const ChassisConfig& chassis_config = ret.ConsumeValueOrDie();

  ChassisConfig chassis_config_from_file;
  ASSERT_OK(
      ReadProtoFromTextFile(chassis_config_path, &chassis_config_from_file));

  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      chassis_config, chassis_config_from_file));
}

INSTANTIATE_TEST_SUITE_P(
    ConvertConfig, OpenconfigConverterSimpleTest,
    testing::Values(
        std::make_tuple(
            "stratum/hal/lib/common/testdata/simple_chassis.pb.txt",
            "stratum/hal/lib/common/testdata/simple_oc_device.pb.txt"),
        std::make_tuple(
            "stratum/hal/lib/common/testdata/port_config_params_chassis.pb.txt",
            "stratum/hal/lib/common/testdata/"
            "port_config_params_oc_device.pb.txt")));

TEST(OpenconfigConverterTest, ChassisConfigToOcDevice_VendorConfig) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/vendor_specific_chassis.pb.txt",
      &chassis_config));
  ::util::StatusOr<openconfig::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);

  const openconfig::Device& device = ret.ConsumeValueOrDie();

  oc::Bcm::Chassis::Config vendor_config_from_file;
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/oc_vendor_config.pb.txt",
      &vendor_config_from_file));

  for (auto& component_key : device.component()) {
    auto& component = component_key.component();
    if (component.has_chassis()) {
      auto& chassis = component.chassis();

      oc::Bcm::Chassis::Config vendor_config;
      chassis.vendor_specific().UnpackTo(&vendor_config);
      ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
          vendor_config, vendor_config_from_file));
      break;
    }
  }
}  // OpenconfigConverterTest.ChassisConfigToOcDevice_VendorConfig

TEST(OpenconfigConverterTest, OcDeviceToVendorConfig) {
  // openconfig::Device::ComponentKey -> ChassisConfig with GoogleConfig
  // The ComponentKey includes vendor-specific config

  openconfig::Device device;
  oc::Bcm::Chassis::Config vendor_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/oc_vendor_config.pb.txt",
      &vendor_config));
  openconfig::Device::ComponentKey* component_key = device.add_component();

  component_key->set_name("dummy switch 1");
  component_key->mutable_component()
      ->mutable_chassis()
      ->mutable_vendor_specific()
      ->PackFrom(vendor_config);

  // linecard
  component_key = device.add_component();
  component_key->set_name(":lc-1");
  component_key->mutable_component()->mutable_id()->set_value("1");
  component_key->mutable_component()
      ->mutable_linecard()
      ->mutable_slot_id()
      ->set_value("1");

  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(device);
  const ChassisConfig& chassis_config = ret.ConsumeValueOrDie();
  ChassisConfig chassis_config_from_file;
  ASSERT_OK(ReadProtoFromTextFile(
      "stratum/hal/lib/common/testdata/vendor_specific_chassis.pb.txt",
      &chassis_config_from_file));

  ASSERT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      chassis_config, chassis_config_from_file));
}  // OpenconfigConverterTest.OcDeviceToVendorConfig

#define ASSERT_CONFIG_ERROR(config_class, config_file_path, status_code, \
                            config_func)                                 \
  do {                                                                   \
    config_class the_config;                                             \
    ASSERT_OK(ReadProtoFromTextFile(config_file_path, &the_config));     \
    auto statusor = config_func(the_config);                             \
    ASSERT_FALSE(statusor.ok());                                         \
    ASSERT_EQ(statusor.status().error_code(), status_code);              \
  } while (0)

TEST(OpenconfigConverterTest, InvalidChassisConfigs) {
  ASSERT_CONFIG_ERROR(
      ChassisConfig, "stratum/hal/lib/common/testdata/invalid_speed.pb.txt",
      ERR_INVALID_PARAM, OpenconfigConverter::ChassisConfigToOcDevice);
  ASSERT_CONFIG_ERROR(
      ChassisConfig,
      "stratum/hal/lib/common/testdata/unknown_trunk_member.pb.txt",
      ERR_INVALID_PARAM, OpenconfigConverter::ChassisConfigToOcDevice);
  ASSERT_CONFIG_ERROR(
      ChassisConfig,
      "stratum/hal/lib/common/testdata/unknown_trunk_type.pb.txt",
      ERR_INVALID_PARAM, OpenconfigConverter::ChassisConfigToOcDevice);
}
// OpenconfigConverterTest.InvalidChassisConfigs

TEST(OpenconfigConverterTest, InvalidOcDevice) {
  ASSERT_CONFIG_ERROR(
      openconfig::Device,
      "stratum/hal/lib/common/testdata/invalid_iface_component.pb.txt",
      ERR_INVALID_PARAM, OpenconfigConverter::OcDeviceToChassisConfig);

  ASSERT_CONFIG_ERROR(openconfig::Device,
                      "stratum/hal/lib/common/testdata/invalid_no_node.pb.txt",
                      ERR_INVALID_PARAM,
                      OpenconfigConverter::OcDeviceToChassisConfig);

  ASSERT_CONFIG_ERROR(
      openconfig::Device,
      "stratum/hal/lib/common/testdata/invalid_no_chassis.pb.txt",
      ERR_INVALID_PARAM, OpenconfigConverter::OcDeviceToChassisConfig);

  ASSERT_CONFIG_ERROR(openconfig::Device,
                      "stratum/hal/lib/common/testdata/invalid_oc_speed.pb.txt",
                      ERR_INVALID_PARAM,
                      OpenconfigConverter::OcDeviceToChassisConfig);
}  // OpenconfigConverterTest.InvalidOcDevice

}  // namespace hal
}  // namespace stratum
