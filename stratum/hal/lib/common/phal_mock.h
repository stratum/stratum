// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_COMMON_PHAL_MOCK_H_
#define STRATUM_HAL_LIB_COMMON_PHAL_MOCK_H_

#include <memory>

#include "stratum/hal/lib/common/phal_interface.h"
#include "gmock/gmock.h"

namespace stratum {
namespace hal {

class PhalMock : public PhalInterface {
 public:
  MOCK_METHOD1(PushChassisConfig, ::util::Status(const ChassisConfig& config));
  MOCK_METHOD1(VerifyChassisConfig,
               ::util::Status(const ChassisConfig& config));
  MOCK_METHOD0(Shutdown, ::util::Status());
  MOCK_METHOD2(RegisterTransceiverEventWriter,
               ::util::StatusOr<int>(
                   std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
                   int priority));
  MOCK_METHOD1(UnregisterTransceiverEventWriter, ::util::Status(int id));
  MOCK_METHOD3(GetFrontPanelPortInfo,
               ::util::Status(int slot, int port,
                              FrontPanelPortInfo* fp_port_info));
  MOCK_METHOD3(GetOpticalTransceiverInfo,
               ::util::Status(int module, int network_interface,
                              OpticalTransceiverInfo* ot_info));
  MOCK_METHOD3(SetOpticalTransceiverInfo,
               ::util::Status(int module, int network_interface,
                              const OpticalTransceiverInfo& ot_info));
  MOCK_METHOD5(SetPortLedState, ::util::Status(int slot, int port, int channel,
                                               LedColor color, LedState state));
  MOCK_METHOD3(RegisterSfpConfigurator,
    ::util::Status(int slot, int port,
      ::stratum::hal::phal::SfpConfigurator* configurator));
};

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_PHAL_MOCK_H_
