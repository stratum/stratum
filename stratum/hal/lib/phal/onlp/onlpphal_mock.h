// Copyright 2019 Dell EMC
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

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLPPHAL_MOCK_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLPPHAL_MOCK_H_

#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper_mock.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpPhalMock : public PhalInterface {
 public:
    ~OnlpPhalMock() override;

    ::util::Status Initialize();
    ::util::Status InitializeOnlpInterface();

    // PhalInterface public methods
    //::util::Status PushChassisConfig(const ChassisConfig& config) override {};
    MOCK_METHOD1(PushChassisConfig, 
       ::util::Status(const ChassisConfig& config) override);
    MOCK_METHOD1(VerifyChassisConfig, 
       ::util::Status(const ChassisConfig& config));
    MOCK_METHOD0(Shutdown, ::util::Status());
    MOCK_METHOD2(RegisterTransceiverEventWriter, ::util::StatusOr<int>(
       std::unique_ptr<ChannelWriter<TransceiverEvent>> writer,
       int priority));
    MOCK_METHOD1(UnregisterTransceiverEventWriter,
        ::util::Status(int id));
    MOCK_METHOD3(GetFrontPanelPortInfo,
        ::util::Status(int slot, int port, FrontPanelPortInfo* fp_port_info));
    MOCK_METHOD5(SetPortLedState,
        ::util::Status(int slot, int port, int channel, 
                       LedColor color, LedState state));
    MOCK_METHOD3(RegisterSfpConfigurator,
        ::util::Status(int slot, int port, SfpConfigurator* configurator));

    // Need this function to grab onlp_interface
    MockOnlpWrapper* GetOnlpInterface() { 
        return (MockOnlpWrapper *)onlp_interface_.get(); 
    };

    // Creates the singleton instance. Expected to be called once to initialize
    // the instance.
    static OnlpPhalMock* CreateSingleton() LOCKS_EXCLUDED(config_lock_);

 private:
    friend class OnlpSwitchConfiguratorTest;
    friend class OnlpSfpConfiguratorTest;

    OnlpPhalMock();

    // Internal mutex lock for protecting the internal maps and initializing the
    // singleton instance.
    static absl::Mutex init_lock_;

    // The singleton instance.
    static OnlpPhalMock* singleton_ GUARDED_BY(init_lock_);

    // Determines if PHAL is fully initialized.
    bool initialized_ = false GUARDED_BY(config_lock_);

    std::unique_ptr<OnlpInterface> onlp_interface_;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum
#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLPPHAL_MOCK_H_
