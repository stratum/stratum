// Copyright 2019 Dell EMC
// Copyright 2020 Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_PHAL_ONLP_ONLP_SWITCH_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_ONLP_SWITCH_CONFIGURATOR_H_

#include <map>
#include <memory>

#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/onlp/onlp_phal_interface.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/switch_configurator_interface.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpSwitchConfigurator : public SwitchConfiguratorInterface {
 public:
  static ::util::StatusOr<std::unique_ptr<OnlpSwitchConfigurator>> Make(
      OnlpPhalInterface* phal_interface, OnlpInterface* onlp_interface);

  // Create a default config
  ::util::Status CreateDefaultConfig(PhalInitConfig* config) const override;

  // Configure the Phal DB
  ::util::Status ConfigurePhalDB(PhalInitConfig* config,
                                 AttributeGroup* root) override;

 private:
  ::util::StatusOr<OidInfo> GetOidInfo(AttributeGroup* group,
                                       OnlpOid oid) const;

  // Add a Port to the Phal DB
  ::util::Status AddPort(int slot, int port,
                         MutableAttributeGroup* mutable_card,
                         const PhalCardConfig::Port& config);

  // Add a Fan to the Phal DB
  ::util::Status AddFan(int id, MutableAttributeGroup* mutable_fan_tray,
                        const PhalFanTrayConfig::Fan& config);

  // Add a Psu to the Phal DB
  ::util::Status AddPsu(int id, MutableAttributeGroup* mutable_psu_tray,
                        const PhalPsuTrayConfig::Psu& config);

  // Add a Led to the Phal DB
  ::util::Status AddLed(int id, MutableAttributeGroup* mutable_group,
                        const PhalLedGroupConfig_Led& config);

  // Add a Thermal to the Phal DB
  ::util::Status AddThermal(int id, MutableAttributeGroup* mutable_group,
                            const PhalThermalGroupConfig_Thermal& config);

  OnlpSwitchConfigurator() = delete;
  OnlpSwitchConfigurator(OnlpPhalInterface* phal_interface,
                         OnlpInterface* onlp_interface)
      : onlp_phal_interface_(phal_interface), onlp_interface_(onlp_interface) {}

  OnlpPhalInterface* onlp_phal_interface_;
  OnlpInterface* onlp_interface_;
  // Default cache policy config
  CachePolicyConfig cache_policy_config_;

  // Need to make sure we don't add the same onlp id twice
  std::map<int, bool> fan_id_map_;
  std::map<int, bool> psu_id_map_;
  std::map<int, bool> led_id_map_;
  std::map<int, bool> thermal_id_map_;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_ONLP_SWITCH_CONFIGURATOR_H_
