// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#ifndef STRATUM_HAL_LIB_COMMON_CONSTANTS_H_
#define STRATUM_HAL_LIB_COMMON_CONSTANTS_H_

#include <stddef.h>

#include "stratum/glue/integral_types.h"

namespace stratum {
namespace hal {

// Invalid handle value for an event writer.
constexpr int kInvalidWriterId = -1;

// Special VRFs used by controller.
constexpr int kVrfDefault = 0;
constexpr int kVrfFallback = 0xffff;
constexpr int kVrfOverride = 0xfffe;
constexpr int kVrfMin = 0;
constexpr int kVrfMax = 0xffff;

// VLAN related constants.
constexpr size_t kVlanIdSize = 2;
constexpr size_t kVlanTagSize = 4;
constexpr uint16 kVlanIdMask = 0x0fff;
constexpr uint16 kDefaultVlan = 1;
constexpr uint16 kArpVlan = 4050;

// CPU port ID. This is a reserved port ID for CPU port (e.g. used as egress
// port by default for all the packets punted to CPU). This value cannot be
// used as ID for any other port.
// This value must match CPU_PORT in
// TODO(fix path to parser): p4/spec/parser.p4
// constexpr uint64 kCpuPortId = 0xFFFFFFFD;
constexpr uint64 kSdnUnspecifiedPortId = 0;
constexpr uint64 kSdnCpuPortId = 0xFFFFFFFD;
constexpr uint64 kCpuPortId = 0xFD;

// Constant broadcast MAC.
constexpr uint64 kBroadcastMac = 0xFFFFFFFFFFFFULL;

// The dst_mac_mask that matches all but multicast packets.
constexpr uint64 kNonMulticastDstMacMask = 0x010000000000ULL;

// Names of the packet in and packet out controller metadata preambles in
// P4Info.
constexpr char kIngressMetadataPreambleName[] = "packet_in";
constexpr char kEgressMetadataPreambleName[] = "packet_out";

// Ethertype (L3 protocol) values specified in IEEE 802.3.
// Source:
//   https://en.wikipedia.org/wiki/EtherType.
constexpr uint16 kEtherTypeIPv4 = 0x0800;
constexpr uint16 kEtherTypeIPv6 = 0x86dd;
constexpr uint16 kEtherTypeArp = 0x0806;

// L4 IP Protocol Values specified in RFC 5237 and RFC 7045.
// Source:
//   https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
constexpr uint8 kIpProtoIcmp = 1;
constexpr uint8 kIpProtoIPv6Icmp = 58;
constexpr uint8 kIpProtoTcp = 6;
constexpr uint8 kIpProtoUdp = 17;
constexpr uint8 kIpProtoGre = 47;

// Precision for converting floating point to ::gnmi::Decimal64
constexpr uint32 kDefaultPrecision = 2;

// Dummy MAC address used for unsupported DataRequests.
constexpr uint64 kDummyMacAddress = 0x112233445566ull;

}  // namespace hal
}  // namespace stratum

#endif  // STRATUM_HAL_LIB_COMMON_CONSTANTS_H_
