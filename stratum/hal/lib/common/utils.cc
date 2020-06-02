// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#include "stratum/hal/lib/common/utils.h"

#include <cfenv>  // NOLINT
#include <cmath>
#include <sstream>  // IWYU pragma: keep
#include <regex>  // NOLINT

#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/public/lib/error.h"
#include "stratum/public/proto/error.pb.h"

namespace stratum {
namespace hal {

std::string PrintNode(const Node& n) {
  return PrintNodeProperties(n.id(), n.slot(), n.index());
}

std::string PrintSingletonPort(const SingletonPort& p) {
  return PrintPortProperties(p.node(), p.id(), p.slot(), p.port(), p.channel(),
                             /*unit=*/-1, /*logical_port=*/-1, p.speed_bps());
}

std::string PrintTrunkPort(const TrunkPort& p) {
  return PrintTrunkProperties(p.node(), p.id(),
                              /*unit=*/-1, /*trunk_port=*/-1, /*speed_bps*/ 0);
}

std::string PrintNodeProperties(uint64 id, int slot, int index) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (id > 0) {
    buffer << sep << "id: " << id;
    sep = ", ";
  }
  buffer << sep << "slot: " << slot;
  if (index > 0) {
    buffer << ", index: " << index;
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintPortProperties(uint64 node_id, uint32 port_id, int slot,
                                int port, int channel, int unit,
                                int logical_port, uint64 speed_bps) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (node_id > 0) {
    buffer << sep << "node_id: " << node_id;
    sep = ", ";
  }
  if (port_id > 0) {
    buffer << sep << "port_id: " << port_id;
    sep = ", ";
  }
  buffer << sep << "slot: " << slot << ", port: " << port;
  sep = ", ";
  if (channel > 0) {
    buffer << ", channel: " << channel;
  }
  if (unit >= 0) {
    buffer << ", unit: " << unit;
  }
  if (logical_port >= 0) {
    buffer << ", logical_port: " << logical_port;
  }
  if (speed_bps > 0) {
    buffer << ", speed: " << speed_bps / kBitsPerGigabit << "G";
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintTrunkProperties(uint64 node_id, uint32 trunk_id, int unit,
                                 int trunk_port, uint64 speed_bps) {
  std::stringstream buffer;
  std::string sep = "";
  buffer << "(";
  if (node_id > 0) {
    buffer << sep << "node_id: " << node_id;
    sep = ", ";
  }
  if (trunk_id > 0) {
    buffer << sep << "trunk_id: " << trunk_id;
    sep = ", ";
  }
  if (unit >= 0) {
    buffer << sep << "unit: " << unit;
    sep = ", ";
  }
  if (trunk_port >= 0) {
    buffer << sep << "trunk_port: " << trunk_port;
    sep = ", ";
  }
  if (speed_bps > 0) {
    buffer << sep << "speed: " << speed_bps / kBitsPerGigabit << "G";
  }
  buffer << ")";

  return buffer.str();
}

std::string PrintPortState(PortState state) {
  switch (state) {
    case PORT_STATE_UP:
      return "UP";
    case PORT_STATE_DOWN:
      return "DOWN";
    case PORT_STATE_FAILED:
      return "FAILED";
    default:
      return "UNKNOWN";
  }
}

SingletonPort BuildSingletonPort(int slot, int port, int channel,
                                 uint64 speed_bps) {
  SingletonPort singleton_port;
  singleton_port.set_slot(slot);
  singleton_port.set_port(port);
  singleton_port.set_channel(channel);
  singleton_port.set_speed_bps(speed_bps);
  return singleton_port;
}

PortLedConfig FindPortLedColorAndState(AdminState admin_state,
                                       PortState oper_state,
                                       HealthState health_state,
                                       TrunkMemberBlockState block_state) {
  if (admin_state != ADMIN_STATE_ENABLED) {
    // Admin disabled overrides other states.
    return {LED_COLOR_AMBER, LED_STATE_SOLID};
  } else if (oper_state != PORT_STATE_UP) {
    // A port which is admin enabled but oper down. We turn off the LEDs in this
    // case.
    return {LED_COLOR_GREEN, LED_STATE_OFF};
  } else if (block_state == TRUNK_MEMBER_BLOCK_STATE_BLOCKED) {
    // A port which is admin enabled, oper up, part of a trunk, and blocked
    // (e.g., as part of LACP protocol). Note that if the port is not part of a
    // trunk block_state will be TRUNK_MEMBER_BLOCK_STATE_UNKNOWN.
    return {LED_COLOR_GREEN, LED_STATE_BLINKING_SLOW};
  } else if (health_state == HEALTH_STATE_GOOD) {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and healthy (e.g. no neighbor
    // mismatch detected).
    return {LED_COLOR_GREEN, LED_STATE_SOLID};
  } else if (health_state == HEALTH_STATE_BAD) {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and unhealthy (e.g. there is a
    // neighbor mismatch).
    return {LED_COLOR_AMBER, LED_STATE_BLINKING_FAST};
  } else {
    // A port which is admin enabled, oper up, either part of a trunk and
    // forwarding or not part of a trunk, and has unknown health state
    // (e.g. when the neighbor status of the port is not known to controller).
    return {LED_COLOR_GREEN, LED_STATE_BLINKING_FAST};
  }
}

PortLedConfig AggregatePortLedColorsStatePairs(
    const std::vector<PortLedConfig>& color_state_pairs) {
  if (color_state_pairs.empty()) {
    return {LED_COLOR_UNKNOWN, LED_STATE_UNKNOWN};
  }

  auto it = color_state_pairs.begin();
  LedColor aggregate_color = it->first;
  LedState aggregate_state = it->second;
  it++;
  while (it != color_state_pairs.end()) {
    if (aggregate_color != it->first || aggregate_state != it->second) {
      // If we have a conflict, show blinking amber if there is at least one
      // blinking amber and show solid amber otherwise.
      if ((aggregate_color == LED_COLOR_AMBER &&
           (aggregate_state == LED_STATE_BLINKING_SLOW ||
            aggregate_state == LED_STATE_BLINKING_FAST)) ||
          (it->first == LED_COLOR_AMBER &&
           (it->second == LED_STATE_BLINKING_SLOW ||
            it->second == LED_STATE_BLINKING_FAST))) {
        aggregate_state = LED_STATE_BLINKING_SLOW;
      } else {
        aggregate_state = LED_STATE_SOLID;
      }
      aggregate_color = LED_COLOR_AMBER;
    }
    it++;
  }

  return {aggregate_color, aggregate_state};
}

std::string ConvertHwStateToString(const HwState& state) {
  switch (state) {
    case HW_STATE_READY:
      return "UP";
    case HW_STATE_NOT_PRESENT:
      return "NOT_PRESENT";
    case HW_STATE_OFF:
      return "DORMANT";
    case HW_STATE_PRESENT:
    case HW_STATE_CONFIGURED_OFF:
      return "DOWN";
    case HW_STATE_FAILED:
      return "LOWER_LAYER_DOWN";
    case HW_STATE_DIAGNOSTIC:
      return "TESTING";
    default:
      return "UNKNOWN";
  }
}

std::string ConvertPortStateToString(const PortState& state) {
  switch (state) {
    case PORT_STATE_UP:
      return "UP";
    case PORT_STATE_DOWN:
      return "DOWN";
    case PORT_STATE_FAILED:
      return "LOWER_LAYER_DOWN";
    case PORT_STATE_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

std::string ConvertAdminStateToString(const AdminState& state) {
  switch (state) {
    case ADMIN_STATE_ENABLED:
      return "UP";
    case ADMIN_STATE_DISABLED:
      return "DOWN";
    case ADMIN_STATE_DIAG:
      return "TESTING";
    case ADMIN_STATE_UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

std::string ConvertSpeedBpsToString(
    const ::google::protobuf::uint64& speed_bps) {
  switch (speed_bps) {
    case kTenGigBps:
      return "SPEED_10GB";
    case kTwentyGigBps:
      return "SPEED_20GB";
    case kTwentyFiveGigBps:
      return "SPEED_25GB";
    case kFortyGigBps:
      return "SPEED_40GB";
    case kFiftyGigBps:
      return "SPEED_50GB";
    case kHundredGigBps:
      return "SPEED_100GB";
    default:
      return "SPEED_UNKNOWN";
  }
}

::google::protobuf::uint64 ConvertStringToSpeedBps(
    const std::string& speed_string) {
  if (speed_string.compare("SPEED_10GB") == 0) {
    return kTenGigBps;
  } else if (speed_string.compare("SPEED_20GB") == 0) {
    return kTwentyGigBps;
  } else if (speed_string.compare("SPEED_25GB") == 0) {
    return kTwentyFiveGigBps;
  } else if (speed_string.compare("SPEED_40GB") == 0) {
    return kFortyGigBps;
  } else if (speed_string.compare("SPEED_50GB") == 0) {
    return kFiftyGigBps;
  } else if (speed_string.compare("SPEED_100GB") == 0) {
    return kHundredGigBps;
  } else {
    return 0LL;
  }
}

std::string ConvertAlarmSeverityToString(const Alarm::Severity& severity) {
  switch (severity) {
    case Alarm::MINOR:
      return "MINOR";
    case Alarm::WARNING:
      return "WARNING";
    case Alarm::MAJOR:
      return "MAJOR";
    case Alarm::CRITICAL:
      return "CRITICAL";
    case Alarm::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

std::string ConvertHealthStateToString(const HealthState& state) {
  switch (state) {
    case HEALTH_STATE_GOOD:
      return "GOOD";
    case HEALTH_STATE_BAD:
      return "BAD";
    default:
      return "UNKNOWN";
  }
}

bool ConvertTrunkMemberBlockStateToBool(const TrunkMemberBlockState& state) {
  return state == TRUNK_MEMBER_BLOCK_STATE_FORWARDING;
}

std::string MacAddressToYangString(
    const ::google::protobuf::uint64& mac_address) {
  return absl::StrFormat("%x:%x:%x:%x:%x:%x", (mac_address >> 40) & 0xFF,
                         (mac_address >> 32) & 0xFF, (mac_address >> 24) & 0xFF,
                         (mac_address >> 16) & 0xFF, (mac_address >> 8) & 0xFF,
                         mac_address & 0xFF);
}

::google::protobuf::uint64 YangStringToMacAddress(
    const std::string& yang_string) {
  std::string tmp_str = yang_string;
  // Remove colons
  tmp_str.erase(std::remove(tmp_str.begin(), tmp_str.end(), ':'),
                tmp_str.end());
  return strtoull(tmp_str.c_str(), NULL, 16);
}

bool IsMacAddressValid(const std::string& mac_address) {
  const std::regex mac_address_regex(kMacAddressRegex);
  return regex_match(mac_address, mac_address_regex);
}

bool IsPortAutonegEnabled(const TriState& state) {
    return state == TriState::TRI_STATE_TRUE;
}

bool IsAdminStateEnabled(const AdminState& admin_state) {
    return admin_state == AdminState::ADMIN_STATE_ENABLED;
}

bool IsLoopbackStateEnabled(const LoopbackState& loopback_state) {
  switch (loopback_state) {
    case LOOPBACK_STATE_MAC:
    case LOOPBACK_STATE_PHY:
      return true;
    default:
      return false;
  }
}

std::string ConvertMediaTypeToString(const MediaType& type) {
  switch (type) {
    case MEDIA_TYPE_SFP:
      return "SFP";
    case MEDIA_TYPE_CFP_COPPER:
    case MEDIA_TYPE_CFP_LR4:
      return "CFP";
    case MEDIA_TYPE_QSFP_PSM4:
    case MEDIA_TYPE_QSFP_SR4:
    case MEDIA_TYPE_QSFP_LR4:
    case MEDIA_TYPE_QSFP_CLR4:
      return "QSFP28";
    case MEDIA_TYPE_QSFP_CSR4:
      return "QSFP_PLUS";
    case MEDIA_TYPE_QSFP_COPPER:
    case MEDIA_TYPE_QSFP_CCR4:
      return "QSFP";

    default:
      return "UNKNOWN";
  }
}

std::string ConvertHwStateToPresentString(const HwState& hw_state) {
  switch (hw_state) {
    case HW_STATE_READY:
    case HW_STATE_OFF:
    case HW_STATE_PRESENT:
    case HW_STATE_CONFIGURED_OFF:
    case HW_STATE_FAILED:
    case HW_STATE_DIAGNOSTIC:
    case HW_STATE_UNKNOWN:
      return "PRESENT";
    case HW_STATE_NOT_PRESENT:
      return "NOT_PRESENT";
    default:
      return "UNKNOWN";
  }
}

::util::StatusOr<double> ConvertDecimal64ToDouble(
    const ::gnmi::Decimal64& value) {
  std::feclearexcept(FE_ALL_EXCEPT);
  double result = value.digits() / std::pow(10, value.precision());
  if (std::feclearexcept(FE_INVALID)) {
    return MAKE_ERROR(ERR_OUT_OF_RANGE)
           << "can not convert decimal"
           << " with digits " << value.digits() << " and precision "
           << value.precision() << " to a double value.";
  }
  return result;
}

::util::StatusOr<::gnmi::Decimal64> ConvertDoubleToDecimal64(double value,
                                                             uint32 precision) {
  std::feclearexcept(FE_ALL_EXCEPT);
  ::gnmi::Decimal64 decimal;
  decimal.set_digits(std::llround(value * std::pow(10, precision)));
  decimal.set_precision(precision);
  if (std::fetestexcept(FE_INVALID)) {
    return MAKE_ERROR(ERR_OUT_OF_RANGE)
           << "can not convert number " << value << " with precision "
           << precision << " to a Decimal64 value";
  }
  return decimal;
}

::gnmi::Decimal64 ConvertDoubleToDecimal64OrDie(const double& value) {
  auto status = ConvertDoubleToDecimal64(value);
  CHECK(status.ok());
  return status.ConsumeValueOrDie();
}

uint64 ConvertHzToMHz(const uint64& val) {
  return val / 1000000;
}

uint64 ConvertMHzToHz(const uint64& val) {
  return val * 1000000;
}

}  // namespace hal
}  // namespace stratum
