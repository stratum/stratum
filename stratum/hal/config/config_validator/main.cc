//
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


// This is the main entry for HAL common modules tests.
#include <string>

#include "absl/strings/str_format.h"
#include "glog/logging.h"
#include "gflags/gflags.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/bcm/bcm.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/glue/logging.h"
#include "gtest/gtest.h"
#include "stratum/lib/utils.h"
#include "stratum/glue/status/status_test_util.h"

#define TO_STR2(x) #x
#define TO_STR(x) TO_STR2(x)

#ifdef PLATFORM
const char* kPlatformName = TO_STR(PLATFORM);
#else
const char* kPlatformName = "DUMMY";
#endif

namespace stratum {
namespace hal {

class ConfigValidator {



};

TEST(ConfigValidator, TestChassisConfig) {
  std::string filename = absl::StrFormat("stratum/hal/config/%s/chassis_config.pb.txt", kPlatformName);
  ChassisConfig chassis_config;
  EXPECT_OK(ReadProtoFromTextFile(filename, &chassis_config));
}

TEST(ConfigValidator, TestPhalConfig) {
  std::string filename = absl::StrFormat("stratum/hal/config/%s/phal_config.pb.txt", kPlatformName);
  PhalInitConfig phal_config;
  EXPECT_OK(ReadProtoFromTextFile(filename, &phal_config));
}

#ifdef BCM_TARGET
TEST(ConfigValidator, TestBcmConfig) {
std::string filename = absl::StrFormat("stratum/hal/config/%s/base_bcm_chassis_map.pb.txt", kPlatformName);
BcmChassisMapList bcm_chassis_map_list;
EXPECT_OK(ReadProtoFromTextFile(filename, &bcm_chassis_map_list));
}
#endif

}  // namespace hal
}  // namespace stratum


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::google::InitGoogleLogging(argv[0]);


  return RUN_ALL_TESTS();
}
