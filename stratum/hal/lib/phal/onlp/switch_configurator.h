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


#ifndef STRATUM_HAL_LIB_PHAL_ONLP_SWITCH_CONFIGURATOR_H_
#define STRATUM_HAL_LIB_PHAL_ONLP_SWITCH_CONFIGURATOR_H_

#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/common/phal_interface.h"
#include "stratum/hal/lib/phal/attribute_group.h"
#include "stratum/hal/lib/phal/onlp/onlp_wrapper.h"
#include "stratum/hal/lib/phal/switch_configurator.h"
#include "stratum/hal/lib/phal/datasource.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

class OnlpSwitchConfigurator : public SwitchConfigurator {
 public:
   static ::util::StatusOr<std::unique_ptr<OnlpSwitchConfigurator>> Make(
        PhalInterface* phal_interface,
        OnlpInterface* onlp_interface);

   // Create a default config
   ::util::Status CreateDefaultConfig(PhalInitConfig* config) const override;

   // Configure the Phal DB
   ::util::Status ConfigurePhalDB(
        PhalInitConfig& config, AttributeGroup* root) override;

 private:
   ::util::StatusOr<OidInfo> GetOidInfo(
        AttributeGroup *group, OnlpOid oid) const;

   // Add a Port to the Phal DB
   ::util::Status AddPort(int card_id, int port_id,
        MutableAttributeGroup* mutable_card,
        const PhalCardConfig::Port& config);

   // Add a Fan to the Phal DB
   ::util::Status AddFan(int id,
        MutableAttributeGroup* mutable_fan_tray,
        const PhalFanTrayConfig::Fan& config);

   // Add a Psu to the Phal DB
   ::util::Status AddPsu(int id,
        MutableAttributeGroup* mutable_psu_tray,
        const PhalPsuTrayConfig::Psu& config);

   // Add a Led to the Phal DB
   ::util::Status AddLed(int id,
        MutableAttributeGroup* mutable_group,
        const PhalLedGroupConfig_Led& config);

   // Add a Thermal to the Phal DB
   ::util::Status AddThermal(int id,
        MutableAttributeGroup* mutable_group,
        const PhalThermalGroupConfig_Thermal& config);

   OnlpSwitchConfigurator() = delete;
   OnlpSwitchConfigurator(PhalInterface* phal_interface, 
        OnlpInterface* onlp_interface)
        : phal_interface_(phal_interface),
          onlp_interface_(onlp_interface) {};

   PhalInterface* phal_interface_;
   OnlpInterface* onlp_interface_;
   // Default cache policy config
   CachePolicyConfig cache_policy_config_;

   // Need to make sure we don't add the same onlp id twice
   std::map<int, bool> sfp_id_map_;
   std::map<int, bool> fan_id_map_;
   std::map<int, bool> psu_id_map_;
   std::map<int, bool> led_id_map_;
   std::map<int, bool> thermal_id_map_;
};

}  // namespace onlp
}  // namespace phal
}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_PHAL_ONLP_SWITCH_CONFIGURATOR_H_
