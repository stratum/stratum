/*
 * Copyright 2019 Dell EMC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef STRATUM_HAL_LIB_PHAL_ONLP_SFP_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_SFP_CONFIGURATOR_H_

#include <memory>

#include "stratum/hal/lib/phal/db.pb.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"
#include "stratum/hal/lib/phal/sfp_configurator.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpSfpConfigurator : public SfpConfigurator {
 public:
    ~OnlpSfpConfigurator() = default;

    // Make a new OnlpSfpConfigurator
    static ::util::StatusOr<std::unique_ptr<OnlpSfpConfigurator>>
        Make(int card_id, int port_id, int slot, int port,
             std::shared_ptr<OnlpSfpDataSource> datasource,
             AttributeGroup* sfp_group,
             OnlpInterface* onlp_interface);

    // Getters
    int GetCardId() { return card_id_; }
    int GetPortId() { return port_id_; }
    int GetSlot() { return slot_; }
    int GetPort() { return port_; }

    // Handle sfp state changes coming from onlp
    ::util::Status HandleEvent(HwState state) override;

    // Functions to add/remove sfp
    ::util::Status AddSfp();
    ::util::Status RemoveSfp();
    ::util::Status AddChannel(int id, AttributeGroup* channel);
    ::util::Status RemoveChannel(int id, AttributeGroup* channel);
    std::shared_ptr<OnlpSfpDataSource> GetSfpDataSource() const
        { return datasource_; }

 private:
    // OnlpSfpConfigurator() = delete;
    OnlpSfpConfigurator() {}
    OnlpSfpConfigurator(int card_id, int port_id, int slot, int port,
        std::shared_ptr<OnlpSfpDataSource> datasource,
        AttributeGroup* sfp_group, OnlpInterface* onlp_interface);

    int card_id_;
    int port_id_;
    int slot_;
    int port_;
    PhalCardConfig::Port config_;
    std::shared_ptr<OnlpSfpDataSource> datasource_;
    AttributeGroup* sfp_group_;      // Pointer to our attribute group
    OnlpInterface* onlp_interface_;
    int sfp_channel_count_;

    // Mutex lock for protecting the internal state when config is pushed or the
    // class is initialized so that other threads do not access the state while
    // the are being changed.
    mutable absl::Mutex config_lock_;

    // Determine if Sfp has been added (i.e. initialized).
    bool initialized_ GUARDED_BY(config_lock_) = false;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_SFP_CONFIGURATOR_H_
