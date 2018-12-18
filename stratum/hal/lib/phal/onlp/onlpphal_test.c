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

#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/hal/lib/phal/onlp/onlphal.h"

#include <functional>
#include <vector>
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/lib/macros.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
//#include "absl/synchronization/mutex.h"
//#include "absl/time/time.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using TransceiverEvent = PhalInterface::TransceiverEvent;

using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using stratum::test_utils::StatusIs;

static constexpr int kMaxXcvrEventDepth = 256;

class OnlphalTest : public ::testing::Test {
 public:
  void SetUp() override {
    onlphal_ = Onlphal::CreateSingleton();

    //const ChassisConfig config;
    //onlphal_->PushChassisConfig(config);

    onlphal_->InitializeOnlpInterface();
    onlphal_->InitializeOnlpEventHandler();
    onlphal_->initialized_ = true;
  }

  void TearDown() override {
    onlphal_->Shutdown();
  }

  ::util::Status InitializeDataSources() { 
    return onlphal_->InitializeOnlpOids(); 
  }

 protected:
  Onlphal* onlphal_;
};


TEST_F(OnlphalTest, OnlphalRegisterAndUnregisterTransceiverEventWriter) {

  std::shared_ptr<Channel<TransceiverEvent>> channel =
      Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);

  // Create and hand-off ChannelWriter to the PhalInterface.
  auto writer1 = ChannelWriter<TransceiverEvent>::Create(channel);
  auto writer2 = ChannelWriter<TransceiverEvent>::Create(channel);

  // Register writer1
  ::util::StatusOr<int> result = onlphal_->RegisterTransceiverEventWriter(
        std::move(writer1) ,PhalInterface::kTransceiverEventWriterPriorityMed);
  EXPECT_TRUE(result.ok());
  int id1 = result.ValueOrDie();
  EXPECT_EQ(id1, 1);

  // Register writer2
  result = onlphal_->RegisterTransceiverEventWriter(
        std::move(writer2) ,PhalInterface::kTransceiverEventWriterPriorityHigh);
  EXPECT_TRUE(result.ok());
  int id2 = result.ValueOrDie();
  EXPECT_EQ(id2, 2);

  // Unregister writer1
  EXPECT_OK(onlphal_->UnregisterTransceiverEventWriter(id1));

  // Unregister writer2
  EXPECT_OK(onlphal_->UnregisterTransceiverEventWriter(id2));

  onlphal_->Shutdown();
}

TEST_F(OnlphalTest, OnlphalWriteTransceiverEvent) {
  std::shared_ptr<Channel<TransceiverEvent>> channel =
      Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);

  // Create and hand-off ChannelWriter to the PhalInterface.
  auto writer1 = ChannelWriter<TransceiverEvent>::Create(channel);
  auto writer2 = ChannelWriter<TransceiverEvent>::Create(channel);

  // Register writer1
  ::util::StatusOr<int> result = onlphal_->RegisterTransceiverEventWriter(
        std::move(writer1) ,PhalInterface::kTransceiverEventWriterPriorityMed);
  EXPECT_TRUE(result.ok());
  int id1 = result.ValueOrDie();
  EXPECT_EQ(id1, 1);

  // Register writer2
  result = onlphal_->RegisterTransceiverEventWriter(
        std::move(writer2) ,PhalInterface::kTransceiverEventWriterPriorityHigh);
  EXPECT_TRUE(result.ok());
  int id2 = result.ValueOrDie();
  EXPECT_EQ(id2, 2);

  // Write Transceiver Events
  TransceiverEvent event;
  event.slot = 1;
  event.port = 3;
  event.state = HW_STATE_PRESENT; // 2

  EXPECT_OK(onlphal_->WriteTransceiverEvent(event));

  // Unregister writer1
  EXPECT_OK(onlphal_->UnregisterTransceiverEventWriter(id1));

  // Unregister writer2
  EXPECT_OK(onlphal_->UnregisterTransceiverEventWriter(id2));
}

TEST_F(OnlphalTest, OnlphalGetFrontPanelPortInfo) {
  // SFP 1
/*
  FrontPanelPortInfo fp_port_info1{};
  EXPECT_OK(onlphal_->GetFrontPanelPortInfo(0, 111, &fp_port_info1));
  EXPECT_EQ(fp_port_info1.get_physical_port_type(), PHYSICAL_PORT_TYPE_SFP_CAGE);
  EXPECT_EQ(fp_port_info1.get_media_type(), MEDIA_TYPE_SFP);
  EXPECT_EQ(fp_port_info1.get_vendor_name(), "test_sfp_vendor");
  //EXPECT_EQ(fp_port_info1.get_part_number(), 6);
  EXPECT_EQ(fp_port_info1.get_serial_number(), "test_sfp_serial");
*/

  // SFP 2
/*
  FrontPanelPortInfo fp_port_info2{};
  EXPECT_OK(onlphal_->GetFrontPanelPortInfo(0, 222, &fp_port_info2));
  EXPECT_EQ(fp_port_info2.get_physical_port_type(), PHYSICAL_PORT_TYPE_SFP_CAGE);
  EXPECT_EQ(fp_port_info2.get_media_type(), MEDIA_TYPE_SFP);
  EXPECT_EQ(fp_port_info2.get_vendor_name(), "sfp_vendor_222");
  //EXPECT_EQ(fp_port_info2.get_part_number(), 6);
  EXPECT_EQ(fp_port_info2.get_serial_number(), "sfp_serial_222");
*/
}

#if 0
TEST_F(OnlphalTest, OnlphalInitializeDataSources) {
  // Mocking sequence:
  // 1. call wrapper's GetOids(ONLP_OID_TYPE_SFP)
  // 2. for each Oid,
  //    call SfpDataSource::Make() -- which trigger wrapper's 
  //           1 GetOidInfo() and 2 GetSfpInfo() calls

/*
  // Mock calls for getting all SFP OIDs
  std::vector<OnlpOid> fake_oids;
  OnlpOid oid1 = ((7) << 24) | (111);
  OnlpOid oid2 = ((7) << 24) | (222);
  fake_oids.push_back(oid1);
  fake_oids.push_back(oid2);
  EXPECT_CALL(*mock_wrapper_,
      GetOidLists(ONLP_OID_TYPE_SFP)).WillOnce(Return(fake_oids));

  // Mock calls for creating SFPDataSource
  // SFP 1
  onlp_oid_hdr_t mock_oid_info1;
  mock_oid_info1.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*mock_wrapper_, GetOidInfo(oid1))
      .WillOnce(Return(OidInfo(mock_oid_info1)));

  onlp_sfp_info_t mock_sfp_info1 = {};
  mock_sfp_info1.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info1.type = ONLP_SFP_TYPE_SFP;
  mock_sfp_info1.sff.sfp_type = SFF_SFP_TYPE_SFP;
  mock_sfp_info1.sff.module_type = SFF_MODULE_TYPE_1G_BASE_SX;
  mock_sfp_info1.sff.caps = SFF_MODULE_CAPS_F_1G;
  mock_sfp_info1.sff.length = 100;

  strncpy(mock_sfp_info1.sff.length_desc, "test_cable_len",
              sizeof(mock_sfp_info1.sff.length_desc));
  strncpy(mock_sfp_info1.sff.vendor, "test_sfp_vendor",
              sizeof(mock_sfp_info1.sff.vendor));
  strncpy(mock_sfp_info1.sff.model, "test_sfp_model",
              sizeof(mock_sfp_info1.sff.model));
  strncpy(mock_sfp_info1.sff.serial, "test_sfp_serial",
              sizeof(mock_sfp_info1.sff.serial));

  SffDomInfo *mock_sfp_dom_info1 = &mock_sfp_info1.dom;
  mock_sfp_dom_info1->temp = 123;
  mock_sfp_dom_info1->nchannels = 1;
  mock_sfp_dom_info1->voltage = 234;
  mock_sfp_dom_info1->channels[0].tx_power = 1111;
  mock_sfp_dom_info1->channels[0].rx_power = 2222;
  mock_sfp_dom_info1->channels[0].bias_cur = 3333;

  // SFP 2
  onlp_oid_hdr_t mock_oid_info2;
  mock_oid_info2.status = ONLP_OID_STATUS_FLAG_PRESENT;
  EXPECT_CALL(*mock_wrapper_, GetOidInfo(oid2))
      .WillOnce(Return(OidInfo(mock_oid_info2)));

  onlp_sfp_info_t mock_sfp_info2 = {};
  mock_sfp_info2.hdr.status = ONLP_OID_STATUS_FLAG_PRESENT;
  mock_sfp_info2.type = ONLP_SFP_TYPE_SFP;
  mock_sfp_info2.sff.sfp_type = SFF_SFP_TYPE_SFP;
  mock_sfp_info2.sff.module_type = SFF_MODULE_TYPE_1G_BASE_SX;
  mock_sfp_info2.sff.caps = SFF_MODULE_CAPS_F_1G;
  mock_sfp_info2.sff.length = 200;

  strncpy(mock_sfp_info2.sff.length_desc, "cable_len_200",
              sizeof(mock_sfp_info2.sff.length_desc));
  strncpy(mock_sfp_info2.sff.vendor, "sfp_vendor_222",
              sizeof(mock_sfp_info2.sff.vendor));
  strncpy(mock_sfp_info2.sff.model, "sfp_model_222",
              sizeof(mock_sfp_info2.sff.model));
  strncpy(mock_sfp_info2.sff.serial, "sfp_serial_222",
              sizeof(mock_sfp_info2.sff.serial));

  SffDomInfo *mock_sfp_dom_info2 = &mock_sfp_info2.dom;
  mock_sfp_dom_info2->temp = 456;
  mock_sfp_dom_info2->nchannels = 1;
  mock_sfp_dom_info2->voltage = 789;
  mock_sfp_dom_info2->channels[0].tx_power = 4444;
  mock_sfp_dom_info2->channels[0].rx_power = 5555;
  mock_sfp_dom_info2->channels[0].bias_cur = 6666;

  EXPECT_CALL(*mock_wrapper_, GetSfpInfo(oid1))
      .WillRepeatedly(Return(SfpInfo(mock_sfp_info1)));
  EXPECT_CALL(*mock_wrapper_, GetSfpInfo(oid2))
      .WillRepeatedly(Return(SfpInfo(mock_sfp_info2)));
*/

  // Initialize all data sources - currently only SFPDataSource
  EXPECT_OK(InitializeDataSources());
}
#endif

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
