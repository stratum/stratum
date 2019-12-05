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

#include "stratum/hal/lib/phal/onlp/onlpphal.h"

#include <functional>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/glue/status/status_test_util.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"
#include "stratum/lib/macros.h"
// #include "absl/synchronization/mutex.h"
// #include "absl/time/time.h"
#include "absl/memory/memory.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

using TransceiverEvent = PhalInterface::TransceiverEvent;

using stratum::test_utils::StatusIs;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;

static constexpr int kMaxXcvrEventDepth = 256;

class OnlpPhalTest : public ::testing::Test {
 public:
  void SetUp() override {
    // CreateSingleton calls Initialize()
    onlpphal_ = OnlpPhal::CreateSingleton();
  }

  void TearDown() override { onlpphal_->Shutdown(); }

 protected:
  OnlpPhal* onlpphal_;
};

TEST_F(OnlpPhalTest, OnlpPhalRegisterAndUnregisterTransceiverEventWriter) {
  std::shared_ptr<Channel<TransceiverEvent>> channel =
      Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);

  // Create and hand-off ChannelWriter to the PhalInterface.
  auto writer1 = ChannelWriter<TransceiverEvent>::Create(channel);
  auto writer2 = ChannelWriter<TransceiverEvent>::Create(channel);

  // Register writer1
  ::util::StatusOr<int> result = onlpphal_->RegisterTransceiverEventWriter(
      std::move(writer1), PhalInterface::kTransceiverEventWriterPriorityMed);
  EXPECT_TRUE(result.ok());
  int id1 = result.ValueOrDie();
  EXPECT_EQ(id1, 1);

  // Register writer2
  result = onlpphal_->RegisterTransceiverEventWriter(
      std::move(writer2), PhalInterface::kTransceiverEventWriterPriorityHigh);
  EXPECT_TRUE(result.ok());
  int id2 = result.ValueOrDie();
  EXPECT_EQ(id2, 2);

  // Unregister writer1
  EXPECT_OK(onlpphal_->UnregisterTransceiverEventWriter(id1));

  // Unregister writer2
  EXPECT_OK(onlpphal_->UnregisterTransceiverEventWriter(id2));

  onlpphal_->Shutdown();
}

TEST_F(OnlpPhalTest, OnlpPhalWriteTransceiverEvent) {
  std::shared_ptr<Channel<TransceiverEvent>> channel =
      Channel<TransceiverEvent>::Create(kMaxXcvrEventDepth);

  // Create and hand-off ChannelWriter to the PhalInterface.
  auto writer1 = ChannelWriter<TransceiverEvent>::Create(channel);
  auto writer2 = ChannelWriter<TransceiverEvent>::Create(channel);

  // Register writer1
  ::util::StatusOr<int> result = onlpphal_->RegisterTransceiverEventWriter(
      std::move(writer1), PhalInterface::kTransceiverEventWriterPriorityMed);
  EXPECT_TRUE(result.ok());
  int id1 = result.ValueOrDie();
  EXPECT_EQ(id1, 1);

  // Register writer2
  result = onlpphal_->RegisterTransceiverEventWriter(
      std::move(writer2), PhalInterface::kTransceiverEventWriterPriorityHigh);
  EXPECT_TRUE(result.ok());
  int id2 = result.ValueOrDie();
  EXPECT_EQ(id2, 2);

  // Write Transceiver Events
  TransceiverEvent event;
  event.slot = 1;
  event.port = 3;
  event.state = HW_STATE_PRESENT;  // 2

  EXPECT_OK(onlpphal_->WriteTransceiverEvent(event));

  // Unregister writer1
  EXPECT_OK(onlpphal_->UnregisterTransceiverEventWriter(id1));

  // Unregister writer2
  EXPECT_OK(onlpphal_->UnregisterTransceiverEventWriter(id2));
}

TEST_F(OnlpPhalTest, DISABLED_OnlpPhalGetFrontPanelPortInfo) {
  // SFP 1
  FrontPanelPortInfo fp_port_info1{};
  EXPECT_OK(onlpphal_->GetFrontPanelPortInfo(0, 111, &fp_port_info1));
  EXPECT_EQ(fp_port_info1.physical_port_type(), PHYSICAL_PORT_TYPE_SFP_CAGE);
  EXPECT_EQ(fp_port_info1.media_type(), MEDIA_TYPE_SFP);
  EXPECT_EQ(fp_port_info1.vendor_name(), "test_sfp_vendor");
  // EXPECT_EQ(fp_port_info1.get_part_number(), 6);
  EXPECT_EQ(fp_port_info1.serial_number(), "test_sfp_serial");

  // SFP 2
  FrontPanelPortInfo fp_port_info2{};
  EXPECT_OK(onlpphal_->GetFrontPanelPortInfo(0, 222, &fp_port_info2));
  EXPECT_EQ(fp_port_info2.physical_port_type(), PHYSICAL_PORT_TYPE_SFP_CAGE);
  EXPECT_EQ(fp_port_info2.media_type(), MEDIA_TYPE_SFP);
  EXPECT_EQ(fp_port_info2.vendor_name(), "sfp_vendor_222");
  // EXPECT_EQ(fp_port_info2.get_part_number(), 6);
  EXPECT_EQ(fp_port_info2.serial_number(), "sfp_serial_222");
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
