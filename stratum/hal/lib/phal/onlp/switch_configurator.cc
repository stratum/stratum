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

#include <utility>
#include <string>
#include <vector>

#include "stratum/hal/lib/phal/onlp/switch_configurator.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/phal.pb.h"
#include "stratum/hal/lib/phal/onlp/sfp_datasource.h"
#include "stratum/hal/lib/phal/onlp/psu_datasource.h"
#include "stratum/hal/lib/phal/onlp/fan_datasource.h"
#include "stratum/hal/lib/phal/onlp/led_datasource.h"
#include "stratum/hal/lib/phal/onlp/thermal_datasource.h"
#include "stratum/hal/lib/phal/onlp/sfp_configurator.h"

using namespace std;  // NOLINT

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {


// Make an instance of OnlpSwitchConfigurator
::util::StatusOr<std::unique_ptr<OnlpSwitchConfigurator>>
OnlpSwitchConfigurator::Make(
    PhalInterface* phal_interface,
    OnlpInterface* onlp_interface) {

    // Make sure we've got a valid Onlp Interface
    CHECK_RETURN_IF_FALSE(onlp_interface != nullptr);

    return absl::WrapUnique(new OnlpSwitchConfigurator(phal_interface,
                                                       onlp_interface));
}

// Generate a default config using the OID list from the NOS
::util::Status OnlpSwitchConfigurator::CreateDefaultConfig(
    PhalInitConfig* phal_config) const {

    std::vector <OnlpOid> oids;

    // Add a new card
    auto card = phal_config->add_cards();

    // Grab all the SFP OIDs
    ASSIGN_OR_RETURN(oids,
          onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_SFP));

    // Spin through the SFP Oids
    for (uint i=0; i < oids.size(); i++) {
        // Add a new port
        auto port = card->add_ports();

        // Set the OID id
        port->set_id(ONLP_OID_ID_GET(oids[i]));

        // Try and determine the physical port type
        auto result = onlp_interface_->GetSfpInfo(oids[i]);
        if (result.ok()) {
            SfpInfo sfp_info = result.ConsumeValueOrDie();

            // See if we've got an sfp type and set the physical
            // port type
            switch (sfp_info.GetSfpType()) {
            case SFP_TYPE_SFP:
                port->set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
                break;

            case SFP_TYPE_QSFP:
            case SFP_TYPE_QSFP_PLUS:
            case SFP_TYPE_QSFP28:
                port->set_physical_port_type(PHYSICAL_PORT_TYPE_QSFP_CAGE);
                break;

            // Don't set the port type
            default:
                break;
            }
        }
    }

    // Add a new fan tray
    auto fan_tray = phal_config->add_fan_trays();

    // Grab all FAN OIDs
    ASSIGN_OR_RETURN(oids,
          onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_FAN));

    // Spin through the FAN Oids
    for (uint i=0; i < oids.size(); i++) {
        // Add a new fan
        auto fan = fan_tray->add_fans();

        // Set the OID id
        fan->set_id(ONLP_OID_ID_GET(oids[i]));
    }

    // Add a PSU tray
    auto psu_tray = phal_config->add_psu_trays();

    // Grab all PSU OIDs
    ASSIGN_OR_RETURN(oids,
          onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_PSU));

    // Spin through the PSU Oids
    for (uint i=0; i < oids.size(); i++) {
        // Add a new port
        auto psu = psu_tray->add_psus();

        // Set the OID id
        psu->set_id(ONLP_OID_ID_GET(oids[i]));
    }

    // Add a Led Group
    auto led_group = phal_config->add_led_groups();

    // Grab all Led OIDs
    ASSIGN_OR_RETURN(oids,
          onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_LED));

    // Spin through the LED Oids
    for (uint i=0; i < oids.size(); i++) {
        // Add a new led
        auto led = led_group->add_leds();

        // Set the OID id
        led->set_id(ONLP_OID_ID_GET(oids[i]));
    }

    // Add a Thermal Group
    auto thermal_group = phal_config->add_thermal_groups();

    // Grab all Thermal OIDs
    ASSIGN_OR_RETURN(oids,
          onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_THERMAL));

    // Spin through the THERMAL Oids
    for (uint i=0; i < oids.size(); i++) {
        // Add a new thermal
        auto thermal = thermal_group->add_thermals();

        // Set the OID id
        thermal->set_id(ONLP_OID_ID_GET(oids[i]));
    }

    return ::util::OkStatus();
}

// Configure the switch's attribute database with the given
// PhalInitConfig config.
::util::Status OnlpSwitchConfigurator::ConfigurePhalDB(
    const PhalInitConfig& phal_config, AttributeGroup* root) {

    // Lock the root group
    auto mutable_root = root->AcquireMutable();

    // Add cards
    for (int j=0; j < phal_config.cards_size(); j++) {
        auto card_config = phal_config.cards(j);

        // If id set to default (i.e. not set) then use
        // the 1-based index of this config item
        int card_id = (card_config.id() == 0 ? (j+1): card_config.id());

        // Add Card to attribute DB
        ASSIGN_OR_RETURN(auto card,
            mutable_root->AddRepeatedChildGroup("cards"));
        std::unique_ptr<MutableAttributeGroup> mutable_card =
            card->AcquireMutable();

        // Use chassis cache policy if we have no card policy
        if (!card_config.has_cache_policy()) {
            card_config.set_allocated_cache_policy(
                new CachePolicyConfig(phal_config.cache_policy()));
        }

        // Add ports per card
        for (int i = 0; i < card_config.ports_size(); i++) {
            auto config = card_config.ports(i);

            // Use card cache policy if we have no port policy
            if (!config.has_cache_policy()) {
                config.set_allocated_cache_policy(
                    new CachePolicyConfig(card_config.cache_policy()));
            }

            // If id set to default (i.e. not set) then use
            // the 1-based index of this config item
            int port_id = (config.id() == 0 ? (i+1): config.id());

            // Add Port to attribute DB
            AddPort(card_id, port_id, mutable_card.get(), config);
        }
    }

    // Add Fans
    for (int fan_tray_id=0;
         fan_tray_id < phal_config.fan_trays_size();
         fan_tray_id++) {
        auto fan_tray_config = phal_config.fan_trays(fan_tray_id);

        // Add Fan Tray to attribute DB
        ASSIGN_OR_RETURN(auto fan_tray,
            mutable_root->AddRepeatedChildGroup("fan_trays"));
        auto mutable_fan_tray = fan_tray->AcquireMutable();

        // Use chassis cache policy if we have no fan tray policy
        if (!fan_tray_config.has_cache_policy()) {
            fan_tray_config.set_allocated_cache_policy(
                new CachePolicyConfig(phal_config.cache_policy()));
        }

        // Add Fans per tray
        for (int i=0; i < fan_tray_config.fans_size(); i++) {
            auto config = fan_tray_config.fans(i);

            // Use fan tray policy if we have no fan policy
            if (!config.has_cache_policy()) {
                config.set_allocated_cache_policy(
                    new CachePolicyConfig(fan_tray_config.cache_policy()));
            }

            // If id set to default (i.e. not set) then use
            // the 1-based index of this config item
            int id = (config.id() == 0 ? (i+1): config.id());

            // Add Fan to attribute DB
            AddFan(id, mutable_fan_tray.get(), config);
        }
    }

    // Add PSUs
    for (int psu_tray_id=0;
         psu_tray_id < phal_config.psu_trays_size();
         psu_tray_id++) {
        auto psu_tray_config = phal_config.psu_trays(psu_tray_id);

        // Add PSU Tray to attribute DB
        ASSIGN_OR_RETURN(auto psu_tray,
            mutable_root->AddRepeatedChildGroup("psu_trays"));
        auto mutable_psu_tray = psu_tray->AcquireMutable();

        // Use chassis cache policy if we have no psu tray policy
        if (!psu_tray_config.has_cache_policy()) {
            psu_tray_config.set_allocated_cache_policy(
                new CachePolicyConfig(phal_config.cache_policy()));
        }

        // Add PSUs per tray
        for (int i = 0; i < psu_tray_config.psus_size(); i++) {
            auto config = psu_tray_config.psus(i);

            // Use psu tray policy if we have no psu policy
            if (!config.has_cache_policy()) {
                config.set_allocated_cache_policy(
                    new CachePolicyConfig(psu_tray_config.cache_policy()));
            }

            // If id set to default (i.e. not set) then use
            // the 1-based index of this config item
            int id = (config.id() == 0 ? (i+1): config.id());

            // Add Psu to attribute DB
            AddPsu(id, mutable_psu_tray.get(), config);
        }
    }

    // Add LEDs
    for (int group_id = 0;
         group_id < phal_config.led_groups_size();
         group_id++) {
        auto group_config = phal_config.led_groups(group_id);

        // Add LED Group to attribute DB
        ASSIGN_OR_RETURN(auto group,
            mutable_root->AddRepeatedChildGroup("led_groups"));
        auto mutable_group = group->AcquireMutable();

        // Use chassis cache policy if we have no led group policy
        if (!group_config.has_cache_policy()) {
            group_config.set_allocated_cache_policy(
                new CachePolicyConfig(phal_config.cache_policy()));
        }

        // Add LEDs
        for (int i = 0; i < group_config.leds_size(); i++) {
            auto config = group_config.leds(i);

            // Use card policy if we have no led group policy
            if (!config.has_cache_policy()) {
                config.set_allocated_cache_policy(
                    new CachePolicyConfig(group_config.cache_policy()));
            }

            // If id set to default (i.e. not set) then use
            // the 1-based index of this config item
            int id = (config.id() == 0 ? (i+1): config.id());

            // Add Led to attribute DB
            AddLed(id, mutable_group.get(), config);
        }
    }

    // Add Thermals
    for (int group_id = 0;
         group_id < phal_config.thermal_groups_size();
         group_id++) {
        auto group_config = phal_config.thermal_groups(group_id);

        // Add Thermal Group to attribute DB
        ASSIGN_OR_RETURN(auto group,
            mutable_root->AddRepeatedChildGroup("thermal_groups"));
        auto mutable_group = group->AcquireMutable();

        // Use chassis cache policy if we have no thermal group policy
        if (!group_config.has_cache_policy()) {
            group_config.set_allocated_cache_policy(
                new CachePolicyConfig(phal_config.cache_policy()));
        }

        // Add Thermals
        for (int i = 0; i < group_config.thermals_size(); i++) {
            auto config = group_config.thermals(i);

            // Use card policy if we have no thermal group policy
            if (!config.has_cache_policy()) {
                config.set_allocated_cache_policy(
                    new CachePolicyConfig(group_config.cache_policy()));
            }

            // If id set to default (i.e. not set) then use
            // the 1-based index of this config item
            int id = (config.id() == 0 ? (i+1): config.id());

            // Add Thermal to attribute DB
            AddThermal(id, mutable_group.get(), config);
        }
    }

    return ::util::OkStatus();
}

::util::StatusOr<OidInfo> OnlpSwitchConfigurator::GetOidInfo(
    AttributeGroup *group, OnlpOid oid) const {

    // Check device info
    auto result = onlp_interface_->GetOidInfo(oid);
    if (!result.ok()) {
        LOG(ERROR) << "failed to GetOidInfo for "
            << to_string(oid) << ": " << result.status().error_message();

        auto mutable_group = group->AcquireMutable();
        mutable_group->AddAttribute("id",
            FixedDataSource<int>::Make(ONLP_OID_ID_GET(oid))->GetAttribute());
        std::string err_msg = "Failed to get oid info for oid: "+to_string(oid)+
            " error code: "+to_string(result.status().error_code());
        mutable_group->AddAttribute("err_msg",
            FixedDataSource<std::string>::Make(err_msg)->GetAttribute());
        mutable_group->AddAttribute("hardware_state",
            FixedEnumDataSource::Make(HwState_descriptor(), HW_STATE_FAILED)
                ->GetAttribute());
    }

    return result;
}

::util::Status OnlpSwitchConfigurator::AddPort(
    int card_id, int port_id,
    MutableAttributeGroup* mutable_card,
    const PhalCardConfig::Port& config) {

    // Add port to attribute DB
    ASSIGN_OR_RETURN(auto port,
        mutable_card->AddRepeatedChildGroup("ports"));
    auto mutable_port = port->AcquireMutable();

    // Create a transceiver group in the Phal DB
    ASSIGN_OR_RETURN(auto sfp,
        mutable_port->AddChildGroup("transceiver"));

    // Check to make sure we haven't already added this id
    if (sfp_id_map_[port_id]) {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "duplicate sfp id: " << port_id;
    }
    sfp_id_map_[port_id] = true;

    // Check to make sure port exists
    // Note: will need to figure out how to map card id and port id
    //       into an OID, for now we ignore card id.
    auto status = GetOidInfo(sfp, ONLP_SFP_ID_CREATE(port_id));
    if (!status.ok()) return status.status();

    // If it's an SFP/QSFP then the transceiver data source
    // will be added dynamically upon insertion
    switch (config.physical_port_type()) {
    case PHYSICAL_PORT_TYPE_SFP_CAGE:
    case PHYSICAL_PORT_TYPE_QSFP_CAGE:

        {
            // Create Caching policy
            ASSIGN_OR_RETURN(auto cache,
                CachePolicyFactory::CreateInstance(
                    config.cache_policy().type(),
                    config.cache_policy().timed_value()));

            // Create a new data source
            ASSIGN_OR_RETURN(auto datasource,
                OnlpSfpDataSource::Make(port_id, onlp_interface_, cache));

            // Create an SFP Configurator
            ASSIGN_OR_RETURN(auto configurator,
                OnlpSfpConfigurator::Make(port_id, datasource, sfp,
                                          onlp_interface_));

            // Store a reference in onlpphal
            // Danger: Attribte Database should hold onto this pointer
            //         until the transceiver group is deleted.... which
            //         should be never.
            RETURN_IF_ERROR(phal_interface_->RegisterSfpConfigurator(
                card_id, port_id, configurator.get()));

            // Save it in the database
            auto mutable_sfp = sfp->AcquireMutable();
            mutable_sfp->AddRuntimeConfigurator(std::move(configurator));
        }
        break;

    // All other port types
    default:
        LOG(INFO) << "card[" << card_id << "]/port[" << port_id
            << "]: transceiver type " << PhysicalPortType_descriptor()
                  ->FindValueByNumber(config.physical_port_type())
                  ->name()
            << " not handled.";
        break;
    }

    return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddFan(
    int id,
    MutableAttributeGroup* mutable_fan_tray,
    const PhalFanTrayConfig::Fan& config) {

    // Add Fan to Fan Trays in the Phal DB
    // note: using a 1-based id for the index
    ASSIGN_OR_RETURN(auto fan,
        mutable_fan_tray->AddRepeatedChildGroup("fans"));

    // Check to make sure we haven't already added this id
    if (fan_id_map_[id]) {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "duplicate fan id: " << id;
    }
    fan_id_map_[id] = true;

    ASSIGN_OR_RETURN(OidInfo oid_info,
        GetOidInfo(fan, ONLP_FAN_ID_CREATE(id)));
    auto mutable_fan = fan->AcquireMutable();

    // Create Caching policy
    ASSIGN_OR_RETURN(auto cache,
        CachePolicyFactory::CreateInstance(
            config.cache_policy().type(),
            config.cache_policy().timed_value()));

    // Create a new data source
    ASSIGN_OR_RETURN(std::shared_ptr<OnlpFanDataSource> datasource,
        OnlpFanDataSource::Make(id, onlp_interface_, cache));

    // Add Fan Attributes
    RETURN_IF_ERROR(mutable_fan->AddAttribute("id",
        datasource->GetFanId()));
    RETURN_IF_ERROR(mutable_fan->AddAttribute("description",
        datasource->GetFanDesc()));
    RETURN_IF_ERROR(mutable_fan->AddAttribute("hardware_state",
        datasource->GetFanHardwareState()));

    // Other attributes only valid when device is present
    if (!oid_info.Present()) {
      return ::util::OkStatus();
    }

    // Other attributes will only be valid when the device is present
    RETURN_IF_ERROR(mutable_fan->AddAttribute("rpm",
        datasource->GetFanRPM()));
    RETURN_IF_ERROR(mutable_fan->AddAttribute("speed_control",
        datasource->GetFanPercentage()));
    RETURN_IF_ERROR(mutable_fan->AddAttribute("direction",
        datasource->GetFanDirection()));

    // Get info DB group
    ASSIGN_OR_RETURN(auto info, mutable_fan->AddChildGroup("info"));

    // release fan lock & acquire info lock
    mutable_fan = nullptr;
    auto mutable_info = info->AcquireMutable();

    // We'll map model to info.part_no in the DB
    RETURN_IF_ERROR(mutable_info->AddAttribute("part_no",
        datasource->GetFanModel()));
    RETURN_IF_ERROR(mutable_info->AddAttribute("serial_no",
        datasource->GetFanSerialNumber()));

    // release info lock & acquire fan lock
    mutable_info = nullptr;
    mutable_fan = fan->AcquireMutable();

    // Get capabilities DB group
    ASSIGN_OR_RETURN(auto caps, mutable_fan->AddChildGroup("capabilities"));

    // release fan lock & acquire info lock
    mutable_fan = nullptr;
    auto mutable_caps = caps->AcquireMutable();

    RETURN_IF_ERROR(mutable_caps->AddAttribute("set_dir",
        datasource->GetCapSetDir()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_dir",
        datasource->GetCapGetDir()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("set_rpm",
        datasource->GetCapSetRpm()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("set_percentage",
        datasource->GetCapSetPercentage()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_rpm",
        datasource->GetCapGetRpm()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_percentage",
        datasource->GetCapGetPercentage()));

    return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddPsu(
    int id,
    MutableAttributeGroup* mutable_psu_tray,
    const PhalPsuTrayConfig::Psu& config) {

    // Add Psu to Psu Trays in the Phal DB
    // note: using a 1-based id for the index
    ASSIGN_OR_RETURN(auto psu,
        mutable_psu_tray->AddRepeatedChildGroup("psus"));

    // Check to make sure we haven't already added this id
    if (psu_id_map_[id]) {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "duplicate psu id: " << id;
    }
    psu_id_map_[id] = true;

    ASSIGN_OR_RETURN(OidInfo oid_info,
        GetOidInfo(psu, ONLP_PSU_ID_CREATE(id)));
    auto mutable_psu = psu->AcquireMutable();

    // Create Caching policy
    ASSIGN_OR_RETURN(auto cache,
        CachePolicyFactory::CreateInstance(
            config.cache_policy().type(),
            config.cache_policy().timed_value()));

    // Create Psu data source
    ASSIGN_OR_RETURN(std::shared_ptr<OnlpPsuDataSource> datasource,
        OnlpPsuDataSource::Make(id, onlp_interface_, cache));

    // Add Psu Attributes
    RETURN_IF_ERROR(mutable_psu->AddAttribute("id",
        datasource->GetPsuId()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("description",
        datasource->GetPsuDesc()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("hardware_state",
        datasource->GetPsuHardwareState()));

    // Other attributes only valid when device is present
    if (!oid_info.Present()) {
      return ::util::OkStatus();
    }

    RETURN_IF_ERROR(mutable_psu->AddAttribute("input_voltage",
        datasource->GetPsuInputVoltage()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("output_voltage",
        datasource->GetPsuOutputVoltage()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("input_current",
        datasource->GetPsuInputCurrent()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("output_current",
        datasource->GetPsuOutputCurrent()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("input_power",
        datasource->GetPsuInputPower()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("output_power",
        datasource->GetPsuOutputPower()));
    RETURN_IF_ERROR(mutable_psu->AddAttribute("type",
        datasource->GetPsuType()));

    // Get info DB group
    ASSIGN_OR_RETURN(auto info, mutable_psu->AddChildGroup("info"));

    // release psu lock & acquire info lock
    mutable_psu = nullptr;
    auto mutable_info = info->AcquireMutable();

    // We'll map model to info.part_no in the DB
    RETURN_IF_ERROR(mutable_info->AddAttribute("part_no",
        datasource->GetPsuModel()));
    RETURN_IF_ERROR(mutable_info->AddAttribute("serial_no",
        datasource->GetPsuSerialNumber()));

    // release info lock & acquire psu lock
    mutable_info = nullptr;
    mutable_psu = psu->AcquireMutable();

    // Get capabilities DB group
    ASSIGN_OR_RETURN(auto caps, mutable_psu->AddChildGroup("capabilities"));

    // release psu lock & acquire info lock
    mutable_psu = nullptr;
    auto mutable_caps = caps->AcquireMutable();

    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_type",
        datasource->GetCapGetType()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_vin",
        datasource->GetCapGetVIn()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_vout",
        datasource->GetCapGetVOut()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_iin",
        datasource->GetCapGetIIn()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_iout",
        datasource->GetCapGetIOut()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_pin",
        datasource->GetCapGetPIn()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_pout",
        datasource->GetCapGetPOut()));

    return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddLed(
    int id,
    MutableAttributeGroup* mutable_group,
    const PhalLedGroupConfig_Led& config) {

    // Add Led to the Phal DB
    // note: using a 1-based id for the index
    ASSIGN_OR_RETURN(auto led,
            mutable_group->AddRepeatedChildGroup("leds"));

    // Check to make sure we haven't already added this id
    if (led_id_map_[id]) {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "duplicate led id: " << id;
    }
    led_id_map_[id] = true;

    ASSIGN_OR_RETURN(OidInfo oid_info,
        GetOidInfo(led, ONLP_LED_ID_CREATE(id)));
    auto mutable_led = led->AcquireMutable();

    // Create Caching policy
    ASSIGN_OR_RETURN(auto cache,
        CachePolicyFactory::CreateInstance(
            config.cache_policy().type(),
            config.cache_policy().timed_value()));

    // Create data source
    ASSIGN_OR_RETURN(std::shared_ptr<OnlpLedDataSource> datasource,
        OnlpLedDataSource::Make(id, onlp_interface_, cache));

    // Add Led Attributes
    RETURN_IF_ERROR(mutable_led->AddAttribute("id",
        datasource->GetLedId()));
    RETURN_IF_ERROR(mutable_led->AddAttribute("description",
        datasource->GetLedDesc()));
    RETURN_IF_ERROR(mutable_led->AddAttribute("hardware_state",
        datasource->GetLedHardwareState()));

    // Other attributes only valid when device is present
    if (!oid_info.Present()) {
      return ::util::OkStatus();
    }

    RETURN_IF_ERROR(mutable_led->AddAttribute("mode",
        datasource->GetLedMode()));
    RETURN_IF_ERROR(mutable_led->AddAttribute("character",
        datasource->GetLedChar()));

    // RETURN_IF_ERROR(mutable_led->AddAttribute("state",
    //    datasource->GetLedState()));
    // RETURN_IF_ERROR(mutable_led->AddAttribute("color",
    //    datasource->GetLedColor()));

    // Get capabilities DB group
    ASSIGN_OR_RETURN(auto caps, mutable_led->AddChildGroup("capabilities"));

    // release psu lock & acquire info lock
    mutable_led = nullptr;
    auto mutable_caps = caps->AcquireMutable();

    RETURN_IF_ERROR(mutable_caps->AddAttribute("off",
        datasource->GetCapOff()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("auto",
        datasource->GetCapAuto()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("auto_blinking",
        datasource->GetCapAutoBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("char",
        datasource->GetCapChar()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("red",
        datasource->GetCapRed()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("red_blinking",
        datasource->GetCapRedBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("orange",
        datasource->GetCapOrange()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("orange_blinking",
        datasource->GetCapOrangeBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("yellow",
        datasource->GetCapYellow()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("yellow_blinking",
        datasource->GetCapYellowBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("green",
        datasource->GetCapGreen()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("green_blinking",
        datasource->GetCapGreenBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("blue",
        datasource->GetCapBlue()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("blue_blinking",
        datasource->GetCapBlueBlinking()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("purple",
        datasource->GetCapPurple()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("purple_blinking",
        datasource->GetCapPurpleBlinking()));

    return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddThermal(
    int id,
    MutableAttributeGroup* mutable_group,
    const PhalThermalGroupConfig_Thermal& config) {

    // Add Thermal to the Phal DB
    // note: using a 1-based id for the index
    ASSIGN_OR_RETURN(auto thermal,
            mutable_group->AddRepeatedChildGroup("thermals"));

    // Check to make sure we haven't already added this id
    if (thermal_id_map_[id]) {
        RETURN_ERROR(ERR_INVALID_PARAM)
            << "duplicate thermal id: " << id;
    }
    thermal_id_map_[id] = true;

    ASSIGN_OR_RETURN(OidInfo oid_info,
        GetOidInfo(thermal, ONLP_THERMAL_ID_CREATE(id)));
    auto mutable_thermal = thermal->AcquireMutable();

    // Create Caching policy
    ASSIGN_OR_RETURN(auto cache,
        CachePolicyFactory::CreateInstance(
            config.cache_policy().type(),
            config.cache_policy().timed_value()));

    // Create data source
    ASSIGN_OR_RETURN(std::shared_ptr<OnlpThermalDataSource> datasource,
        OnlpThermalDataSource::Make(id, onlp_interface_, cache));

    // Add Thermal Attributes
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("id",
        datasource->GetThermalId()));
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("description",
        datasource->GetThermalDesc()));
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("hardware_state",
        datasource->GetThermalHardwareState()));

    // Other attributes only valid when device is present
    if (!oid_info.Present()) {
      return ::util::OkStatus();
    }

    RETURN_IF_ERROR(mutable_thermal->AddAttribute("cur_temp",
        datasource->GetThermalCurTemp()));
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("warn_temp",
        datasource->GetThermalWarnTemp()));
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("error_temp",
        datasource->GetThermalErrorTemp()));
    RETURN_IF_ERROR(mutable_thermal->AddAttribute("shut_down_temp",
        datasource->GetThermalShutDownTemp()));

    // Get capabilities DB group
    ASSIGN_OR_RETURN(auto caps, mutable_thermal->AddChildGroup("capabilities"));

    // release thermal lock & acquire info lock
    mutable_thermal = nullptr;
    auto mutable_caps = caps->AcquireMutable();

    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_temperature",
        datasource->GetCapTemp()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_warning_threshold",
        datasource->GetCapWarnThresh()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_error_threshold",
        datasource->GetCapErrThresh()));
    RETURN_IF_ERROR(mutable_caps->AddAttribute("get_shutdown_threshold",
        datasource->GetCapShutdownThresh()));

    return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum

