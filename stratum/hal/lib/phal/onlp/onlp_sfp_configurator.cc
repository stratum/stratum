// Copyright 2019 Dell EMC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_sfp_configurator.h"

#include <fstream>
#include <iostream>
#include <string>

#include "stratum/hal/lib/common/common.pb.h"

using namespace google::protobuf::util;  // NOLINT

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

OnlpSfpConfigurator::OnlpSfpConfigurator(
    std::shared_ptr<OnlpSfpDataSource> datasource, AttributeGroup* sfp_group,
    OnlpOid oid)
    : OnlpEventCallback(oid),
      datasource_(ABSL_DIE_IF_NULL(datasource)),
      sfp_group_(ABSL_DIE_IF_NULL(sfp_group)) {
  // lock us so we can modify
  auto mutable_sfp = sfp_group_->AcquireMutable();

  // Ok, now go add the sfp attributes
  mutable_sfp->AddAttribute("id", datasource_->GetSfpId());
  mutable_sfp->AddAttribute("description", datasource_->GetSfpDesc());
  // Hardware state is not updated by the datasource, but by us in response to
  // the Onlp callback.
  mutable_sfp->AddAttribute(
      "hardware_state",
      FixedEnumDataSource::Make(HwState_descriptor(), HW_STATE_NOT_PRESENT)
          ->GetAttribute());
}

::util::StatusOr<std::unique_ptr<OnlpSfpConfigurator>>
OnlpSfpConfigurator::Make(std::shared_ptr<OnlpSfpDataSource> datasource,
                          AttributeGroup* sfp_group, OnlpOid oid) {
  return absl::WrapUnique(new OnlpSfpConfigurator(datasource, sfp_group, oid));
}

::util::Status OnlpSfpConfigurator::HandleOidStatusChange(
    const OidInfo& oid_info) {
  CHECK_RETURN_IF_FALSE(GetOid() == oid_info.GetHeader()->id)
      << "Status change event oid " << oid_info.GetHeader()->id
      << " does not match configurator oid: " << GetOid();

  return HandleEvent(oid_info.GetHardwareState());
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
::util::Status OnlpSfpConfigurator::AddSfp() {
  // Make sure we don't already have a SFP added to the DB
  absl::WriterMutexLock l(&config_lock_);

  if (initialized_) {
    VLOG(1) << "SFP " << datasource_->GetSfpId() << " already exists.";
    return ::util::OkStatus();
  }

  // lock us so we can modify
  auto mutable_sfp = sfp_group_->AcquireMutable();

  mutable_sfp->AddAttribute(
      "hardware_state",
      FixedEnumDataSource::Make(HwState_descriptor(), HW_STATE_PRESENT)
          ->GetAttribute());

  // TODO(max): Error checking
  mutable_sfp->AddAttribute("media_type", datasource_->GetSfpMediaType());
  mutable_sfp->AddAttribute("connector_type", datasource_->GetSfpType());
  mutable_sfp->AddAttribute("module_type", datasource_->GetSfpModuleType());
  mutable_sfp->AddAttribute("cable_length", datasource_->GetSfpCableLength());
  mutable_sfp->AddAttribute("cable_length_desc",
                            datasource_->GetSfpCableLengthDesc());
  mutable_sfp->AddAttribute("temperature", datasource_->GetSfpTemperature());
  mutable_sfp->AddAttribute("vcc", datasource_->GetSfpVoltage());
  mutable_sfp->AddAttribute("channel_count", datasource_->GetSfpChannelCount());

  {
    // Get HardwareInfo DB group
    ASSIGN_OR_RETURN(auto info, mutable_sfp->AddChildGroup("info"));
    auto mutable_info_group = info->AcquireMutable();
    // Ok, now go add the info attributes
    mutable_info_group->AddAttribute("mfg_name", datasource_->GetSfpVendor());
    mutable_info_group->AddAttribute("part_no", datasource_->GetSfpModel());
    mutable_info_group->AddAttribute("serial_no",
                                     datasource_->GetSfpSerialNumber());
  }

  {
    // Get SfpModuleCaps DB group
    ASSIGN_OR_RETURN(auto caps,
                     mutable_sfp->AddChildGroup("module_capabilities"));
    auto mutable_caps = caps->AcquireMutable();
    // Ok, now go remove the attributes
    mutable_caps->AddAttribute("f_100", datasource_->GetModCapF100());
    mutable_caps->AddAttribute("f_1g", datasource_->GetModCapF1G());
    mutable_caps->AddAttribute("f_10g", datasource_->GetModCapF10G());
    mutable_caps->AddAttribute("f_40g", datasource_->GetModCapF40G());
    mutable_caps->AddAttribute("f_100g", datasource_->GetModCapF100G());
  }

  // Add SFPChannel Attributes
  // note: use a 0-based index for both database and ONLP
  ASSIGN_OR_RETURN(auto sfp_channel_count,
                   datasource_->GetSfpChannelCount()->ReadValue<int>());
  for (int id = 0; id < sfp_channel_count; id++) {
    // Get a new channel
    ASSIGN_OR_RETURN(auto channel,
                     mutable_sfp->AddRepeatedChildGroup("channels"));
    auto mutable_channel = channel->AcquireMutable();
    // Now add the attributes
    mutable_channel->AddAttribute("rx_power", datasource_->GetSfpRxPower(id));
    mutable_channel->AddAttribute("tx_power", datasource_->GetSfpTxPower(id));
    mutable_channel->AddAttribute("tx_bias", datasource_->GetSfpTxBias(id));
  }

  // we're now initialized
  initialized_ = true;

  return ::util::OkStatus();
}

::util::Status OnlpSfpConfigurator::RemoveSfp() {
  // Make sure we have a been initialized
  absl::WriterMutexLock l(&config_lock_);
  if (!initialized_) {
    VLOG(1)
        << "Can't remove SFP "
        << sfp_group_->AcquireReadable()->ReadAttribute<int>("id").ValueOrDie()
        << " from attribute DB, because it does not exists.";
    return ::util::OkStatus();
  }

  // lock us so we can modify
  auto mutable_sfp = sfp_group_->AcquireMutable();

  mutable_sfp->AddAttribute(
      "hardware_state",
      FixedEnumDataSource::Make(HwState_descriptor(), HW_STATE_NOT_PRESENT)
          ->GetAttribute());

  // TODO(max): Error checking
  mutable_sfp->RemoveAttribute("media_type");
  mutable_sfp->RemoveAttribute("connector_type");
  mutable_sfp->RemoveAttribute("module_type");
  mutable_sfp->RemoveAttribute("cable_length");
  mutable_sfp->RemoveAttribute("cable_length_desc");
  mutable_sfp->RemoveAttribute("temperature");
  mutable_sfp->RemoveAttribute("vcc");

  {
    // Get HardwareInfo DB group
    ASSIGN_OR_RETURN(auto info, mutable_sfp->GetChildGroup("info"));
    auto mutable_info = info->AcquireMutable();
    mutable_info->RemoveAttribute("mfg_name");
    mutable_info->RemoveAttribute("part_no");
    mutable_info->RemoveAttribute("serial_no");
  }
  RETURN_IF_ERROR(mutable_sfp->RemoveChildGroup("info"));

  {
    // Get SfpModuleCaps DB group
    ASSIGN_OR_RETURN(auto caps,
                     mutable_sfp->GetChildGroup("module_capabilities"));
    auto mutable_caps = caps->AcquireMutable();
    mutable_caps->RemoveAttribute("f_100");
    mutable_caps->RemoveAttribute("f_1g");
    mutable_caps->RemoveAttribute("f_10g");
    mutable_caps->RemoveAttribute("f_40g");
    mutable_caps->RemoveAttribute("f_100g");
  }
  RETURN_IF_ERROR(mutable_sfp->RemoveChildGroup("module_capabilities"));

  // Remove SFPChannel Attributes
  // note: use a 0-based index for both database and ONLP
  ASSIGN_OR_RETURN(auto sfp_channel_count,
                   datasource_->GetSfpChannelCount()->ReadValue<int>())
  for (int id = 0; id < sfp_channel_count; id++) {
    // Get channel group
    ASSIGN_OR_RETURN(auto channel,
                     mutable_sfp->GetRepeatedChildGroup("channels", id));

    auto mutable_channel = channel->AcquireMutable();
    mutable_channel->RemoveAttribute("rx_power");
    mutable_channel->RemoveAttribute("tx_power");
    mutable_channel->RemoveAttribute("tx_bias");
  }
  // Remove all the channel groups
  RETURN_IF_ERROR(mutable_sfp->RemoveRepeatedChildGroup("channels"));

  // we're now not initialized
  initialized_ = false;

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
