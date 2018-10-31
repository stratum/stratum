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

#include "stratum/hal/lib/common/openconfig_converter.h"

#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace stratum {

namespace hal {

TEST(OpenconfigConverterTest, ChassisConfigToOcDevice_SampleGeneric Tomahawk100gConfig) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/"
      "testdata/test_chassis_config_generic_tomahawk_100g_hercules.pb.txt",
      &chassis_config));
  ::util::StatusOr<oc::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest, ChassisConfigToOcDevice_SampleGeneric Tomahawk40g100gConfig) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_chassis_config_generic_tomahawk_40g_100g_hercules.pb.txt",
      &chassis_config));
  ::util::StatusOr<oc::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest,
     ChassisConfigToOcDevice_SampleGeneric Trident240gConfig) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_chassis_config_generic_trident2_40g_hercules.pb.txt",
      &chassis_config));
  ::util::StatusOr<oc::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest, ChassisConfigToOcDevice_SampleGeneric Trident240gConfig) {
  ChassisConfig chassis_config;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_chassis_config_generic_trident2_40g_hercules.pb.txt",
      &chassis_config));
  ::util::StatusOr<oc::Device> ret =
      OpenconfigConverter::ChassisConfigToOcDevice(chassis_config);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest, OcDeviceToChassisConfig_SampleGeneric Tomahawk40g100gConfig) {
  oc::Device oc_device;
  ASSERT_OK(
      ReadProtoFromTextFile("platforms/networking/hercules/hal/lib/common/"
                            "testdata/test_oc_device_generic_tomahawk_100g_hercules.pb.txt",
                            &oc_device));
  ASSERT_OK(OpenconfigConverter::ValidateOcDeviceProto(oc_device));
  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(oc_device);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest, OcDeviceToChassisConfig_SampleGeneric Tomahawk100gConfig) {
  oc::Device oc_device;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_oc_device_generic_tomahawk_40g_100g_hercules.pb.txt",
      &oc_device));
  ASSERT_OK(OpenconfigConverter::ValidateOcDeviceProto(oc_device));
  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(oc_device);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest,
     OcDeviceToChassisConfig_SampleGeneric Trident240gConfig) {
  oc::Device oc_device;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_oc_device_generic_trident2_40g_hercules.pb.txt",
      &oc_device));
  ASSERT_OK(OpenconfigConverter::ValidateOcDeviceProto(oc_device));
  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(oc_device);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

TEST(OpenconfigConverterTest, OcDeviceToChassisConfig_SampleGeneric Trident240gConfig) {
  oc::Device oc_device;
  ASSERT_OK(ReadProtoFromTextFile(
      "platforms/networking/hercules/hal/lib/common/testdata/"
      "test_oc_device_generic_trident2_40g_hercules.pb.txt",
      &oc_device));
  ASSERT_OK(OpenconfigConverter::ValidateOcDeviceProto(oc_device));
  ::util::StatusOr<ChassisConfig> ret =
      OpenconfigConverter::OcDeviceToChassisConfig(oc_device);
  ASSERT_OK(ret);
  // TODO(aghaffar): Check the output.
}

}  // namespace hal

}  // namespace stratum
