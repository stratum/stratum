// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0


#ifndef STRATUM_HAL_LIB_COMMON_UTILS_H_
#define STRATUM_HAL_LIB_COMMON_UTILS_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "gnmi/gnmi.pb.h"

namespace stratum {
namespace hal {

// PortKey is a generic data structure which is meant to be used as a key that
// uniquely identifies a "port" in stratum code base. By "port" we mean a
// singleton port, a transceiver port, a flex/non-flex port group (i.e. a set
// of ports with the same (slot, port) which are either flex or non-flex), etc.
// The port whose key is identified by this data strcuture can be channelized (
// in which case there is a non-zero channel number), or non-channelized (in
// which case channel number is set to zero). In case the key is used when the
// channel is not important at all (e.g. when the key identifies a flex or
// non-flex port group), we use the default value of -1 for the channel.
struct PortKey {
  int slot;
  int port;
  int channel;
  PortKey(int _slot, int _port, int _channel)
      : slot(_slot), port(_port), channel(_channel) {}
  PortKey(int _slot, int _port) : slot(_slot), port(_port), channel(-1) {}
  PortKey() : slot(-1), port(-1), channel(-1) {}
  bool operator<(const PortKey& other) const {
    return (slot < other.slot ||
            (slot == other.slot &&
             (port < other.port ||
              (port == other.port && channel < other.channel))));
  }
  bool operator==(const PortKey& other) const {
    return (slot == other.slot && port == other.port &&
            channel == other.channel);
  }
  std::string ToString() const {
    if (channel > 0) {
      return absl::StrCat("(slot: ", slot, ", port: ", port,
                          ", channel: ", channel, ")");
    } else {
      return absl::StrCat("(slot: ", slot, ", port: ", port, ")");
    }
  }
};

// A custom hash functor for SingletonPort proto message in common.proto.
class SingletonPortHash {
 public:
  size_t operator()(const SingletonPort& port) const {
    size_t hash_val = 0;
    std::hash<int> integer_hasher;
    hash_val ^= integer_hasher(port.slot());
    hash_val ^= integer_hasher(port.port());
    hash_val ^= integer_hasher(port.channel());
    // Use middle 32 bits of speed_bps
    hash_val ^=
        integer_hasher(static_cast<int>((port.speed_bps() >> 16) & 0xFFFFFFFF));
    return hash_val;
  }
};

// A custom equal functor for SingletonPort proto messages in common.proto.
class SingletonPortEqual {
 public:
  bool operator()(const SingletonPort& lhs, const SingletonPort& rhs) const {
    return (lhs.slot() == rhs.slot()) && (lhs.port() == rhs.port()) &&
           (lhs.channel() == rhs.channel()) &&
           (lhs.speed_bps() == rhs.speed_bps());
  }
};

// Functor for comparing two SingletonPort instances based on slot, port,
// channel and speed_bps values in that order. Returns true if the first
// argument precedes the second in order, false otherwise.
class SingletonPortLess {
 public:
  // Returns true if the first argument precedes the second; false otherwise.
  bool operator()(const SingletonPort& x, const SingletonPort& y) const {
    return ComparePorts(x, y);
  }

 private:
  // Compares slot, port, channel and speed_bps in that order.
  // Returns true if the first agrument precedes the second, false otherwise.
  bool ComparePorts(const SingletonPort& x, const SingletonPort& y) const {
    if (x.slot() != y.slot()) {
      return x.slot() < y.slot();
    } else if (x.port() != y.port()) {
      return x.port() < y.port();
    } else if (x.channel() != y.channel()) {
      return x.channel() < y.channel();
    } else {
      return x.speed_bps() < y.speed_bps();
    }
  }
};

// Prints a Node proto message in a consistent and readable format.
std::string PrintNode(const Node& n);

// Prints a SingletonPort proto message in a consistent and readable format.
std::string PrintSingletonPort(const SingletonPort& p);

// Prints a TrunkPort proto message in a consistent and readable format.
std::string PrintTrunkPort(const TrunkPort& p);

// A set of helper functions to print a superset of node/port/trunk properties
// that are worth logging, in a consistent and readable way. These methods
// check and ignores the invalid args passed to it when printing. Other
// printer function make use of these helpers to not duplicate the priting
// logic.
std::string PrintNodeProperties(uint64 id, int slot, int index);

std::string PrintPortProperties(uint64 node_id, uint32 port_id, int slot,
                                int port, int channel, int unit,
                                int logical_port, uint64 speed_bps);

std::string PrintTrunkProperties(uint64 node_id, uint32 trunk_id, int unit,
                                 int trunk_port, uint64 speed_bps);

// Prints PortState in a consistent format.
std::string PrintPortState(PortState state);

// Builds a SingletonPort proto message with the given field values. No sanity
// checking is performed that the parameters are valid for the switch.
SingletonPort BuildSingletonPort(int slot, int port, int channel,
                                 uint64 speed_bps);

// An alias for the pair of (LedColor, LedState) for a front panel port LED.
using PortLedConfig = std::pair<LedColor, LedState>;

// A util function that translates the state(s) of a channelized/non-channelized
// singleton port to a pair of (LedColor, LedState) to be shown on the front
// panel port LED.
PortLedConfig FindPortLedColorAndState(AdminState admin_state,
                                       PortState oper_state,
                                       HealthState health_state,
                                       TrunkMemberBlockState block_state);

// A util function that aggregate the (LedColor, LedState) pairs, corresponding
// to different channels of a front panel, into one single (LedColor, LedState)
// pair. This method is used when each front panel port has only one LED and
// the per-channel (LedColor, LedState) pairs need to be aggregated to be shown
// on this single LED.
PortLedConfig AggregatePortLedColorsStatePairs(
    const std::vector<PortLedConfig>& color_state_pairs);

// A set of helper methods used to convert enums to a format used by gNMI
// collectors.

// A helper function that convert Stratum hardware state enum to string.
std::string ConvertHwStateToString(const HwState& state);

// A helper function that convert Stratum port state enum to string.
std::string ConvertPortStateToString(const PortState& state);

// A helper function that convert Stratum admin state enum to string.
std::string ConvertAdminStateToString(const AdminState& state);

// A helper function that convert speed number to string format.
std::string ConvertSpeedBpsToString(
    const ::google::protobuf::uint64& speed_bps);

// A helper function that convert OpenConfig speed string to speed number.
::google::protobuf::uint64 ConvertStringToSpeedBps(
    const std::string& speed_string);

// A helper function that convert gRPC alarm severity enum to string.
std::string ConvertAlarmSeverityToString(const Alarm::Severity& severity);

// A helper function that convert Stratum health state to string.
std::string ConvertHealthStateToString(const HealthState& state);

// A helper function that convert Stratum trunk member block state to boolean.
bool ConvertTrunkMemberBlockStateToBool(const TrunkMemberBlockState& state);

// A helper function that convert data received from the HAL into a format
// expected by the gNMI interface (MAC addresses are expected to be
// std::strings in the following format: "XX:XX:XX:XX:XX:XX").
std::string MacAddressToYangString(
    const ::google::protobuf::uint64& mac_address);

// A helper function that convert data received from the gNMI interface into a
// format expected by the HAL (MAC addresses are expected to be
// ::google::protobuf::uint64).
::google::protobuf::uint64 YangStringToMacAddress(
    const std::string& yang_string);

// A helper function that check if string of mac_address is valid.
bool IsMacAddressValid(const std::string& mac_address);

// A helper function that check if autoneg state is enabled.
bool IsPortAutonegEnabled(const TriState& state);

// A helper function that check if port admin state is enabled.
bool IsAdminStateEnabled(const AdminState& admin_state);

// A helper function that check if port loopback state is enabled.
bool IsLoopbackStateEnabled(const LoopbackState& loopback_state);

// A helper function that convert Stratum MediaType to string.
std::string ConvertMediaTypeToString(const MediaType& type);

// A helper function taht convert Stratum HwState to OpenConfig present
// state string (PRESENT, NOT_PRESENT)
std::string ConvertHwStateToPresentString(const HwState& hw_state);

// Converts ::gnmi::Decimal64 to double type.
::util::StatusOr<double> ConvertDecimal64ToDouble(
    const ::gnmi::Decimal64& value);

// Converts double to ::gnmi::Decimal64 type.
::util::StatusOr<::gnmi::Decimal64> ConvertDoubleToDecimal64(
    double value, uint32 precision = kDefaultPrecision);

// A helper method that converts a double to gNMI Decimal64.
// For use in as a 'process_func', hence no Status return type.
::gnmi::Decimal64 ConvertDoubleToDecimal64OrDie(const double& value);

// A helper method that do nothing to the value which pass to it.
// This is useful if we have a process functor in a helper function but we
// don't want to do anything to the value.
template<typename T> T DontProcess(const T& val) { return val; }

// A helper method that converts frequency from Hz to MHz.
uint64 ConvertHzToMHz(const uint64& val);

// A helper method that converts frequency from MHz to Hz.
uint64 ConvertMHzToHz(const uint64& val);

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_UTILS_H_
