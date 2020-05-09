// Copyright 2020-present Open Networking Foundation
// Copyright 2020 PLVision
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/tai/tai_switch_configurator.h"

#include <string>
#include <utility>
#include <vector>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/lib/macros.h"
#include "stratum/hal/lib/phal/tai/tai_optics_datasource.h"


namespace stratum {
namespace hal {
namespace phal {
namespace tai {

// TaiSwitchConfigurator::Make method makes an instance of
// TaiSwitchConfigurator
::util::StatusOr<std::unique_ptr<TaiSwitchConfigurator>>
TaiSwitchConfigurator::Make(TaiInterface* tai_interface) {
  return absl::WrapUnique(new TaiSwitchConfigurator(tai_interface));
}

// TaiSwitchConfigurator::CreateDefaultConfig method generates a default
// config using the TAI API.
// Note that we stores TAI object id in the "vendor_specific_id" field since
// we need to use that ID later.
::util::Status TaiSwitchConfigurator::CreateDefaultConfig(
    PhalInitConfig* phal_config) const {
  ASSIGN_OR_RETURN(auto modules, tai_interface_->GetModuleIds());
  uint32 slot = 1;
  for (auto module_id : modules) {
    auto optical_module = phal_config->add_optical_modules();
    optical_module->set_slot(slot);
    optical_module->set_vendor_specific_id(module_id);

    ASSIGN_OR_RETURN(auto netifs,
                     tai_interface_->GetNetworkInterfaceIds(module_id));
    uint32 port_index = 1;
    for (auto netif_id : netifs) {
      auto optical_port = optical_module->add_ports();
      optical_port->set_port(port_index);
      optical_port->set_vendor_specific_id(netif_id);
      ++port_index;
    }
    ++slot;
  }

  return ::util::OkStatus();
}

// TaiSwitchConfigurator::ConfigurePhalDB method configures the switch's
// attribute database with the given PhalInitConfig config.
::util::Status TaiSwitchConfigurator::ConfigurePhalDB(
    PhalInitConfig* phal_config, AttributeGroup* root) {
  auto mutable_root = root->AcquireMutable();

  // Add optical modules
  for (auto module : *phal_config->mutable_optical_modules()) {
    if (!module.has_cache_policy()) {
      module.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }
    ASSIGN_OR_RETURN(auto optical_module_group,
                     mutable_root->AddRepeatedChildGroup("optical_modules"));
    auto mutable_optical_module_group = optical_module_group->AcquireMutable();
    RETURN_IF_ERROR(mutable_optical_module_group->AddAttribute("id",
                    FixedDataSource<int32>::Make(
                        module.slot())->GetAttribute()));

    for (auto port : *module.mutable_ports()) {
      if (!port.has_cache_policy()) {
        port.set_allocated_cache_policy(
            new CachePolicyConfig(module.cache_policy()));
      }
      RETURN_IF_ERROR(
          AddOpticalPort(mutable_optical_module_group.get(), port));
    }
  }
  return ::util::OkStatus();
}

::util::Status TaiSwitchConfigurator::AddOpticalPort(
    MutableAttributeGroup* mutable_module_group,
    const PhalOpticalModuleConfig::Port& port_config) {
  // To add optics proper data sources for network interfaces from a module
  // * frequency.
  // * target_output_power.
  // * operational_mode.
  // * output_power.
  // * input_power.
  ASSIGN_OR_RETURN(auto optical_port,
                   mutable_module_group->AddRepeatedChildGroup("network_interfaces"));
  auto mutable_optical_port = optical_port->AcquireMutable();
  ASSIGN_OR_RETURN(std::shared_ptr<TaiOpticsDataSource> datasource,
                   TaiOpticsDataSource::Make(port_config, tai_interface_));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("id",
                  datasource->GetId()));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("frequency",
                  datasource->GetTxLaserFrequency()));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("target_output_power",
                  datasource->GetTargetOutputPower()));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("operational_mode",
                  datasource->GetOperationalMode()));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("output_power",
                  datasource->GetCurrentOutputPower()));
  RETURN_IF_ERROR(mutable_optical_port->AddAttribute("input_power",
                  datasource->GetCurrentInputPower()));
  return ::util::OkStatus();
}

}  // namespace tai
}  // namespace phal
}  // namespace hal
}  // namespace stratum
