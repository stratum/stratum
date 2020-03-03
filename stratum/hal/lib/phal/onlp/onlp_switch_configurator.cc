// Copyright 2019 Dell EMC
// Copyright 2020 Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/phal/onlp/onlp_switch_configurator.h"

#include <string>
#include <utility>
#include <vector>

#include "stratum/glue/gtl/map_util.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/phal/onlp/onlp_fan_datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_led_datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_psu_datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_sfp_configurator.h"
#include "stratum/hal/lib/phal/onlp/onlp_sfp_datasource.h"
#include "stratum/hal/lib/phal/onlp/onlp_thermal_datasource.h"
#include "stratum/hal/lib/phal/phal.pb.h"

namespace stratum {
namespace hal {
namespace phal {
namespace onlp {

// Make an instance of OnlpSwitchConfigurator
::util::StatusOr<std::unique_ptr<OnlpSwitchConfigurator>>
OnlpSwitchConfigurator::Make(OnlpPhalInterface* phal_interface,
                             OnlpInterface* onlp_interface) {
  // Make sure we've got a valid Onlp Interface
  CHECK_RETURN_IF_FALSE(onlp_interface != nullptr);

  return absl::WrapUnique(
      new OnlpSwitchConfigurator(phal_interface, onlp_interface));
}

// Generate a default config using the OID list from the NOS
// The procedure is similar for each type:
//  1. Get OID list for type, e.g. SFP
//  2. Get ID from OID, usually increasing from 1
//  3. Add instance to config with default cache policy
::util::Status OnlpSwitchConfigurator::CreateDefaultConfig(
    PhalInitConfig* phal_config) const {
  // Handle SFPs
  // Add a new card
  // TODO(unknown): what about multiple cards?
  auto card = phal_config->add_cards();
  ASSIGN_OR_RETURN(auto oids,
                   onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_SFP));
  for (const auto& oid : oids) {
    ASSIGN_OR_RETURN(const auto sfp_info, onlp_interface_->GetSfpInfo(oid));
    // Don't add to card yet, because init could fail.
    PhalCardConfig::Port port;
    port.set_port(ONLP_OID_ID_GET(oid));
    // See if we've got an sfp type and set the physical port type
    switch (sfp_info.GetSfpType()) {
      case SFP_TYPE_SFP28:
      case SFP_TYPE_SFP:
        port.set_physical_port_type(PHYSICAL_PORT_TYPE_SFP_CAGE);
        break;
      case SFP_TYPE_QSFP:
      case SFP_TYPE_QSFP_PLUS:
      case SFP_TYPE_QSFP28:
        port.set_physical_port_type(PHYSICAL_PORT_TYPE_QSFP_CAGE);
        break;
      default:
        LOG(ERROR) << "Unknown SFP type: " << sfp_info.GetSfpType()
                   << " on port with OID " << oid << ".";
        break;
    }
    *card->add_ports() = port;
  }

  // Handle fans
  auto fan_tray = phal_config->add_fan_trays();
  ASSIGN_OR_RETURN(oids, onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_FAN));
  for (const auto& oid : oids) {
    auto fan = fan_tray->add_fans();
    fan->set_slot(ONLP_OID_ID_GET(oid));
  }

  // Handle PSUs
  auto psu_tray = phal_config->add_psu_trays();
  ASSIGN_OR_RETURN(oids, onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_PSU));
  for (const auto& oid : oids) {
    auto psu = psu_tray->add_psus();
    psu->set_slot(ONLP_OID_ID_GET(oid));
  }

  // Handle LEDs
  auto led_group = phal_config->add_led_groups();
  ASSIGN_OR_RETURN(oids, onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_LED));
  for (const auto& oid : oids) {
    auto led = led_group->add_leds();
    led->set_led_index(ONLP_OID_ID_GET(oid));
  }

  // Handle Thermals
  auto thermal_group = phal_config->add_thermal_groups();
  ASSIGN_OR_RETURN(oids,
                   onlp_interface_->GetOidList(ONLP_OID_TYPE_FLAG_THERMAL));
  for (const auto& oid : oids) {
    auto thermal = thermal_group->add_thermals();
    thermal->set_thermal_index(ONLP_OID_ID_GET(oid));
  }

  return ::util::OkStatus();
}

// Configure the switch's attribute database with the given
// PhalInitConfig config.
::util::Status OnlpSwitchConfigurator::ConfigurePhalDB(
    PhalInitConfig* phal_config, AttributeGroup* root) {
  // Lock the root group
  auto mutable_root = root->AcquireMutable();

  // Add cards
  for (auto& card_config : *phal_config->mutable_cards()) {
    ASSIGN_OR_RETURN(auto card, mutable_root->AddRepeatedChildGroup("cards"));
    std::unique_ptr<MutableAttributeGroup> mutable_card =
        card->AcquireMutable();

    // Use chassis cache policy if we have no card policy
    if (!card_config.has_cache_policy()) {
      card_config.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }

    // Add ports per card
    for (auto& port_config : *card_config.mutable_ports()) {
      // Use card cache policy if we have no port policy
      if (!port_config.has_cache_policy()) {
        port_config.set_allocated_cache_policy(
            new CachePolicyConfig(card_config.cache_policy()));
      }

      // Add Port to attribute DB
      AddPort(card_config.slot(), port_config.port(), mutable_card.get(),
              port_config);
    }
  }

  // Add Fans
  for (auto& fan_tray_config : *phal_config->mutable_fan_trays()) {
    // Add Fan Tray to attribute DB
    ASSIGN_OR_RETURN(auto fan_tray,
                     mutable_root->AddRepeatedChildGroup("fan_trays"));
    auto mutable_fan_tray = fan_tray->AcquireMutable();

    // Use chassis cache policy if we have no fan tray policy
    if (!fan_tray_config.has_cache_policy()) {
      fan_tray_config.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }

    // Add Fans per tray
    for (auto& fan_config : *fan_tray_config.mutable_fans()) {
      // Use fan tray policy if we have no fan policy
      if (!fan_config.has_cache_policy()) {
        fan_config.set_allocated_cache_policy(
            new CachePolicyConfig(fan_tray_config.cache_policy()));
      }

      // Add Fan to attribute DB
      AddFan(fan_config.slot(), mutable_fan_tray.get(), fan_config);
    }
  }

  // Add PSUs
  for (auto& psu_tray_config : *phal_config->mutable_psu_trays()) {
    // Add PSU Tray to attribute DB
    ASSIGN_OR_RETURN(auto psu_tray,
                     mutable_root->AddRepeatedChildGroup("psu_trays"));
    auto mutable_psu_tray = psu_tray->AcquireMutable();

    // Use chassis cache policy if we have no psu tray policy
    if (!psu_tray_config.has_cache_policy()) {
      psu_tray_config.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }

    // Add PSUs per tray
    for (auto& psu_config : *psu_tray_config.mutable_psus()) {
      // Use psu tray policy if we have no psu policy
      if (!psu_config.has_cache_policy()) {
        psu_config.set_allocated_cache_policy(
            new CachePolicyConfig(psu_tray_config.cache_policy()));
      }

      // Add Psu to attribute DB
      AddPsu(psu_config.slot(), mutable_psu_tray.get(), psu_config);
    }
  }

  // Add LEDs
  for (auto led_group_config : *phal_config->mutable_led_groups()) {
    // Add LED Group to attribute DB
    ASSIGN_OR_RETURN(auto group,
                     mutable_root->AddRepeatedChildGroup("led_groups"));
    auto mutable_group = group->AcquireMutable();

    // Use chassis cache policy if we have no led group policy
    if (!led_group_config.has_cache_policy()) {
      led_group_config.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }

    // Add LEDs
    for (auto& led_config : *led_group_config.mutable_leds()) {
      // Use card policy if we have no led group policy
      if (!led_config.has_cache_policy()) {
        led_config.set_allocated_cache_policy(
            new CachePolicyConfig(led_group_config.cache_policy()));
      }

      // Add Led to attribute DB
      AddLed(led_config.led_index(), mutable_group.get(), led_config);
    }
  }

  // Add Thermals
  for (auto thermal_group_config : *phal_config->mutable_thermal_groups()) {
    // Add Thermal Group to attribute DB
    ASSIGN_OR_RETURN(auto group,
                     mutable_root->AddRepeatedChildGroup("thermal_groups"));
    auto mutable_group = group->AcquireMutable();

    // Use chassis cache policy if we have no thermal group policy
    if (!thermal_group_config.has_cache_policy()) {
      thermal_group_config.set_allocated_cache_policy(
          new CachePolicyConfig(phal_config->cache_policy()));
    }

    // Add Thermals
    for (auto thermal_config : *thermal_group_config.mutable_thermals()) {
      // Use card policy if we have no thermal group policy
      if (!thermal_config.has_cache_policy()) {
        thermal_config.set_allocated_cache_policy(
            new CachePolicyConfig(thermal_group_config.cache_policy()));
      }

      // Add Thermal to attribute DB
      AddThermal(thermal_config.thermal_index(), mutable_group.get(),
                 thermal_config);
    }
  }

  return ::util::OkStatus();
}

::util::StatusOr<OidInfo> OnlpSwitchConfigurator::GetOidInfo(
    AttributeGroup* group, OnlpOid oid) const {
  // Check device info
  auto result = onlp_interface_->GetOidInfo(oid);
  if (!result.ok()) {
    LOG(ERROR) << "failed to GetOidInfo for " << std::to_string(oid) << ": "
               << result.status().error_message();

    auto mutable_group = group->AcquireMutable();
    mutable_group->AddAttribute(
        "id", FixedDataSource<int>::Make(ONLP_OID_ID_GET(oid))->GetAttribute());
    std::string err_msg =
        "Failed to get oid info for oid: " + std::to_string(oid) +
        " error code: " + std::to_string(result.status().error_code());
    mutable_group->AddAttribute(
        "err_msg", FixedDataSource<std::string>::Make(err_msg)->GetAttribute());
    mutable_group->AddAttribute(
        "hardware_state",
        FixedEnumDataSource::Make(HwState_descriptor(), HW_STATE_FAILED)
            ->GetAttribute());
  }

  return result;
}

::util::Status OnlpSwitchConfigurator::AddPort(
    int slot, int port, MutableAttributeGroup* mutable_card,
    const PhalCardConfig::Port& config) {
  // Add port to attribute DB
  ASSIGN_OR_RETURN(auto port_group,
                   mutable_card->AddRepeatedChildGroup("ports"));
  auto mutable_port = port_group->AcquireMutable();

  // Create a transceiver group in the Phal DB
  ASSIGN_OR_RETURN(auto sfp, mutable_port->AddChildGroup("transceiver"));

  // Check to make sure port exists
  // Note: will need to figure out how to map slot and port
  //       into an OID, for now we ignore slot.
  ASSIGN_OR_RETURN(auto oid_info, GetOidInfo(sfp, ONLP_SFP_ID_CREATE(port)));

  // If it's an SFP/QSFP then the transceiver data source
  // will be added dynamically upon insertion
  switch (config.physical_port_type()) {
    case PHYSICAL_PORT_TYPE_SFP_CAGE:
    case PHYSICAL_PORT_TYPE_QSFP_CAGE: {
      // Create Caching policy
      ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                       config.cache_policy().type(),
                                       config.cache_policy().timed_value()));

      // Create a new data source
      ASSIGN_OR_RETURN(auto datasource,
                       OnlpSfpDataSource::Make(port, onlp_interface_, cache));

      // Create an SFP Configurator
      ASSIGN_OR_RETURN(auto configurator,
                       OnlpSfpConfigurator::Make(datasource, sfp,
                                                ONLP_SFP_ID_CREATE(port)));

      // Register configurator as callback to ONLP
      RETURN_IF_ERROR(
          onlp_phal_interface_->RegisterOnlpEventCallback(configurator.get()));

      // Save it in the database
      auto mutable_sfp = sfp->AcquireMutable();
      mutable_sfp->AddRuntimeConfigurator(std::move(configurator));
      break;
    }
    // All other port types
    default:
      LOG(INFO) << "card[" << slot << "]/port[" << port
                << "]: transceiver type "
                << PhysicalPortType_descriptor()
                       ->FindValueByNumber(config.physical_port_type())
                       ->name()
                << " not handled.";
      break;
  }

  return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddFan(
    int id, MutableAttributeGroup* mutable_fan_tray,
    const PhalFanTrayConfig::Fan& config) {
  // Add Fan to Fan Trays in the Phal DB
  // note: using a 1-based id for the index
  ASSIGN_OR_RETURN(auto fan, mutable_fan_tray->AddRepeatedChildGroup("fans"));

  // Check to make sure we haven't already added this id
  CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(&fan_id_map_, id, true))
      << "duplicate fan id: " << id;

  ASSIGN_OR_RETURN(OidInfo oid_info, GetOidInfo(fan, ONLP_FAN_ID_CREATE(id)));
  auto mutable_fan = fan->AcquireMutable();

  // Create Caching policy
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  // Create a new data source
  ASSIGN_OR_RETURN(std::shared_ptr<OnlpFanDataSource> datasource,
                   OnlpFanDataSource::Make(id, onlp_interface_, cache));

  // Add Fan Attributes
  RETURN_IF_ERROR(mutable_fan->AddAttribute("id", datasource->GetFanId()));
  RETURN_IF_ERROR(
      mutable_fan->AddAttribute("description", datasource->GetFanDesc()));
  RETURN_IF_ERROR(mutable_fan->AddAttribute("hardware_state",
                                            datasource->GetFanHardwareState()));

  // Other attributes only valid when device is present
  if (!oid_info.Present()) {
    return ::util::OkStatus();
  }

  // Other attributes will only be valid when the device is present
  RETURN_IF_ERROR(mutable_fan->AddAttribute("rpm", datasource->GetFanRPM()));
  RETURN_IF_ERROR(mutable_fan->AddAttribute("speed_control",
                                            datasource->GetFanPercentage()));
  RETURN_IF_ERROR(
      mutable_fan->AddAttribute("direction", datasource->GetFanDirection()));

  // Get info DB group
  ASSIGN_OR_RETURN(auto info, mutable_fan->AddChildGroup("info"));

  // release fan lock & acquire info lock
  mutable_fan = nullptr;
  auto mutable_info = info->AcquireMutable();

  // We'll map model to info.part_no in the DB
  RETURN_IF_ERROR(
      mutable_info->AddAttribute("part_no", datasource->GetFanModel()));
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

  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("set_dir", datasource->GetCapSetDir()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_dir", datasource->GetCapGetDir()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("set_rpm", datasource->GetCapSetRpm()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "set_percentage", datasource->GetCapSetPercentage()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_rpm", datasource->GetCapGetRpm()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "get_percentage", datasource->GetCapGetPercentage()));

  return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddPsu(
    int id, MutableAttributeGroup* mutable_psu_tray,
    const PhalPsuTrayConfig::Psu& config) {
  // Add Psu to Psu Trays in the Phal DB
  // note: using a 1-based id for the index
  ASSIGN_OR_RETURN(auto psu, mutable_psu_tray->AddRepeatedChildGroup("psus"));

  // Check to make sure we haven't already added this id
  CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(&psu_id_map_, id, true))
      << "duplicate psu id: " << id;

  ASSIGN_OR_RETURN(OidInfo oid_info, GetOidInfo(psu, ONLP_PSU_ID_CREATE(id)));
  auto mutable_psu = psu->AcquireMutable();

  // Create Caching policy
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  // Create Psu data source
  ASSIGN_OR_RETURN(std::shared_ptr<OnlpPsuDataSource> datasource,
                   OnlpPsuDataSource::Make(id, onlp_interface_, cache));

  // Add Psu Attributes
  RETURN_IF_ERROR(mutable_psu->AddAttribute("id", datasource->GetPsuId()));
  RETURN_IF_ERROR(
      mutable_psu->AddAttribute("description", datasource->GetPsuDesc()));
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
  RETURN_IF_ERROR(
      mutable_psu->AddAttribute("input_power", datasource->GetPsuInputPower()));
  RETURN_IF_ERROR(mutable_psu->AddAttribute("output_power",
                                            datasource->GetPsuOutputPower()));
  RETURN_IF_ERROR(mutable_psu->AddAttribute("type", datasource->GetPsuType()));

  // Get info DB group
  ASSIGN_OR_RETURN(auto info, mutable_psu->AddChildGroup("info"));

  // release psu lock & acquire info lock
  mutable_psu = nullptr;
  auto mutable_info = info->AcquireMutable();

  // We'll map model to info.part_no in the DB
  RETURN_IF_ERROR(
      mutable_info->AddAttribute("part_no", datasource->GetPsuModel()));
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

  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_type", datasource->GetCapGetType()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_vin", datasource->GetCapGetVIn()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_vout", datasource->GetCapGetVOut()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_iin", datasource->GetCapGetIIn()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_iout", datasource->GetCapGetIOut()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_pin", datasource->GetCapGetPIn()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_pout", datasource->GetCapGetPOut()));

  return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddLed(
    int id, MutableAttributeGroup* mutable_group,
    const PhalLedGroupConfig_Led& config) {
  // Add Led to the Phal DB
  // note: using a 1-based id for the index
  ASSIGN_OR_RETURN(auto led, mutable_group->AddRepeatedChildGroup("leds"));

  // Check to make sure we haven't already added this id
  CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(&led_id_map_, id, true))
      << "duplicate led id: " << id;

  ASSIGN_OR_RETURN(OidInfo oid_info, GetOidInfo(led, ONLP_LED_ID_CREATE(id)));
  auto mutable_led = led->AcquireMutable();

  // Create Caching policy
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  // Create data source
  ASSIGN_OR_RETURN(std::shared_ptr<OnlpLedDataSource> datasource,
                   OnlpLedDataSource::Make(id, onlp_interface_, cache));

  // Add Led Attributes
  RETURN_IF_ERROR(mutable_led->AddAttribute("id", datasource->GetLedId()));
  RETURN_IF_ERROR(
      mutable_led->AddAttribute("description", datasource->GetLedDesc()));
  RETURN_IF_ERROR(mutable_led->AddAttribute("hardware_state",
                                            datasource->GetLedHardwareState()));

  // Other attributes only valid when device is present
  if (!oid_info.Present()) {
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(mutable_led->AddAttribute("mode", datasource->GetLedMode()));
  RETURN_IF_ERROR(
      mutable_led->AddAttribute("character", datasource->GetLedChar()));

  // RETURN_IF_ERROR(mutable_led->AddAttribute("state",
  //    datasource->GetLedState()));
  // RETURN_IF_ERROR(mutable_led->AddAttribute("color",
  //    datasource->GetLedColor()));

  // Get capabilities DB group
  ASSIGN_OR_RETURN(auto caps, mutable_led->AddChildGroup("capabilities"));

  // release psu lock & acquire info lock
  mutable_led = nullptr;
  auto mutable_caps = caps->AcquireMutable();

  RETURN_IF_ERROR(mutable_caps->AddAttribute("off", datasource->GetCapOff()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("auto", datasource->GetCapAuto()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("auto_blinking",
                                             datasource->GetCapAutoBlinking()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("char", datasource->GetCapChar()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("red", datasource->GetCapRed()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("red_blinking",
                                             datasource->GetCapRedBlinking()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("orange", datasource->GetCapOrange()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "orange_blinking", datasource->GetCapOrangeBlinking()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("yellow", datasource->GetCapYellow()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "yellow_blinking", datasource->GetCapYellowBlinking()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("green", datasource->GetCapGreen()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "green_blinking", datasource->GetCapGreenBlinking()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("blue", datasource->GetCapBlue()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("blue_blinking",
                                             datasource->GetCapBlueBlinking()));
  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("purple", datasource->GetCapPurple()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "purple_blinking", datasource->GetCapPurpleBlinking()));

  return ::util::OkStatus();
}

::util::Status OnlpSwitchConfigurator::AddThermal(
    int id, MutableAttributeGroup* mutable_group,
    const PhalThermalGroupConfig_Thermal& config) {
  // Add Thermal to the Phal DB
  // note: using a 1-based id for the index
  ASSIGN_OR_RETURN(auto thermal,
                   mutable_group->AddRepeatedChildGroup("thermals"));

  // Check to make sure we haven't already added this id
  CHECK_RETURN_IF_FALSE(gtl::InsertIfNotPresent(&thermal_id_map_, id, true))
      << "duplicate thermal id: " << id;

  ASSIGN_OR_RETURN(OidInfo oid_info,
                   GetOidInfo(thermal, ONLP_THERMAL_ID_CREATE(id)));
  auto mutable_thermal = thermal->AcquireMutable();

  // Create Caching policy
  ASSIGN_OR_RETURN(auto cache, CachePolicyFactory::CreateInstance(
                                   config.cache_policy().type(),
                                   config.cache_policy().timed_value()));

  // Create data source
  ASSIGN_OR_RETURN(std::shared_ptr<OnlpThermalDataSource> datasource,
                   OnlpThermalDataSource::Make(id, onlp_interface_, cache));

  // Add Thermal Attributes
  RETURN_IF_ERROR(
      mutable_thermal->AddAttribute("id", datasource->GetThermalId()));
  RETURN_IF_ERROR(mutable_thermal->AddAttribute("description",
                                                datasource->GetThermalDesc()));
  RETURN_IF_ERROR(mutable_thermal->AddAttribute(
      "hardware_state", datasource->GetThermalHardwareState()));

  // Other attributes only valid when device is present
  if (!oid_info.Present()) {
    return ::util::OkStatus();
  }

  RETURN_IF_ERROR(mutable_thermal->AddAttribute(
      "cur_temp", datasource->GetThermalCurTemp()));
  RETURN_IF_ERROR(mutable_thermal->AddAttribute(
      "warn_temp", datasource->GetThermalWarnTemp()));
  RETURN_IF_ERROR(mutable_thermal->AddAttribute(
      "error_temp", datasource->GetThermalErrorTemp()));
  RETURN_IF_ERROR(mutable_thermal->AddAttribute(
      "shut_down_temp", datasource->GetThermalShutDownTemp()));

  // Get capabilities DB group
  ASSIGN_OR_RETURN(auto caps, mutable_thermal->AddChildGroup("capabilities"));

  // release thermal lock & acquire info lock
  mutable_thermal = nullptr;
  auto mutable_caps = caps->AcquireMutable();

  RETURN_IF_ERROR(
      mutable_caps->AddAttribute("get_temperature", datasource->GetCapTemp()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("get_warning_threshold",
                                             datasource->GetCapWarnThresh()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute("get_error_threshold",
                                             datasource->GetCapErrThresh()));
  RETURN_IF_ERROR(mutable_caps->AddAttribute(
      "get_shutdown_threshold", datasource->GetCapShutdownThresh()));

  return ::util::OkStatus();
}

}  // namespace onlp
}  // namespace phal
}  // namespace hal

}  // namespace stratum
