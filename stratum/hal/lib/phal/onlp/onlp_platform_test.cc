// Copyright 2020 Open Networking Foundation
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

#include <functional>
#include <iostream>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/lib/macros.h"
// #include "stratum/lib/test_utils/matchers.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

class OnlpPlatformTest : public ::testing::Test {
 public:
 protected:
  void SetUp() override { onlp_ = OnlpWrapper::CreateSingleton(); }

  OnlpWrapper* onlp_;
};

namespace {
TEST_F(OnlpPlatformTest, SfpTest) {
  auto sfp_list = onlp_->GetOidList(ONLP_OID_TYPE_FLAG_SFP);
  ASSERT_OK(sfp_list);
  auto sfps = sfp_list.ConsumeValueOrDie();

  // Check that we can get the SfpHeader of all ports.
  for (size_t i = 0; i < sfps.size(); ++i) {
    auto status = onlp_->GetSfpInfo(sfps[i]);
    ASSERT_OK(status);
    auto sfp = status.ConsumeValueOrDie();
    ASSERT_EQ(sfp.GetId(), i + 1)
        << "Port Id does not match position in OidList.";
    if (!sfp.Present()) {
      EXPECT_EQ(sfp.GetSfpType(), SFP_TYPE_UNKNOWN)
          << "SFP " << sfp.GetId() << " is not present, but still reports "
          << "type " << SfpType_Name(sfp.GetSfpType()) << ".";
    } else {
      // SFP present, more detailed information is available.
      // Check the Onlp structs directly.
      onlp_sfp_info_t onlp_sfp_info = {{sfps[i]}};
      ASSERT_TRUE(ONLP_SUCCESS(onlp_sfp_info_get(sfps[i], &onlp_sfp_info)));
      EXPECT_EQ(onlp_sfp_info.type, onlp_sfp_info.sff.sfp_type)
          << "SFP " << ONLP_OID_ID_GET(onlp_sfp_info.hdr.id)
          << " reports onlp_sfp_info_t.type: " << onlp_sfp_info.type
          << " , but sff_info_s.sfp_type: " << onlp_sfp_info.sff.sfp_type;
    }
  }

  // Tests with physical cableing requirements.
  LOG(INFO) << "The following tests require the following cable setup:\n"
            << "Port 1: 40G, Port 2: 100G, Port 3: empty";
  auto port1 = onlp_->GetSfpInfo(sfps[0]).ConsumeValueOrDie();
  auto port2 = onlp_->GetSfpInfo(sfps[1]).ConsumeValueOrDie();
  auto port3 = onlp_->GetSfpInfo(sfps[2]).ConsumeValueOrDie();

  EXPECT_TRUE(port1.Present());
  EXPECT_TRUE(port2.Present());
  EXPECT_TRUE(!port3.Present());

  EXPECT_NE(port1.GetSfpType(), SFP_TYPE_UNKNOWN);
  EXPECT_NE(port2.GetSfpType(), SFP_TYPE_UNKNOWN);
  EXPECT_EQ(port3.GetSfpType(), SFP_TYPE_UNKNOWN)
      << "SFP Type should not be " << SfpType_Name(port3.GetSfpType())
      << " when nothing is plugged in.";

  EXPECT_NE(port1.GetSfpVendor(), "");
  EXPECT_NE(port2.GetSfpVendor(), "");
  EXPECT_NE(port1.GetSfpModel(), "");
  EXPECT_NE(port2.GetSfpModel(), "");
  EXPECT_NE(port1.GetSfpSerialNumber(), "");
  EXPECT_NE(port2.GetSfpSerialNumber(), "");
}

TEST_F(OnlpPlatformTest, FanTest) {
  auto oid_list = onlp_->GetOidList(ONLP_OID_TYPE_FLAG_FAN);
  ASSERT_OK(oid_list);
  auto fans = oid_list.ConsumeValueOrDie();

  // Check that we can get the SfpHeader of all fans.
  for (size_t i = 0; i < fans.size(); ++i) {
    auto status = onlp_->GetFanInfo(fans[i]);
    ASSERT_OK(status);
    auto fan_info = status.ConsumeValueOrDie();
    ASSERT_EQ(fan_info.GetId(), i + 1)
        << "Fan Id does not match position in OidList.";
    FanCaps caps;
    fan_info.GetCaps(&caps);
    if (caps.get_dir()) {
      EXPECT_NE(fan_info.GetFanDir(), FAN_DIR_UNKNOWN)
          << "Fan " << fan_info.GetId() << " reports no direction";
    }
    if (caps.get_rpm()) {
      EXPECT_NE(fan_info.GetOnlpFan().ValueOrDie()->rpm, 0)
          << "Fan " << fan_info.GetId() << " reports 0 RPM";
    }
    if (caps.get_percentage()) {
      EXPECT_NE(fan_info.GetOnlpFan().ValueOrDie()->percentage, 0)
          << "Fan " << fan_info.GetId() << " reports 0 percentage";
    }
  }
}

// TEST_F(OnlpPlatformTest, SfpTestInteractive) {
//   LOG(INFO) << "foo";
//   char c;
//   std::cin >> c;
//   LOG(INFO) << "bar";
// }

}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
