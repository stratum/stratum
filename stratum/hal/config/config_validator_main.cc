// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <string>

#include "absl/strings/str_format.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {

class ConfigValidator {};

TEST(ConfigValidator, TestChassisConfig) {
  std::string filename = absl::StrFormat(
      "stratum/hal/config/%s/chassis_config.pb.txt", STRINGIFY(PLATFORM));
  ChassisConfig chassis_config;
  EXPECT_OK(ReadProtoFromTextFile(filename, &chassis_config));
}

#ifndef SIM_TARGET
TEST(ConfigValidator, TestPhalConfig) {
  std::string filename = absl::StrFormat(
      "stratum/hal/config/%s/phal_config.pb.txt", STRINGIFY(PLATFORM));
  PhalInitConfig phal_config;
  EXPECT_OK(ReadProtoFromTextFile(filename, &phal_config));
}
#endif

#ifdef BCM_TARGET
TEST(ConfigValidator, TestBcmConfig) {
  std::string filename = absl::StrFormat(
      "stratum/hal/config/%s/base_bcm_chassis_map.pb.txt", STRINGIFY(PLATFORM));
  BcmChassisMapList bcm_chassis_map_list;
  EXPECT_OK(ReadProtoFromTextFile(filename, &bcm_chassis_map_list));
}
#endif

#ifdef TOFINO_TARGET
TEST(ConfigValidator, TestSdePortmap) {
  std::string filename = absl::StrFormat("stratum/hal/config/%s/port_map.json",
                                         STRINGIFY(PLATFORM));
  std::string port_map_content;
  ASSERT_OK(ReadFileToString(filename, &port_map_content));
  nlohmann::json port_map =
      nlohmann::json::parse(port_map_content, nullptr, false);
  EXPECT_FALSE(port_map.is_discarded()) << "Failed to parse port_map as JSON.";
}
#endif

}  // namespace hal
}  // namespace stratum
