#include "platforms/networking/hercules/hal/lib/common/openconfig_converter.h"

#include "platforms/networking/hercules/lib/test_utils/matchers.h"
#include "platforms/networking/hercules/lib/utils.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace google {
namespace hercules {
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
}  // namespace hercules
}  // namespace google
