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


#include <iostream>
#include <fstream>
#include <string>

#include "stratum/hal/lib/phal/onlp/sfp_configurator.h"
#include "stratum/hal/lib/common/common.pb.h"

using namespace std;  // NOLINT
using namespace google::protobuf::util;  // NOLINT

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

OnlpSfpConfigurator::OnlpSfpConfigurator(int id,
    std::shared_ptr<OnlpSfpDataSource>datasource,
    AttributeGroup* sfp_group, OnlpInterface* onlp_interface)
      : id_(id),
        datasource_(ABSL_DIE_IF_NULL(datasource)),
        sfp_group_(ABSL_DIE_IF_NULL(sfp_group)),
        onlp_interface_(ABSL_DIE_IF_NULL(onlp_interface)) {
    // lock us so we can modify
    auto mutable_sfp = sfp_group_->AcquireMutable();

    // Ok, now go add the sfp attributes
    mutable_sfp->AddAttribute("id", datasource_->GetSfpId());
    mutable_sfp->AddAttribute("description", datasource_->GetSfpDesc());
    mutable_sfp->AddAttribute("hardware_state",
        datasource_->GetSfpHardwareState());
}

::util::StatusOr<std::unique_ptr<OnlpSfpConfigurator>>
OnlpSfpConfigurator::Make(int id,
    std::shared_ptr<OnlpSfpDataSource>datasource,
    AttributeGroup* sfp_group,
    OnlpInterface* onlp_interface) {

    return absl::WrapUnique(
            new OnlpSfpConfigurator(id, datasource, sfp_group, onlp_interface));
}

::util::Status OnlpSfpConfigurator::HandleEvent(HwState state) {
    // Check SFP state
    switch (state) {
      // Add SFP attributes
      case HW_STATE_PRESENT:
        RETURN_IF_ERROR(AddSfp());
        break;

      // Remove SFP attributes
      case HW_STATE_NOT_PRESENT:
        RETURN_IF_ERROR(RemoveSfp());
        break;

      default:
        RETURN_ERROR() << "Unknown SFP event state " << state << ".";
    }

    return ::util::OkStatus();
}

// Add an Sfp transceiver
// Note: you can't hold a mutable lock on parent attribute group
//       while also holding a mutable lock on a child attribute group
//       and adding attributes (via the AddAttribute call) to the child
//       attribute group.  Could be a bug, but for now the code below
//       works around the problem.
::util::Status OnlpSfpConfigurator::AddSfp() {
    // Make sure we don't already have a datasource added to the DB
    absl::WriterMutexLock l(&config_lock_);
    if (initialized_) {
        RETURN_ERROR() << "sfp id " << id_ << " already added";
    }

    // lock us so we can modify
    auto mutable_sfp = sfp_group_->AcquireMutable();

    mutable_sfp->AddAttribute("media_type", datasource_->GetSfpMediaType());
    mutable_sfp->AddAttribute("connector_type",
        datasource_->GetSfpType());
    mutable_sfp->AddAttribute("module_type", datasource_->GetSfpModuleType());
    mutable_sfp->AddAttribute("cable_length", datasource_->GetSfpCableLength());
    mutable_sfp->AddAttribute("cable_length_desc",
        datasource_->GetSfpCableLengthDesc());
    mutable_sfp->AddAttribute("temperature", datasource_->GetSfpTemperature());
    mutable_sfp->AddAttribute("vcc", datasource_->GetSfpVoltage());
    mutable_sfp->AddAttribute("channel_count",
        datasource_->GetSfpChannelCount());

    // Get HardwareInfo DB group
    ASSIGN_OR_RETURN(auto info, mutable_sfp->AddChildGroup("info"));

    // release sfp lock & acquire info lock
    mutable_sfp = nullptr;
    auto mutable_info = info->AcquireMutable();

    // Ok, now go add the info attributes
    mutable_info->AddAttribute("mfg_name", datasource_->GetSfpVendor());
    mutable_info->AddAttribute("part_no", datasource_->GetSfpModel());
    mutable_info->AddAttribute("serial_no", datasource_->GetSfpSerialNumber());

    // release info lock
    mutable_info = nullptr;

    // Get SfpModuleCaps DB group
    mutable_sfp = sfp_group_->AcquireMutable();
    ASSIGN_OR_RETURN(auto caps,
        mutable_sfp->AddChildGroup("module_capabilities"));

    // release sfp & acquire caps lock
    mutable_sfp = nullptr;
    auto mutable_caps = caps->AcquireMutable();

    // Ok, now go remove the attributes
    mutable_caps->AddAttribute("f_100", datasource_->GetModCapF100());
    mutable_caps->AddAttribute("f_1g", datasource_->GetModCapF1G());
    mutable_caps->AddAttribute("f_10g", datasource_->GetModCapF10G());
    mutable_caps->AddAttribute("f_40g", datasource_->GetModCapF40G());
    mutable_caps->AddAttribute("f_100g", datasource_->GetModCapF100G());

    // release caps lock
    mutable_caps = nullptr;

    // Add SFPChannel Attributes
    // note: use a 0-based index for both database and ONLP
    ASSIGN_OR_RETURN(sfp_channel_count_,
        datasource_->GetSfpChannelCount()->ReadValue<int>());
    for (int id=0; id < sfp_channel_count_; id++) {
        // lock us so we can modify
        mutable_sfp = sfp_group_->AcquireMutable();

        // Get a new channel
        ASSIGN_OR_RETURN(auto channel,
            mutable_sfp->AddRepeatedChildGroup("channels"));

        // release lock
        mutable_sfp = nullptr;

        // Note: don't move pointer, just a reference
        AddChannel(id, channel);
    }

    // we're now initialized
    initialized_ = true;

    return ::util::OkStatus();
}

::util::Status OnlpSfpConfigurator::RemoveSfp() {
    // Make sure we have a been initialized
    absl::WriterMutexLock l(&config_lock_);
    if (!initialized_) {
        RETURN_ERROR() << "sfp id " << id_ << " has not been added";
    }

    // lock us so we can modify
    auto mutable_sfp = sfp_group_->AcquireMutable();

    mutable_sfp->RemoveAttribute("media_type");
    mutable_sfp->RemoveAttribute("connector_type");
    mutable_sfp->RemoveAttribute("module_type");
    mutable_sfp->RemoveAttribute("cable_length");
    mutable_sfp->RemoveAttribute("cable_length_desc");
    mutable_sfp->RemoveAttribute("temperature");
    mutable_sfp->RemoveAttribute("vcc");

    // Get HardwareInfo DB group
    ASSIGN_OR_RETURN(auto info, mutable_sfp->GetChildGroup("info"));
    auto mutable_info = info->AcquireMutable();

    // Ok, now go remove the attributes
    mutable_info->RemoveAttribute("mfg_name");
    mutable_info->RemoveAttribute("part_no");
    mutable_info->RemoveAttribute("serial_no");

    // Remove "info" group
    mutable_info = nullptr;
    RETURN_IF_ERROR(mutable_sfp->RemoveChildGroup("info"));

    // Get SfpModuleCaps DB group
    ASSIGN_OR_RETURN(auto caps,
        mutable_sfp->GetChildGroup("module_capabilities"));
    auto mutable_caps = caps->AcquireMutable();

    // Ok, now go remove the attributes
    mutable_caps->RemoveAttribute("f_100");
    mutable_caps->RemoveAttribute("f_1g");
    mutable_caps->RemoveAttribute("f_10g");
    mutable_caps->RemoveAttribute("f_40g");
    mutable_caps->RemoveAttribute("f_100g");

    // Remove "module_caps" group
    mutable_caps = nullptr;
    RETURN_IF_ERROR(mutable_sfp->RemoveChildGroup("module_capabilities"));

    // Remove SFPChannel Attributes
    // note: use a 0-based index for both database and ONLP
    for (int id=0; id < sfp_channel_count_; id++) {
        // Get channel group
        ASSIGN_OR_RETURN(auto channel,
            mutable_sfp->GetRepeatedChildGroup("channels", id));

        // Note: don't move pointer, just a reference
        RemoveChannel(id, channel);
    }

    // Remove all the channel groups
    RETURN_IF_ERROR(mutable_sfp->RemoveRepeatedChildGroup("channels"));

    // we're now not initialized
    initialized_ = false;

    return ::util::OkStatus();
}

::util::Status OnlpSfpConfigurator::AddChannel(int id,
    AttributeGroup* channel) {

    // lcok channel group
    auto mutable_channel = channel->AcquireMutable();

    // Now add the attributes
    mutable_channel->AddAttribute("rx_power", datasource_->GetSfpRxPower(id));
    mutable_channel->AddAttribute("tx_power", datasource_->GetSfpTxPower(id));
    mutable_channel->AddAttribute("tx_bias", datasource_->GetSfpTxBias(id));

    return ::util::OkStatus();
}

::util::Status OnlpSfpConfigurator::RemoveChannel(int id,
    AttributeGroup* channel) {

    // lock channel group
    auto mutable_channel = channel->AcquireMutable();

    // Remove the attributes
    mutable_channel->RemoveAttribute("rx_power");
    mutable_channel->RemoveAttribute("tx_power");
    mutable_channel->RemoveAttribute("tx_bias");

    return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum

