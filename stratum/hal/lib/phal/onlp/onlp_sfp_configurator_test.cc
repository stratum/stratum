// Copyright 2019 Dell EMC
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

#include "stratum/hal/lib/phal/onlp/onlp_sfp_configurator.h"

// #include <fstream>
// #include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/adapter.h"
#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler_mock.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/lib/test_utils/matchers.h"
#include "stratum/lib/utils.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using ::stratum::test_utils::StatusIs;
using test_utils::EqualsProto;
using ::testing::_;
using ::testing::A;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

class OnlpSfpConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_OK(SetupAttributeGroup());

    // Create switch configurator
    onlp_wrapper_mock_ = absl::make_unique<StrictMock<OnlpWrapperMock>>();
    onlp_sfp_info_t onlp_sfp_info = {};
    SfpInfo sfp_info(onlp_sfp_info);

    EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(_))
        .WillRepeatedly(Return(::util::StatusOr<OidInfo>(sfp_info)));

    EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(_))
        .WillRepeatedly(Return(::util::StatusOr<SfpInfo>(sfp_info)));

    onlp_event_handler_ =
        absl::make_unique<OnlpEventHandlerMock>(onlp_wrapper_mock_.get());

    auto cache =
        CachePolicyFactory::CreateInstance(CachePolicyConfig::NO_CACHE, 0)
            .ConsumeValueOrDie();

    auto onlp_sfp_datasource =
        OnlpSfpDataSource::Make(port_, onlp_wrapper_mock_.get(), cache)
            .ConsumeValueOrDie();

    onlp_sfp_configurator_ =
        OnlpSfpConfigurator::Make(onlp_sfp_datasource, sfp_, port_)
            .ConsumeValueOrDie();
  }

  ::util::Status SetupAttributeGroup() {
    root_ = AttributeGroup::From(PhalDB::descriptor());
    auto mutable_root = root_->AcquireMutable();

    // Create fake card[0]/port[0]/transceiver
    ASSIGN_OR_RETURN(auto card, mutable_root->AddRepeatedChildGroup("cards"));
    auto mutable_card = card->AcquireMutable();
    ASSIGN_OR_RETURN(auto port, mutable_card->AddRepeatedChildGroup("ports"));
    auto mutable_port = port->AcquireMutable();
    ASSIGN_OR_RETURN(sfp_, mutable_port->AddChildGroup("transceiver"));

    return ::util::OkStatus();
  }

  int port_ = 1;
  std::unique_ptr<OnlpEventHandlerMock> onlp_event_handler_;
  std::unique_ptr<OnlpSfpConfigurator> onlp_sfp_configurator_;
  std::unique_ptr<AttributeGroup> root_;
  std::unique_ptr<StrictMock<OnlpWrapperMock>> onlp_wrapper_mock_;
  AttributeGroup* sfp_;
};

namespace {

TEST_F(OnlpSfpConfiguratorTest, HandlesOidStatusChangePresent) {
  onlp_oid_hdr_t fake_oid = {};
  fake_oid.id = port_;
  fake_oid.status = ONLP_OID_STATUS_FLAG_PRESENT;

  EXPECT_OK(onlp_sfp_configurator_->HandleOidStatusChange(OidInfo(fake_oid)));

  auto mutable_sfp_ = sfp_->AcquireReadable();
  ASSERT_TRUE(mutable_sfp_);

  auto hardware_state =
      mutable_sfp_->GetAttribute("hardware_state").ConsumeValueOrDie();
  ASSERT_TRUE(hardware_state);

  auto state =
      hardware_state->ReadValue<const google::protobuf::EnumValueDescriptor*>();
  ASSERT_OK(state);
  EXPECT_EQ(state.ValueOrDie()->number(), HwState::HW_STATE_PRESENT);

  // TODO(max): Test remaining attributes.
}

TEST_F(OnlpSfpConfiguratorTest, IsRegisterableAsOnlpEventCallback) {
  EXPECT_OK(
      onlp_event_handler_->RegisterEventCallback(onlp_sfp_configurator_.get()));
}

}  // namespace
}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
