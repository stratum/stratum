// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_phal.h"

#include <functional>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/onlp/onlp_event_handler_mock.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/lib/macros.h"

DECLARE_int32(onlp_polling_interval_ms);

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {
namespace {

using TransceiverEvent = PhalInterface::TransceiverEvent;

using test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::StrictMock;

static constexpr int kMaxXcvrEventDepth = 256;

class OnlpPhalTest : public ::testing::Test {
 protected:
  void SetUp() override {
    onlp_wrapper_mock_ = absl::make_unique<StrictMock<OnlpWrapperMock>>();
    onlp_sfp_info_t onlp_sfp_info = {};
    onlp_sfp_info.type = ONLP_SFP_TYPE_QSFP;
    onlp_sfp_info.hdr.id = ONLP_SFP_ID_CREATE(1);
    onlp_sfp_info.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
    onlp_sfp_info.sff.module_type = SFF_MODULE_TYPE_40G_BASE_CR4;
    onlp_sfp_info.sff.sfp_type = SFF_SFP_TYPE_QSFP;
    snprintf(onlp_sfp_info.sff.vendor, sizeof(onlp_sfp_info.sff.vendor),
             "sfp-vendor-name");
    SfpInfo sfp1_info(onlp_sfp_info);
    onlp_sfp_info.hdr.id = ONLP_SFP_ID_CREATE(2);
    SfpInfo sfp2_info(onlp_sfp_info);

    std::vector<OnlpOid> oids = {ONLP_SFP_ID_CREATE(1), ONLP_SFP_ID_CREATE(2)};

    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_SFP))
        .WillRepeatedly(Return(std::vector<OnlpOid>(oids)));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_PSU))
        .WillRepeatedly(Return(std::vector<OnlpOid>({})));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_FAN))
        .WillRepeatedly(Return(std::vector<OnlpOid>({})));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_LED))
        .WillRepeatedly(Return(std::vector<OnlpOid>({})));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidList(ONLP_OID_TYPE_FLAG_THERMAL))
        .WillRepeatedly(Return(std::vector<OnlpOid>({})));
    EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(ONLP_SFP_ID_CREATE(1)))
        .WillRepeatedly(Return(sfp1_info));
    EXPECT_CALL(*onlp_wrapper_mock_, GetSfpInfo(117440514))
        .WillRepeatedly(Return(sfp2_info));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(ONLP_SFP_ID_CREATE(1)))
        .WillRepeatedly(Return(sfp1_info));
    EXPECT_CALL(*onlp_wrapper_mock_, GetOidInfo(117440514))
        .WillRepeatedly(Return(sfp2_info));
    EXPECT_CALL(*onlp_wrapper_mock_, GetSfpMaxPortNumber())
        .WillRepeatedly(Return(2));
    // CreateSingleton calls Initialize()
    onlp_phal_ = OnlpPhal::CreateSingleton(onlp_wrapper_mock_.get());

    // Wait a bit so that the Onlp event handler can pick up the present state.
    absl::SleepFor(absl::Milliseconds(FLAGS_onlp_polling_interval_ms + 100));
  }

  void TearDown() override { onlp_phal_->Shutdown(); }

 protected:
  int card_ = 1;
  OnlpPhal* onlp_phal_;
  std::unique_ptr<StrictMock<OnlpWrapperMock>> onlp_wrapper_mock_;
};

TEST_F(OnlpPhalTest, PushChassisConfigSuccess) {
  ChassisConfig config;
  EXPECT_OK(onlp_phal_->PushChassisConfig(config));
}

TEST_F(OnlpPhalTest, RegisterOnlpEventCallbackSuccess) {
  OnlpOid oid;
  auto event_callback = absl::make_unique<OnlpEventCallbackMock>(oid);

  EXPECT_OK(onlp_phal_->RegisterOnlpEventCallback(event_callback.get()));
  // TODO(max): add expectations
}

// TODO(max): add more tests

}  // namespace

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
