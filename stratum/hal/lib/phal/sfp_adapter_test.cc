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

#include "stratum/hal/lib/phal/sfp_adapter.h"

#include <functional>
#include <vector>

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
// #include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/lib/macros.h"

DECLARE_int32(onlp_polling_interval_ms);

namespace stratum {
namespace hal {
namespace phal {

// TODO(max)
#if 0

using TransceiverEvent = PhalInterface::TransceiverEvent;

using stratum::test_utils::StatusIs;
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

// TEST_F(OnlpPhalTest, OnlpPhalRegisterAndUnregisterTransceiverEventWriter) {
//   std::shared_ptr<Channel<TransceiverEvent>> channel =
//       Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);

//   // Create and hand-off ChannelWriter to the PhalInterface.
//   auto writer1 = ChannelWriter<TransceiverEvent>::Create(channel);
//   auto writer2 = ChannelWriter<TransceiverEvent>::Create(channel);

//   // Register writer1
//   ::util::StatusOr<int> result = onlp_phal_->RegisterTransceiverEventWriter(
//       std::move(writer1), PhalInterface::kTransceiverEventWriterPriorityMed);
//   EXPECT_TRUE(result.ok());
//   int id1 = result.ValueOrDie();
//   EXPECT_EQ(id1, 1);

//   // Register writer2
//   result = onlp_phal_->RegisterTransceiverEventWriter(
//       std::move(writer2), PhalInterface::kTransceiverEventWriterPriorityHigh);
//   EXPECT_TRUE(result.ok());
//   int id2 = result.ValueOrDie();
//   EXPECT_EQ(id2, 2);

//   // Unregister writer1
//   EXPECT_OK(onlp_phal_->UnregisterTransceiverEventWriter(id1));

//   // Unregister writer2
//   EXPECT_OK(onlp_phal_->UnregisterTransceiverEventWriter(id2));
// }

TEST_F(OnlpPhalTest, OnlpPhalGetFrontPanelPortInfo) {
  // SFP 1
  FrontPanelPortInfo fp_port_info1{};
  EXPECT_OK(onlp_phal_->GetFrontPanelPortInfo(card_, 1, &fp_port_info1));
  EXPECT_EQ(fp_port_info1.physical_port_type(), PHYSICAL_PORT_TYPE_QSFP_CAGE);
  EXPECT_EQ(fp_port_info1.media_type(), MEDIA_TYPE_QSFP_COPPER);
  EXPECT_EQ(fp_port_info1.vendor_name(), "sfp-vendor-name");
  // EXPECT_EQ(fp_port_info1.get_part_number(), 6);
  // EXPECT_EQ(fp_port_info1.serial_number(), "test_sfp_serial");

  // SFP 2
  FrontPanelPortInfo fp_port_info2{};
  EXPECT_OK(onlp_phal_->GetFrontPanelPortInfo(card_, 2, &fp_port_info2));
  EXPECT_EQ(fp_port_info2.physical_port_type(), PHYSICAL_PORT_TYPE_QSFP_CAGE);
  EXPECT_EQ(fp_port_info2.media_type(), MEDIA_TYPE_QSFP_COPPER);
  EXPECT_EQ(fp_port_info2.vendor_name(), "sfp-vendor-name");
  // EXPECT_EQ(fp_port_info2.get_part_number(), 6);
  // EXPECT_EQ(fp_port_info2.serial_number(), "sfp_serial_222");
}

#endif

}  // namespace phal
}  // namespace hal
}  // namespace stratum
