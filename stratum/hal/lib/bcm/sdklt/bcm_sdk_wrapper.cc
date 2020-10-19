// Copyright 2018 Google LLC
// Copyright 2018-present Open Networking Foundation
// Copyright 2019 Broadcom. All rights reserved. The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
// SPDX-License-Identifier: Apache-2.0

#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"  // NOLINT

#include <arpa/inet.h>
#include <byteswap.h>
#include <fcntl.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <sstream>  // IWYU pragma: keep
#include <string>
#include <thread>
#include <utility>

#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "gflags/gflags.h"
#include "stratum/glue/gtl/cleanup.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/net_util/ipaddress.h"
#include "stratum/glue/status/posix_error_space.h"
#include "stratum/glue/status/status_macros.h"
#include "stratum/hal/lib/bcm/constants.h"
#include "stratum/hal/lib/bcm/macros.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
#include "yaml-cpp/yaml.h"
// #include "util/endian/endian.h"

extern "C" {
// LT Operations
#include "bcmlt/bcmlt.h"
#include "bcmltd/chip/bcmltd_str.h"

// System configuration
#include "bcma/sys/bcma_sys_conf_sdk.h"

// BSL
#include "bcma/bsl/bcma_bslenable.h"
#include "bcma/bsl/bcma_bslcons.h"
#include "bcma/bsl/bcma_bslfile.h"
#include "bcma/bsl/bcma_bslsink.h"

// HA
#include "bcma/ha/bcma_ha.h"

// DRD
#include "bcmdrd_config.h"
#include "bcmdrd/bcmdrd_dev.h"

//MGMT
#include "bcmmgmt/bcmmgmt.h"

// BDE
#include "bcmlu/bcmlu_ngbde.h"

// Link Manager types
#include "bcmlm/bcmlm_types.h"

// Packet I/O
#include "bcmlu/bcmpkt/bcmlu_knet.h"
#include "bcmlu/bcmpkt/bcmlu_unet.h"
#include "bcmpkt/bcmpkt_dev.h"
#include "bcmpkt/bcmpkt_unet.h"
#include "bcmpkt/bcmpkt_knet.h"
#include "bcmpkt/bcmpkt_buf.h"
#include "bcmpkt/bcmpkt_txpmd.h"
#include "bcmpkt/bcmpkt_rxpmd.h"

#include <bcmdrd/bcmdrd_dev.h>
#include <bcmdrd/bcmdrd_feature.h>
#include <bcmbd/bcmbd.h>
#include <bcmlrd/bcmlrd_init.h>
#include <bcmmgmt/bcmmgmt.h>
#include <bcmmgmt/bcmmgmt_sysm_default.h>
#include <bcmlt/bcmlt.h>

#include <bcma/bsl/bcma_bslmgmt.h>
#include <bcma/bsl/bcma_bslcmd.h>
#include <bcma/bcmlt/bcma_bcmltcmd.h>
#include <bcma/bcmpc/bcma_bcmpccmd.h>
#include <bcma/bcmbd/bcma_bcmbdcmd_cmicd.h>
#include <bcma/bcmbd/bcma_bcmbdcmd_dev.h>
#include <bcma/bcmpkt/bcma_bcmpktcmd.h>
#include <bcma/cint/bcma_cint_cmd.h>
#include <bcma/ha/bcma_ha.h>
#include <bcma/sys/bcma_sys_conf_sdk.h>
#include <bcmlu/bcmlu_ngbde.h>
#include "bcma/sal/bcma_salcmd.h"

// CLI
#include <bcma/cli/bcma_cli_unit.h>
#include <bcma/cli/bcma_cli_bshell.h>

}

DEFINE_int64(linkscan_interval_in_usec, 200000, "Linkscan interval in usecs.");
DEFINE_int64(port_counters_interval_in_usec, 100 * 1000,
             "Port counter interval in usecs.");
DEFINE_int32(max_num_linkscan_writers, 10,
             "Max number of linkscan event Writers supported.");
DECLARE_string(bcm_sdk_checkpoint_dir);

// TODO: There are many CHECK_RETURN_IF_FALSE in this file which will
// need to be changed to return ERR_INTERNAL as opposed to ERR_INVALID_PARAM.

namespace stratum {
namespace hal {
namespace bcm {

constexpr absl::Duration BcmSdkWrapper::kWriteTimeout;
constexpr int BcmSdkWrapper::kUdfChunkSize;
// ACL stats-related constants
constexpr int BcmSdkWrapper::kColoredStatCount;
constexpr int BcmSdkWrapper::kUncoloredStatCount;
constexpr int BcmSdkWrapper::kMaxStatCount;
constexpr int BcmSdkWrapper::kTotalCounterIndex;
constexpr int BcmSdkWrapper::kRedCounterIndex;
constexpr int BcmSdkWrapper::kGreenCounterIndex;

// Software multicast structures
// TODO: synchronize access
static absl::flat_hash_map<uint8, std::vector<int>> multicast_group_id_to_replicas_ = {};
static absl::flat_hash_map<uint64, uint8> dst_mac_to_multicast_group_id = {};

// All the C style functions and vars used to work with BCM sdk need to be
// put into the following unnamed namespace.
namespace {

// System configuration structure
static bcma_sys_conf_t sys_conf, * isc;
static bool probed = false; // probed or created devices

// SDK callback to log to console a BSL message.
extern "C" int bsl_out_hook(bsl_meta_t* meta, const char* format,
                            va_list args) {
  BcmSdkWrapper* bcm_sdk_wrapper = BcmSdkWrapper::GetSingleton();
  if (!bcm_sdk_wrapper) {
    LOG(ERROR) << "BcmSdkWrapper singleton instance is not initialized.";
    return 0;
  }

  int rc = 0;
  if (pthread_self() == bcm_sdk_wrapper->GetDiagShellThreadId()) {
    if (meta->source != BSL_SRC_ECHO) {
      rc = vprintf(format, args);
    }
  } else {
    const char* file = (meta->file == nullptr ? "<unknown>" : meta->file);
    const int line = (meta->file == nullptr ? -1 : meta->line);
    const char* func = (meta->func == nullptr ? "<unknown>" : meta->func);
    char msg[1024];
    rc = vsnprintf(msg, sizeof(msg), format, args);
    // Log all the errors and warnings from SDK as LOG(ERROR). Additionally, all
    // the messages with meta->xtra == (BSL_LS_BCMAPI_CUSTOM | BSL_DEBUG) are
    // considered error.
    if (meta->severity <= BSL_WARN ||
        meta->xtra == (BSL_LS_APPL_SHELL | BSL_DEBUG)) {
      LOG(ERROR) << "BSL error (" << file << ":" << line << ":" << func
                 << "): " << msg;
    }
  }

  return rc;
}

// SDK callback to check if a debug message is to be logged.
extern "C" int bsl_check_hook(bsl_packed_meta_t meta_pack) {
  bsl_layer_t layer = static_cast<bsl_layer_t>(BSL_LAYER_GET(meta_pack));
  bsl_source_t source = static_cast<bsl_source_t>(BSL_SOURCE_GET(meta_pack));
  int severity = BSL_SEVERITY_GET(meta_pack);
  int x = bcma_bslenable_get(layer, source);
  int y = severity <= x || source == BSL_SRC_SHELL;
  return y;
}

// Callback for removing KNET intf.
extern "C" int knet_intf_remover(int unit, const bcmpkt_netif_t* netif,
                                 void* dummy) {
  return bcmpkt_netif_destroy(unit, netif->id);
}

// Callback for removing KNET filter.
extern "C" int knet_filter_remover(int unit, const bcmpkt_filter_t* filter,
                                   void* dummy) {
  return bcmpkt_filter_destroy(unit, filter->id);
}

// A callback function executed in BCM linkscan thread context.
extern "C" void sdk_linkscan_callback(bcmlt_table_notif_info_t* notify_info,
                                      void* user_data) {
  BcmSdkWrapper* bcm_sdk_wrapper = BcmSdkWrapper::GetSingleton();
  if (!bcm_sdk_wrapper) {
    LOG(ERROR) << "BcmSdkWrapper singleton instance is not initialized.";
    return;
  }

  bcmlt_entry_handle_t eh;
  int unit;
  PortState linkstatus;
  uint64_t port, link = 0;
  unit = notify_info->unit;
  eh = notify_info->entry_hdl;
  bcmlt_entry_field_get(eh, "PORT_ID", &port);
  bcmlt_entry_field_get(eh, "LINK_STATE", &link);
  LOG(INFO) << "Unit: " << unit << " Port: " << port << " Link: "
            << (link ? "UP" : "DOWN") << ".";
  linkstatus = link ? PORT_STATE_UP : PORT_STATE_DOWN;

  // Forward the event.
  bcm_sdk_wrapper->OnLinkscanEvent(unit, port, linkstatus);
}

::util::StatusOr<std::string> dump_rxpmd_header(int unit, int netif_id, bcmpkt_packet_t *packet) {
  auto pb = shr_pb_create();
  auto _ = gtl::MakeCleanup([pb]() { shr_pb_destroy(pb); });
  bcmdrd_dev_type_t dev_type;

  RETURN_IF_BCM_ERROR(bcmpkt_dev_type_get(unit, &dev_type));
  uint32* rxpmd = nullptr;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_get(packet, &rxpmd));
  if (shr_pb_printf(pb, "Rxpmd header:\n") == -1) {
    return ::util::Status(::util::error::Code::INTERNAL, "shr_pb_printf");
  }
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_dump(dev_type, rxpmd, BCMPKT_RXPMD_DUMP_F_NONE_ZERO, pb));
  if (shr_pb_printf(pb, "Reasons:\n") == -1) {
    return ::util::Status(::util::error::Code::INTERNAL, "shr_pb_printf");
  }
  RETURN_IF_BCM_ERROR(bcmpkt_rx_reason_dump(dev_type, rxpmd, pb));
  auto s = absl::StrCat("packet received for netif ", netif_id, ":\n",
      shr_pb_str(pb));
  return s;
}

int bcmpkt_data_dump(shr_pb_t *pb, const uint8_t *data, int size) {
  int idx;
  if (size > 256) {
      size = 256;
  }
  for (idx = 0; idx < size; idx++) {
      if ((idx & 0xf) == 0) {
          shr_pb_printf(pb, "%04x: ", idx);
      }
      if ((idx & 0xf) == 8) {
          shr_pb_printf(pb, "- ");
      }
      shr_pb_printf(pb, "%02x ", data[idx]);
      if ((idx & 0xf) == 0xf) {
          shr_pb_printf(pb, "\n");
      }
  }
  if ((idx & 0xf) != 0) {
      shr_pb_printf(pb, "\n");
  }
  return SHR_E_NONE;
}

std::string bcmpkt_data_buf_dump(const bcmpkt_data_buf_t *dbuf) {
  auto pb = shr_pb_create();
  auto _ = gtl::MakeCleanup([pb]() { shr_pb_destroy(pb); });
  shr_pb_printf(pb, "head - %p\n", dbuf->head);
  shr_pb_printf(pb, "data - %p\n", dbuf->data);
  shr_pb_printf(pb, "len - %" PRIu32 "\n", dbuf->len);
  shr_pb_printf(pb, "data_len - %" PRIu32 "\n", dbuf->data_len);
  shr_pb_printf(pb, "refcnt - %d\n", dbuf->ref_count);
  bcmpkt_data_dump(pb, dbuf->data, dbuf->data_len);
  return shr_pb_str(pb);
}

extern "C" int packet_receive_callback(int unit, int netif_id,
                                       bcmpkt_packet_t *packet,
                                       void *arg) {
  // TODO(BRCM): handle as per the need
  if (packet->type != BCMPKT_FWD_T_NORMAL) {
    return SHR_E_NONE;
  }

  auto rxpmd_header = dump_rxpmd_header(unit, netif_id, packet);
  if (rxpmd_header.ok()) {
    VLOG(1) << rxpmd_header.ValueOrDie();
  } else {
    LOG(ERROR) << rxpmd_header.status();
    return SHR_E_INTERNAL;
  }
  VLOG(1) << bcmpkt_data_buf_dump(packet->data_buf);

  return SHR_E_NONE;
}

int GetFieldMinMaxValue(int unit, const char* table, const char* field,
                        uint64_t* min, uint64_t* max) {
  uint32_t number_of_elements;
  bcmlt_field_def_t* buffer;
  bcmlt_field_def_t* field_def;
  uint32_t actual_number;
  int rv;
  uint32_t idx;
  bool found = false;

  rv = bcmlt_table_field_defs_get(unit, table, 0, NULL, &number_of_elements);
  if (rv != SHR_E_NONE) {
    return rv;
  }
  std::vector<bcmlt_field_def_t> buf(number_of_elements);
  rv = bcmlt_table_field_defs_get(unit, table, buf.size(), buf.data(),
                                  &actual_number);
  if ((rv != SHR_E_NONE) || (actual_number != buf.size())) {
    return SHR_E_INTERNAL;
  }
  for (auto const& field_def : buf) {
    if (field_def.symbol != true) {
      if ((::strcmp(field, field_def.name) == 0)) {
        *max = field_def.max;
        *min = field_def.min;
        found = true;
        break;
      }
    }
  }
  return (found ? SHR_E_NONE : SHR_E_NOT_FOUND);
}

// Converts MAC address as uint64 in host order to bcm_mac_t byte array. In
// this byte array the MSB is at the byte with the lowest index.
void Uint64ToBcmMac(uint64 mac, uint8 (* bcm_mac)[6]) {
  for (int i = 5; i >= 0; --i) {
    (*bcm_mac)[i] = mac & 0xff;
    mac >>= 8;
  }
}

// Prints a bcm_mac_t byte array, where MSB is at the byte with the lowest
// index.
std::string BcmMacToStr(const uint8 bcm_mac[6]) {
  std::stringstream buffer;
  std::string sep = "";
  for (int i = 0; i < 6; ++i) {
    buffer << sep << std::setfill('0') << std::setw(2) << std::hex
           << (bcm_mac[i] & 0xff);
    sep = ":";
  }

  return buffer.str();
}

// TODO(max): add contructors for sane default state, also convert "call sites"
struct l3_intf_t {
  int l3a_intf_id;     // Interface ID
  uint64 l3a_mac_addr; // MAC address
  int l3a_vid;         // VLAN ID
  int l3a_ttl;         // TTL threshold
  int l3a_mtu;         // MTU
};

struct l3_intf_object_t {
  int intf;            // interface
  uint64 mac_addr;     // mac
  int vlan;            // vlan
  int port;            // port
  int trunk;           // trunk
};

struct l3_route_t {
  bool l3a_flag;        // IPv4(false) or IPv6(true)
  int l3a_vrf;          // Virtual router instance
  int l3a_lookup_class; // Classification class ID
  int l3a_intf;         // L3 interface associated with route
  uint32 l3a_subnet;    // IP subnet address (IPv4)
  uint32 l3a_ip_mask;   // IP subnet mask (IPv4)
  std::string l3a_ip6_net;   // IP subnet address (IPv6)
  std::string l3a_ip6_mask;  // IP subnet mask (IPv6)
};

struct l3_host_t {
  bool l3a_flag;        // IPv4(false) or IPv6(true)
  int l3a_vrf;          // Virtual router instance
  int l3a_lookup_class; // Classification class ID
  int l3a_intf;         // L3 interface associated with route
  uint32 l3a_ip_addr;   // Destination host IP address (IPv4)
  std::string l3a_ip6_addr;  // Destination host IP address (IPv6)
};

// Pretty prints an L3 route.
std::string PrintL3Route(const l3_route_t& route) {
  std::stringstream buffer;
  if (route.l3a_flag) {
    buffer << "IPv6 LPM route (";
    buffer << "subnet: " << PackedStringToIPAddressOrDie(route.l3a_ip6_net) << ", ";
    buffer << "prefix: " << PackedStringToIPAddressOrDie(route.l3a_ip6_mask) << ", ";
  } else {
    buffer << "IPv4 LPM route (";
    buffer << "subnet: " << HostUInt32ToIPAddress(route.l3a_subnet) << ", ";
    buffer << "prefix: " << HostUInt32ToIPAddress(route.l3a_ip_mask) << ", ";
  }
  buffer << "vrf: " << route.l3a_vrf << ", ";
  buffer << "class_id: " << route.l3a_lookup_class << ", ";
  buffer << "egress_intf_id: " << route.l3a_intf << ")";

  return buffer.str();
}

// Pretty prints an L3 intf object.
std::string PrintL3RouterIntf(l3_intf_t intf) {
  uint8 mac[6];
  std::stringstream buffer;
  buffer << "(vlan: " << intf.l3a_vid << ", ";
  buffer << "ttl: " << intf.l3a_ttl << ", ";
  buffer << "mtu: " << intf.l3a_mtu << ", ";
  Uint64ToBcmMac(intf.l3a_mac_addr, &mac);
  buffer << "src_mac: " << BcmMacToStr(mac) << ", ";
  buffer << "router_intf_id: " << intf.l3a_intf_id << ")";
  return buffer.str();
}

// Pretty prints an L3 egress object.
std::string
PrintL3EgressIntf(l3_intf_object_t l3_intf_obj, int egress_intf_id) {
  uint8 mac[6];
  std::stringstream buffer;
  if (l3_intf_obj.trunk > 0) {
    buffer << "(trunk: " << l3_intf_obj.trunk << ", ";
  } else {
    buffer << "(port: " << l3_intf_obj.port << ", ";
  }
  buffer << "vlan: " << l3_intf_obj.vlan << ", ";
  buffer << "router_intf_id: " << l3_intf_obj.intf << ", ";
  Uint64ToBcmMac(l3_intf_obj.mac_addr, &mac);
  buffer << "dst_mac: " << BcmMacToStr(mac) << ", ";
  buffer << "egress_intf_id: " << egress_intf_id << ")";
  return buffer.str();
}

// Pretty prints an L3 host.
std::string PrintL3Host(const l3_host_t& host) {
  std::stringstream buffer;
  if (host.l3a_flag) {
    buffer << "IPv6 host route (";
    buffer << "subnet: " << PackedStringToIPAddressOrDie(host.l3a_ip6_addr) << ", ";
  } else {
    buffer << "IPv4 host route (";
    buffer << "subnet: " << HostUInt32ToIPAddress(host.l3a_ip_addr) << ", ";
  }
  buffer << "vrf: " << host.l3a_vrf << ", ";
  buffer << "class_id: " << host.l3a_lookup_class << ", ";
  buffer << "egress_intf_id: " << host.l3a_intf << ")";

  return buffer.str();
}

// RCPU header for KNET packets. This structure is private to this file, hence
// defined in private namespace.
struct VlanTag {
  uint16 vlan_id;
  uint16 type;
} __attribute__((__packed__));

struct RcpuData {
  uint16 rcpu_signature;
  uint8 rcpu_opcode;
  uint8 rcpu_flags;
  uint16 rcpu_transid;
  uint16 rcpu_payloadlen;
  uint16 rcpu_replen;
  uint8 rcpu_metalen;
  uint8 rcpu_queueid;
  uint8 reserved[2];
} __attribute__((__packed__));

struct RcpuHeader {
  struct ether_header ether_header;
  struct VlanTag vlan_tag;
  struct RcpuData rcpu_data;
} __attribute__((__packed__));

}  // namespace

namespace {
// Fetches a variable length field from a DCB header of an RX KNET packet. The
// field needs to be contained within a single 32-bit word (no crossing between
// words). The DCB header is composed of 32-bit words in network byte order, so
// byte swapping is done as needed.
//
// FieldType is the return type. This needs to be at least the number of bits
// between start_bit and end_bit (inclusive). 32 bits max.
//
// 'word' is the index of the word from the start of the DCB to examine. The
// first couple words are consumed by the KNET kernel module, so the dcb pointer
// doesn't point to word 0.
//
// 'start_bit' and 'end_bit' are the bits to extract from the word (inclusive).
// 31 is the most significant bit and 0 is the least significant bit. This is
// chosen to match the notation in the Broadcom chip register guides.
//
template <typename FieldType, int word, int start_bit, int end_bit>
FieldType GetDcbField(const void* dcb) {
  static_assert(word >= 2, "KNET cant access first 2 DCB words");
  static_assert(start_bit >= end_bit, "Must have start_bit >= end_bit");
  static_assert(start_bit >= 0 && start_bit < 32, "Invalid start bit");
  static_assert(end_bit >= 0 && end_bit < 32, "Invalid end bit");
  static_assert(start_bit - end_bit + 1 <= sizeof(FieldType) * 8,
                "Return type too small for the field");

  uint32 mask = ((static_cast<uint64>(1) << (start_bit + 1)) - 1) &
                ~((static_cast<uint64>(1) << end_bit) - 1);
  const uint32* data = reinterpret_cast<const uint32*>(dcb);
  return (ntohl(data[word - 2]) & mask) >> end_bit;
}

// The rxpmd header is definitely not in network byte order.
template <typename FieldType, int word, int start_bit, int end_bit>
FieldType GetRxpmdField(const void* rxpmd) {
  static_assert(word >= 2, "KNET cant access first 2 RXPMD words");
  static_assert(start_bit >= end_bit, "Must have start_bit >= end_bit");
  static_assert(start_bit >= 0 && start_bit < 32, "Invalid start bit");
  static_assert(end_bit >= 0 && end_bit < 32, "Invalid end bit");
  static_assert(start_bit - end_bit + 1 <= sizeof(FieldType) * 8,
                "Return type too small for the field");

  uint32 mask = ((static_cast<uint64>(1) << (start_bit + 1)) - 1) &
                ~((static_cast<uint64>(1) << end_bit) - 1);
  const uint32* data = reinterpret_cast<const uint32*>(rxpmd);
  return (data[word] & mask) >> end_bit;
}

// Sets a variable length field in a SOB Module Header (SOBMH) in an TX KNET
// packet. The field needs to be contained within a single 32-bit word (no
// crossing between words). The SOBMH is composed of 32-bit words in network
// byte order, so byte swapping is done as needed.
//
// 'word' is which word from the start of the module header needs to be
// manipulated. The value passed in here matches up with the Broadcom chip
// register guides.
//
// 'start_bit' and 'end_bit' are the bits to set in the word (inclusive). 31 is
// the most significant bit and 0 is the least significant bit. This is chosen
// to match the notation in the Broadcom chip register guides.
//
// If the value couldn't be set as requested, returns false. Currently the only
// way that can occur is if the requested value exceeds the width of the field.
//
template <int word, int start_bit, int end_bit>
bool SetSobField(void* sob, uint32 value) {
  static_assert(word >= 0, "Word index is negative");
  static_assert(word < 3, "SOBMH we use is 3 words long");
  static_assert(start_bit >= end_bit, "Must have start_bit >= end_bit");
  static_assert(start_bit >= 0 && start_bit < 32, "Invalid start bit");
  static_assert(end_bit >= 0 && end_bit < 32, "Invalid end bit");

  uint32 mask = ((static_cast<uint64>(1) << (start_bit + 1)) - 1) &
                ~((static_cast<uint64>(1) << end_bit) - 1);
  if ((value & (mask >> end_bit)) != value) {
    // The value didn't fit in the field
    return false;
  }
  uint32* data = reinterpret_cast<uint32*>(sob);
  data[word] = htonl((ntohl(data[word]) & ~mask) | ((value << end_bit) & mask));
  return true;
}

// Sets a portion of a variable length field in a SOB Module Header, for fields
// that are split into multiple pieces. This is similar to SetSobField except it
// also takes a value_start_bit and value_end_bit to select which bits are to be
// copied.
//
// Unlike SetSobField, it is not possible to detect values that exceed the total
// size of the field. That must be done separately with SobFieldSizeVerify(). In
// practice, that means this will always return true.
//
template <int word, int field_start_bit, int field_end_bit, int value_start_bit,
          int value_end_bit>
bool SetSobSplitField(void* sob, uint32 value) {
  static_assert(value_start_bit >= value_end_bit,
                "Must have value_start_bit >= value_end_bit");
  static_assert(value_start_bit >= 0 && value_start_bit < 32,
                "Invalid value start bit");
  static_assert(value_end_bit >= 0 && value_end_bit < 32,
                "Invalid value end bit");
  static_assert(
      value_start_bit - value_end_bit == field_start_bit - field_end_bit,
      "Size must match");

  uint32 value_mask = ((static_cast<uint64>(1) << (value_start_bit + 1)) - 1) &
                      ~((static_cast<uint64>(1) << value_end_bit) - 1);

  return SetSobField<word, field_start_bit, field_end_bit>(
      sob, (value & value_mask) >> value_end_bit);
}

// Verifies that 'value' fits in 'size' bits. This is primarily intended to be
// used in conjunction with SetSobSplitField to verify that values being set are
// not being silently truncated.
//
template <int size>
bool SobFieldSizeVerify(uint32 value) {
  // Note: allowing size == 32 is useless because value is uint32. If needed,
  // it should be trivial to add a template specialization for size == 32, that
  // just always returns true.
  static_assert(size > 0 && size < 32, "Invalid size");

  uint32 max_value = 1;
  max_value <<= size;
  max_value -= 1;
  return (value <= max_value);
}

template<typename TK, typename TV>
std::set<TV> extract_values(std::map<TK, TV> const& input_map) {
  std::set<TV> retval;
  for (auto const& element : input_map) {
    retval.insert(element.second);
  }
  return retval;
};

template<typename TK, typename TV>
std::set<TK> extract_keys(std::map<TK, TV> const& input_map) {
  std::set<TK> retval;
  for (auto const& element : input_map) {
    retval.insert(element.first);
  }
  return retval;
};

// Retrieves the key of a value in a container.
template<typename TS, typename TI, typename TV>
::util::StatusOr<bool> FindAndReturnEntry(TS search, TI index, TV value) {
  for (auto it = search->begin(); it != search->end(); ++it) {
    if (index == it->second) {
      *value = it->first;
      return true;
    }
  }
  return false;
}

// Returns a pointer to the const index associated with the given value.
// TODO(max): replace FindAndReturnEntry with this
template<typename Collection>
const typename Collection::key_type* FindIndexOrNull(
    Collection& collection, const typename Collection::mapped_type& value) {
  for (const auto& entry : collection) {
    if (entry.second == value) {
      return &entry.first;
    }
  }
  return nullptr;
}

// TODO(max): errmsg should not be an argument.
// TODO: Replace InUseMap with an array or vector, but not std::vector<bool>!
::util::StatusOr<int> GetFreeSlot(std::map<int, bool>* map,
                                  std::string ErrMsg) {
  for (auto& slot : *map) {
    if (!slot.second) {
      return slot.first;
    }
  }
  return MAKE_ERROR(ERR_INTERNAL) << ErrMsg;
}

void ConsumeSlot(std::map<int, bool>* map, int index) {
  auto& slot_in_use = gtl::FindOrDie(*map, index);
  CHECK(!slot_in_use);
  slot_in_use = true;
}

void ReleaseSlot(std::map<int, bool>* map, int index) {
  auto& slot_in_use = gtl::FindOrDie(*map, index);
  CHECK(slot_in_use);
  slot_in_use = false;
}

bool SlotExists(std::map<int, bool>* map, int index) {
  return gtl::ContainsKey(*map, index);
}

int bcmlt_custom_entry_commit(bcmlt_entry_handle_t entry_hdl,
                              bcmlt_opcode_t op,
                              bcmlt_priority_level_t prio) {
  int rv;
  bcmlt_entry_info_t entry_info;
  rv = bcmlt_entry_commit(entry_hdl, op, prio);
  if (rv != SHR_E_NONE) {
    return rv;
  }
  rv = bcmlt_entry_info_get(entry_hdl, &entry_info);
  if (rv != SHR_E_NONE) {
    return rv;
  }
  return entry_info.status;
}

::util::Status GetTableLimits(int unit, const char* table, int* min, int* max) {
  uint64_t table_max;
  uint64_t table_min;
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, TABLE_INFOs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_symbol_add(entry_hdl, TABLE_IDs, table));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_get(entry_hdl, INDEX_MAX_LIMITs, &table_max));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_get(entry_hdl, INDEX_MIN_LIMITs, &table_min));
  *max = static_cast<int>(table_max);
  *min = static_cast<int>(table_min);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

} // namespace

BcmSdkWrapper* BcmSdkWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BcmSdkWrapper::init_lock_(absl::kConstInit);

BcmSdkWrapper::BcmSdkWrapper(BcmDiagShell* bcm_diag_shell)
    : unit_to_chip_type_(), unit_to_soc_device_(), unit_to_logical_ports_(),
      unit_to_my_station_max_limit_(), unit_to_my_station_min_limit_(),
      unit_to_l3_intf_max_limit_(), unit_to_l3_intf_min_limit_(),
      l3_interface_ids_(), l3_egress_interface_ids_(),
      l3_ecmp_egress_interface_ids_(), unit_to_fp_groups_max_limit_(),
      ifp_group_ids_(), efp_group_ids_(), vfp_group_ids_(), fp_group_ids_(),
      unit_to_fp_rules_max_limit_(), ifp_rule_ids_(), efp_rule_ids_(),
      vfp_rule_ids_(), fp_rule_ids_(), unit_to_fp_policy_max_limit_(),
      ifp_policy_ids_(), efp_policy_ids_(), vfp_policy_ids_(),
      fp_policy_ids_(), unit_to_fp_meter_max_limit_(), ifp_meter_ids_(),
      efp_meter_ids_(), fp_meter_ids_(), unit_to_fp_max_limit_(),
      ifp_acl_ids_(), efp_acl_ids_(), vfp_acl_ids_(), fp_acl_ids_(),
      unit_to_udf_chunk_ids_(), unit_to_chunk_ids_(),
      bcm_diag_shell_(bcm_diag_shell),
      linkscan_event_writers_() {
  // TODO(BRCM): check if any initialization is needed.
  // for now this is good
}

BcmSdkWrapper::~BcmSdkWrapper() { ShutdownAllUnits().IgnoreError(); }

::util::StatusOr<std::string> BcmSdkWrapper::GenerateBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map, OperationMode mode) {
  std::stringstream buffer;
  const size_t max_num_units = base_bcm_chassis_map.bcm_chips_size();

  // PC_PM Table
  YAML::Emitter pc_pm;
  pc_pm << YAML::BeginDoc;
  pc_pm << YAML::BeginMap;
  pc_pm << YAML::Key << "device";
  pc_pm << YAML::Value << YAML::BeginMap;
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_pm << YAML::Key << unit;
    pc_pm << YAML::Value << YAML::BeginMap;
    pc_pm << YAML::Key << "PC_PM";
    pc_pm << YAML::Value << YAML::BeginMap;
    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      // Key is a map (PC_PM_ID: serdes_core)
      pc_pm << YAML::Key << YAML::BeginMap << YAML::Key << "PC_PM_ID"
            << YAML::Value << bcm_port.serdes_core() << YAML::EndMap;

      pc_pm << YAML::Value << YAML::BeginMap;

      pc_pm << YAML::Key << "PM_OPMODE" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << "PC_PM_OPMODE_DEFAULT" << YAML::EndSeq;

      // TODO(max): SPEED_MAX has to be set to the highest supported value, else
      // speed changes are not possible at runtime. We set it to 100G for now.
      pc_pm << YAML::Key << "SPEED_MAX" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << 100000 << 0 << 0 << 0 << YAML::EndSeq;

      pc_pm << YAML::Key << "LANE_MAP" << YAML::Value << YAML::Flow
            << YAML::BeginSeq << YAML::Hex << 0xf << 0 << 0 << 0 << YAML::Dec
            << YAML::EndSeq;

      pc_pm << YAML::EndMap;  // PC_PM_ID
    }
    pc_pm << YAML::EndMap;  // PC_PM
    pc_pm << YAML::EndMap;  // <unit>
  }
  pc_pm << YAML::EndMap;  // device
  pc_pm << YAML::EndDoc;
  buffer << pc_pm.c_str() << "\n";

  // PC_PM_CORE
  YAML::Emitter pc_pm_core;
  pc_pm_core << YAML::BeginDoc;
  pc_pm_core << YAML::BeginMap;
  pc_pm_core << YAML::Key << "device";
  pc_pm_core << YAML::Value << YAML::BeginMap;
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_pm_core << YAML::Key << unit;
    pc_pm_core << YAML::Value << YAML::BeginMap;
    pc_pm_core << YAML::Key << "PC_PM_CORE";
    pc_pm_core << YAML::Value << YAML::BeginMap;
    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      if (bcm_port.tx_lane_map() || bcm_port.rx_lane_map() ||
          bcm_port.tx_polarity_flip() || bcm_port.rx_polarity_flip()) {
        // Key is a map (PC_PM_ID: serdes_core, CORE_INDEX: unit)
        pc_pm_core << YAML::Key << YAML::BeginMap << YAML::Key << "PC_PM_ID"
                   << YAML::Value << bcm_port.serdes_core() << YAML::Key
                   << "CORE_INDEX" << YAML::Value << bcm_port.unit()
                   << YAML::EndMap;

        pc_pm_core << YAML::Value << YAML::BeginMap;

        if (bcm_port.tx_lane_map()) {
          pc_pm_core << YAML::Key << "TX_LANE_MAP" << YAML::Value
                     << bcm_port.tx_lane_map();
        }

        if (bcm_port.rx_lane_map()) {
          pc_pm_core << YAML::Key << "RX_LANE_MAP" << YAML::Value
                     << bcm_port.rx_lane_map();
        }

        if (bcm_port.tx_polarity_flip()) {
          pc_pm_core << YAML::Key << "TX_POLARITY_FLIP" << YAML::Value
                     << bcm_port.tx_polarity_flip();
        }

        if (bcm_port.tx_polarity_flip()) {
          pc_pm_core << YAML::Key << "RX_POLARITY_FLIP" << YAML::Value
                     << bcm_port.rx_polarity_flip();
        }

        pc_pm_core << YAML::EndMap;
      }
    }
    pc_pm_core << YAML::EndMap;  // PC_PM_CORE
    pc_pm_core << YAML::EndMap;  // <unit>
  }
  pc_pm_core << YAML::EndMap;  // device
  pc_pm_core << YAML::EndDoc;
  buffer << pc_pm_core.c_str() << "\n";

  // TODO(Yi): PC_PM_TX_LANE_PROFILE from serdes db.
  //  Note: PC_PM_LANE depends on PC_PM_TX_LANE_PROFILE
  //  YAML::Emitter pc_pm_lane;
  //  pc_pm_lane << YAML::BeginDoc;
  //  pc_pm_lane << YAML::BeginMap;
  //  pc_pm_lane << YAML::Key << "device";
  //  pc_pm_lane << YAML::Value << YAML::BeginMap;
  //  pc_pm_lane << YAML::Key << "0";
  //  pc_pm_lane << YAML::Value << YAML::BeginMap;
  //  pc_pm_lane << YAML::Key << "PC_PM_LANE";
  //  pc_pm_lane << YAML::Value << YAML::BeginMap;
  //
  //  for (auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
  //    int pc_pm_id = bcm_port.serdes_core();
  //
  //    for (int lane_id = 0; lane_id < bcm_port.num_serdes_lanes(); lane_id++)
  //    {
  //
  //      // Key is a map (PC_PM_ID: xx, CORE_INDEX: unit, CORE_LANE: xx)
  //      pc_pm_lane << YAML::Key << YAML::BeginMap
  //                 << YAML::Key << "PC_PM_ID" << YAML::Value << pc_pm_id
  //                 << YAML::Key << "CORE_INDEX"
  //                 << YAML::Value << bcm_port.unit()
  //                 << YAML::Key << "CORE_LANE" << YAML::Value << lane_id
  //                 << YAML::EndMap;
  //
  //      // TODO(Yi): Support multiple op mode and profile
  //      pc_pm_lane << YAML::Value << YAML::BeginMap;
  //      pc_pm_lane << YAML::Key << "PORT_OPMODE";
  //      pc_pm_lane << YAML::Value << YAML::Flow
  //                 << YAML::BeginSeq << "PC_PORT_OPMODE_ANY" << YAML::EndSeq;
  //      pc_pm_lane << YAML::Key << "PC_PM_TX_LANE_PROFILE_ID";
  //      pc_pm_lane << YAML::Value << YAML::Flow
  //                 << YAML::BeginSeq << tx_lane_profile_id << YAML::EndSeq;
  //      pc_pm_lane << YAML::EndMap;  // PORT_OPMODE
  //    }
  //  }
  //  pc_pm_lane << YAML::EndMap;  // PC_PM_LANE
  //  pc_pm_lane << YAML::EndMap;  // 0
  //  pc_pm_lane << YAML::EndMap;  // device
  //  pc_pm_lane << YAML::EndDoc;
  //  buffer << pc_pm_lane.c_str() << "\n";

  // PC_PORT
  YAML::Emitter pc_port;
  pc_port << YAML::BeginDoc;
  pc_port << YAML::BeginMap;
  pc_port << YAML::Key << "device";
  pc_port << YAML::Value << YAML::BeginMap;
  for (size_t unit = 0; unit < max_num_units; ++unit) {
    pc_port << YAML::Key << unit;
    pc_port << YAML::Value << YAML::BeginMap;
    pc_port << YAML::Key << "PC_PORT";
    pc_port << YAML::Value << YAML::BeginMap;

    for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
      if (bcm_port.unit() != unit) {
        continue;
      }
      // Key is a map (PORT_ID: logical_port)
      pc_port << YAML::Key << YAML::BeginMap << YAML::Key << "PORT_ID"
              << YAML::Value << bcm_port.logical_port() << YAML::EndMap;
      pc_port << YAML::Value << YAML::BeginMap << YAML::Key << "PC_PHYS_PORT_ID"
              << YAML::Value << bcm_port.physical_port() << YAML::Key
              << "ENABLE" << YAML::Value << 1 << YAML::Key << "OPMODE"
              << YAML::Value
              << absl::StrCat("PC_PORT_OPMODE_",
                              bcm_port.speed_bps() / kBitsPerGigabit, "G")
              << YAML::EndMap;  // PORT_ID
    }
    pc_port << YAML::EndMap;  // PC_PORT
    pc_port << YAML::EndMap;  // <unit>
  }
  pc_port << YAML::EndMap;  // device
  pc_port << YAML::EndDoc;
  buffer << pc_port.c_str() << "\n";

  return buffer.str();
}

::util::Status BcmSdkWrapper::InitializeSdk(
    const std::string& config_file_path,
    const std::string& config_flush_file_path,
    const std::string& bcm_shell_log_file_path) {

  int rv;
  int ndev;
  int unit;

  // Initialize system configuration structure
  if (!isc) {
    isc = &sys_conf;
    bcma_sys_conf_init(isc);
  }

  // Initialize system log output
  bsl_config_t bsl_config;
  bcma_bslenable_init();
  bsl_config_t_init(&bsl_config);
  bsl_config.out_hook = bsl_out_hook;
  bsl_config.check_hook = bsl_check_hook;
  bsl_init(&bsl_config);

  // TODO(BRCM): enable log messages as necessary
  // Initialize output hook
  // bcma_bslenable_set(BSL_LAY_APPL, BSL_SRC_ECHO, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_BCMBD, BSL_SRC_INIT, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_BCMCFG, BSL_SRC_INIT, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_BCMDRD, BSL_SRC_DEV, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_BCMFP, BSL_SRC_COMMON, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_SYS, BSL_SRC_PCI, BSL_SEV_DEBUG);
  // bcma_bslenable_set(BSL_LAY_BCMPKT, BSL_SRC_PACKET, BSL_SEV_DEBUG);

  // Create console sink l_config
  bcma_bslcons_init();

  // Create file sink
  bcma_bslfile_init();

  RETURN_IF_ERROR(InitCLI());

  // Probe for supported devices and initialize DRD
  if ((ndev = bcma_sys_conf_drd_init(isc)) < 0) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Could not find any supported device.";
  }

  probed = true;
  LOG(INFO) << "Found " << ndev << " device" << ((ndev == 1) ? "" : "s") << ".";

  // Initialize HA
  bcma_ha_init(true, false);
  for (unit = 0; unit < BCMDRD_CONFIG_MAX_UNITS; unit++) {
    if (!bcmdrd_dev_exists(unit)) {
      continue;
    }
    rv = bcma_ha_unit_open(unit, DEFAULT_HA_FILE_SIZE, true, false);
    if (rv < 0) {
      LOG(INFO) << "Failed to create HA memory for unit " << unit << "(" << rv
                << ").";
    }
  }
  // Start all SDK components and attach all devices
  RETURN_IF_BCM_ERROR(bcmmgmt_init(false, config_file_path.c_str()));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::FindUnit(int unit, int pci_bus, int pci_slot,
                                       BcmChip::BcmChipType chip_type) {

  int num_devices = 0;
  int u = 0;
  bcmlu_dev_info_t dev_info, * di = &dev_info;
  bcmdrd_dev_t* dev;

  if (!probed) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "BDE not initialized yet. Call InitializeSdk() first.";
  }

  // Get number of probed devices
  RETURN_IF_BCM_ERROR(bcmlu_ngbde_num_dev_get(&num_devices));

  if (num_devices <= 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "No devices found.";
  }

  for (int dev_num = 0; dev_num < num_devices; ++dev_num) {
    if (bcmlu_ngbde_dev_info_get(dev_num, di) < 0) {
      continue;
    }
    for (u = 0; u < BCMDRD_CONFIG_MAX_UNITS; u++) {
      if (bcmdrd_dev_exists(u)) {
        dev = bcmdrd_dev_get(u);
        if (dev != NULL) {
          /* TODO(BRCM): read pci_bus, pci_slot from linux */
          /* and compare with identified device */
          if ((di->device_id == dev->id.device_id) &&
              (di->vendor_id == dev->id.vendor_id)) {
            absl::WriterMutexLock l(&data_lock_);
            /* TODO(BRCM): Add validation to make sure chip_type matches the */
            /* device we found here. */
            unit_to_chip_type_[unit] = chip_type;
            unit_to_soc_device_[unit] = new BcmSocDevice();
            unit_to_soc_device_[unit]->dev_num = dev_num;

            if (u != unit) {
              return MAKE_ERROR(ERR_INTERNAL)
                     << "Unit " << unit << " was not assigned to SOC device "
                     << dev->name
                     << " found on PCI bus " << pci_bus << ", PCI slot "
                     << pci_slot
                     << ". The device handle for this SOC device (" << u
                     << ") does not match the unit number.";
            }
            LOG(INFO) << "Unit " << unit << " is assigned to SOC device "
                      << dev->name
                      << " found on PCI bus " << pci_bus << ", PCI slot "
                      << pci_slot
                      << ".";
            return ::util::OkStatus();
          }
        }
      }
    }
  }
  return MAKE_ERROR(ERR_INTERNAL)
         << "Could not find any SOC device on PCI bus " << pci_bus
         << ", PCI slot " << pci_slot << ".";
}

::util::Status BcmSdkWrapper::InitializeUnit(int unit, bool warm_boot) {

  uint64_t all_ports_no_cpu_bitmap[3] = {
    0xFFFFFFFFFFFFFFFeULL,
    kuint64max,
    0
  };
  int rv;
  uint64_t physical_device_port;
  uint64_t port_macro_id;
  int table_max;
  int table_min;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  std::vector<std::pair<int, int>> configured_ports;
  std::map<int, std::pair<int, int>> tmp_map;

  if (!probed) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "BDE not initialized yet. Call InitializeSdk() first.";
  }

  // SOC device init.
  {
    absl::WriterMutexLock l(&data_lock_);
    if (!unit_to_soc_device_.count(unit)) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Unit " << unit << " has not been assigned to any SOC device.";
    }
    // Set MTU for all the L3 intf of this unit to the default value.
    unit_to_mtu_[unit] = kDefaultMtu;

    // Populate logical ports, corresponding physical id and port macro id
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
    while ((rv = bcmlt_entry_commit(entry_hdl,
                                    BCMLT_OPCODE_TRAVERSE,
                                    BCMLT_PRIORITY_NORMAL)) == SHR_E_NONE) {
      if (bcmlt_entry_info_get(entry_hdl, &entry_info) != SHR_E_NONE ||
          entry_info.status != SHR_E_NONE) {
        break;
      }
      uint64_t l_port;
      if (bcmlt_entry_field_get(entry_hdl, PORT_IDs, &l_port) != SHR_E_NONE) {
        break;
      }
      if (bcmlt_entry_field_get(entry_hdl, PC_PHYS_PORT_IDs,
                                &physical_device_port) != SHR_E_NONE) {
        break;
      }
      configured_ports.push_back(std::make_pair(l_port, physical_device_port));
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PHYS_PORTs, &entry_hdl));
    for (const auto &p : configured_ports) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, PC_PHYS_PORT_IDs, p.second));
      RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                             BCMLT_PRIORITY_NORMAL));
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_get(entry_hdl, PC_PM_IDs, &port_macro_id));
      tmp_map[p.first] = std::make_pair(p.second, port_macro_id);
    }
    unit_to_logical_ports_[unit] = tmp_map;
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }

  RETURN_IF_ERROR(GetTableLimits(unit, L2_MY_STATIONs, &table_min, &table_max));
  unit_to_my_station_min_limit_[unit] = table_min;
  unit_to_my_station_max_limit_[unit] = table_max;
  my_station_ids_[unit] = {};

  RETURN_IF_ERROR(GetTableLimits(unit, L3_EIFs, &table_min, &table_max));
  // TODO(BRCM): fixup to avoid interface with,
  // is this really needed, verify
  unit_to_l3_intf_min_limit_[unit] = table_min + 1;
  unit_to_l3_intf_max_limit_[unit] = table_max;
  l3_interface_ids_[unit] = {};

  InUseMap l3_egress_intf;
  RETURN_IF_ERROR(GetTableLimits(unit, L3_UC_NHOPs, &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    l3_egress_intf.emplace(i, false);
  }
  l3_egress_interface_ids_[unit] = l3_egress_intf;

  InUseMap l3_ecmp_egress_intf;
  RETURN_IF_ERROR(GetTableLimits(unit, ECMPs, &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    l3_ecmp_egress_intf.emplace(i + 1, false);
  }
  l3_ecmp_egress_interface_ids_[unit] = l3_ecmp_egress_intf;

  fp_group_ids_[unit] = new AclGroupIds();
  int max_fp_groups = 0;

  // IFP - group
  RETURN_IF_ERROR(GetTableLimits(unit, FP_ING_GRP_TEMPLATEs,
                                 &table_min, &table_max));
  InUseMap ifp_groups;
  for (auto i = table_min; i <= table_max; i++) {
    ifp_groups.emplace(i, false);
  }
  ifp_group_ids_[unit] = ifp_groups;
  max_fp_groups += table_max;

  // VFP - group
  InUseMap vfp_groups;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_VLAN_GRP_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    vfp_groups.emplace(i, false);
  }
  vfp_group_ids_[unit] = vfp_groups;
  max_fp_groups += table_max;

  // EFP - group
  InUseMap efp_groups;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_EGR_GRP_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    efp_groups.emplace(i, false);
  }
  efp_group_ids_[unit] = efp_groups;
  max_fp_groups += table_max;

  unit_to_fp_groups_max_limit_[unit] = max_fp_groups;

  fp_rule_ids_[unit] = new AclRuleIds();
  int max_fp_rules = 0;
  // IFP - rules
  InUseMap ifp_rules;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_ING_RULE_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    ifp_rules.emplace(i, false);
  }
  ifp_rule_ids_[unit] = ifp_rules;
  max_fp_rules += table_max;

  // VFP - rules
  InUseMap vfp_rules;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_VLAN_RULE_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    vfp_rules.emplace(i, false);
  }
  vfp_rule_ids_[unit] = vfp_rules;
  max_fp_rules += table_max;

  // EFP - rules
  InUseMap efp_rules;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_EGR_RULE_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    efp_rules.emplace(i, false);
  }
  efp_rule_ids_[unit] = efp_rules;
  max_fp_rules += table_max;

  unit_to_fp_rules_max_limit_[unit] = max_fp_rules;

  fp_policy_ids_[unit] = new AclPolicyIds();
  int max_fp_policies = 0;
  // IFP - policies
  InUseMap ifp_policies;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_ING_POLICY_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    ifp_policies.emplace(i, false);
  }
  ifp_policy_ids_[unit] = ifp_policies;
  max_fp_policies += table_max;

  // VFP - policies
  InUseMap vfp_policies;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_VLAN_POLICY_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    vfp_policies.emplace(i, false);
  }
  vfp_policy_ids_[unit] = vfp_policies;
  max_fp_policies += table_max;

  // EFP - policies
  InUseMap efp_policies;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_EGR_POLICY_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    efp_policies.emplace(i, false);
  }
  efp_policy_ids_[unit] = efp_policies;
  max_fp_policies += table_max;

  unit_to_fp_policy_max_limit_[unit] = max_fp_policies;

  fp_meter_ids_[unit] = new AclMeterIds();
  int max_fp_meters = 0;
  // IFP - Meters
  InUseMap ifp_meters;
  RETURN_IF_ERROR(GetTableLimits(unit, METER_FP_ING_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    ifp_meters.emplace(i, false);
  }
  ifp_meter_ids_[unit] = ifp_meters;
  max_fp_meters += table_max;

  // EFP - Meters
  InUseMap efp_meters;
  RETURN_IF_ERROR(GetTableLimits(unit, METER_FP_EGR_TEMPLATEs,
                                 &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    efp_meters.emplace(i, false);
  }
  efp_meter_ids_[unit] = efp_meters;
  max_fp_meters += table_max;

  unit_to_fp_meter_max_limit_[unit] = max_fp_meters;

  // FP ACLs
  fp_acl_ids_[unit] = new AclIds();
  int max_fp_acls = 0;
  // IFP Acls
  InUseMap ifp_acls;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_ING_ENTRYs, &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    ifp_acls.emplace(i, false);
  }
  ifp_acl_ids_[unit] = ifp_acls;
  max_fp_acls += table_max;

  // VFP Acls
  InUseMap vfp_acls;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_VLAN_ENTRYs, &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    vfp_acls.emplace(i, false);
  }
  vfp_acl_ids_[unit] = vfp_acls;
  max_fp_acls += table_max;

  // EFP Acls
  InUseMap efp_acls;
  RETURN_IF_ERROR(GetTableLimits(unit, FP_EGR_ENTRYs, &table_min, &table_max));
  for (auto i = table_min; i <= table_max; i++) {
    efp_acls.emplace(i, false);
  }
  efp_acl_ids_[unit] = efp_acls;
  max_fp_acls += table_max;

  unit_to_fp_max_limit_[unit] = max_fp_acls;

  // UDF Chunks
  InUseMap udf_chunks;
  for (auto i = 0; i <= kUdfMaxChunks; i++) {
    udf_chunks.emplace(i, false);
  }
  unit_to_udf_chunk_ids_[unit] = udf_chunks;
  unit_to_chunk_ids_[unit] = new ChunkIds();

  // Disable port level MAC address learning
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORT_LEARNs, &entry_hdl));
  for (const auto &p : configured_ports) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_LEARNs, 0));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Enable IFP, EFP and VFP on all ports
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORT_FPs, &entry_hdl));
  for (const auto &p : configured_ports) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLANs, 1));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_INGs, 1));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGRs, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Set default VLAN STG
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_STGs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Configure ports in forwarding state
  const char* vlan_stg_str[1] = {"FORWARD"};
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_STGs, &entry_hdl));
  for (const auto &p : configured_ports) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_array_symbol_add(entry_hdl, STATEs, p.first,
                                           vlan_stg_str, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Create default VLAN ingress action profile
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, VLAN_ING_TAG_ACTION_PROFILEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_ING_TAG_ACTION_PROFILE_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, UT_OTAGs, ADDs));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Create default VLAN (1) and add all ports as members
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, kDefaultVlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, EGR_MEMBER_PORTSs, 0, all_ports_no_cpu_bitmap,
                                  3));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, ING_MEMBER_PORTSs, 0, all_ports_no_cpu_bitmap,
                                  3));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, UNTAGGED_MEMBER_PORTSs, 0,
                                  all_ports_no_cpu_bitmap, 3));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_IIF_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Configure default port VLAN ID of 1 for all ports.
  // Enable IPv4 and IPv6 routing on port
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORTs, &entry_hdl));
  for (const auto &p : configured_ports) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MY_MODIDs, 0));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, VLAN_ING_TAG_ACTION_PROFILE_IDs, 1));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ING_OVIDs, kDefaultVlan));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, PORT_TYPEs, ETHERNETs));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, V4L3s, 1));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, V6L3s, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, PORT_PKT_CONTROL_IDs, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Configure profile to classify 0x8100 at bytes 12,13 of the packet to be
  // outer TPID and add 0x8100 as outgoing packets outer TPID.
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_OUTER_TPIDs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_OUTER_TPID_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ING_TPIDs, 0x8100));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, EGR_TPIDs, 0x8100));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Configure PORT_POLICY to classify packets with value 0x8100 at bytes 12,13
  // as outer VLAN tagged packet.
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORT_POLICYs, &entry_hdl));
  for (const auto &p : configured_ports) {
    uint64_t pass_on_outer_tpid_match_map[1] = {1};
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_array_add(entry_hdl, PASS_ON_OUTER_TPID_MATCHs, 0,
                                    pass_on_outer_tpid_match_map, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Create L3_IIF_PROFILE 1 and enable IPv4 and IPv6 routing.
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IIF_PROFILEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_IIF_PROFILE_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_UCs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UCs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Create L3_IIF index 1 and set VRF_ID=0.
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IIFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_IIF_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_IIF_PROFILE_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Enable packet counters on all ports
  // TODO(max): only add configured ports to bitmap, reduces polling CPU load
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_CONTROLs, &entry_hdl));
  auto _ = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_array_add(entry_hdl, PORTSs, 0,
                                                  all_ports_no_cpu_bitmap, 3));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, INTERVALs,
                      FLAGS_port_counters_interval_in_usec));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MULTIPLIER_PORTs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MULTIPLIER_EPIPEs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MULTIPLIER_IPIPEs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MULTIPLIER_TMs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MULTIPLIER_EVICTs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  for (auto const& p : configured_ports) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_MACs, &entry_hdl));
    auto cl1 = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                  BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_MAC_ERRs, &entry_hdl));
    auto cl2 = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                  BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_L3s, &entry_hdl));
    auto cl3 = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, p.first));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                  BCMLT_PRIORITY_NORMAL));
  }

  // Initialize packet device
  // Code from SDKLT wiki
  bcmpkt_dev_init_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.cgrp_size = 4;
  cfg.cgrp_bmp = 0x7;
  RETURN_IF_BCM_ERROR(bcmpkt_dev_init(unit, &cfg));
  RETURN_IF_ERROR(CleanupKnet(unit));

  LOG(INFO) << "Unit " << unit << " initialized successfully (warm_boot: "
            << (warm_boot ? "YES" : "NO") << ").";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ShutdownUnit(int unit) {
  int rv;
  if (!unit_to_soc_device_.count(unit)) {
    return ::util::OkStatus();
  }

  // Check for valid sys_conf structure
  if (isc == NULL) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "System configuration structure is not initialized.";
  }

  // Shut down SDK (detach all the running devices and
  // stop all the registered SDK components
  rv = bcmmgmt_shutdown(true);
  if (SHR_FAILURE(rv)) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to shutdown the SDK System Manager.";
  }

  // Clean up HA file for the unit
  if (!bcmdrd_dev_exists(unit)) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unit " << unit << "  is not found.";
  } else {
    bcma_ha_unit_close(unit, false);
  }

  // Remove devices from DRD
  bcma_sys_conf_drd_cleanup(isc);

  // TODO(BRCM): Clean up CLI ?

  // Clean up system log
  rv = bcma_bslmgmt_cleanup();
  if (SHR_FAILURE(rv)) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to cleanup system log.";
  }

  // Release system configuration structure
  bcma_sys_conf_cleanup(isc);

  // Remove the unit from unit_to_soc_device_ map.
  delete unit_to_soc_device_[unit];
  unit_to_soc_device_.erase(unit);

  // Remove the unit from unit_to_mtu_ map.
  unit_to_mtu_.erase(unit);  // NOOP if unit is not present for some reason.

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ShutdownAllUnits() {
  ::util::Status status = ::util::OkStatus();
  absl::WriterMutexLock l(&data_lock_);
  // Detach all the units. Continue even if there is an error, but save the
  // error to return at the end. If the unit has not been correctly initialized
  // or not initialized at all, ShutdownUnit() will do the clean or will be a
  // NOOP.
  std::vector<int> units;
  std::transform(unit_to_soc_device_.begin(), unit_to_soc_device_.end(),
                 std::back_inserter(units),
                 [](decltype(unit_to_soc_device_)::value_type const& p) {
                   return p.first;
                 });
  for (int unit : units) {
    APPEND_STATUS_IF_ERROR(status, ShutdownUnit(unit));
  }
  gtl::STLDeleteValues(
      &unit_to_soc_device_);  // if some entries were not deleted

  return status;
}

::util::Status BcmSdkWrapper::SetModuleId(int unit, int module) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitializePort(int unit, int port) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));
  // Port Disable and Set max frame
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  // may be Bug: crashing when linkscan is enabled
  // RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ENABLEs, 0));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAX_FRAME_SIZEs, kDefaultMaxFrameSize));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Linkscan mode
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, SOFTWAREs));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Port Block
  const char* block = "BLOCK";
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_STGs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_symbol_add(entry_hdl, STATEs, port, &block, 1));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Port counters
  // TODO(max): add port to port CTR_CONTROL PORT field.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetPortOptions(int unit, int port,
                                             const BcmPortOptions& options) {
  const char* block = "BLOCK";
  const char* forward = "FORWARD";
  uint64_t max;
  uint64_t min;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));
  // Enable
  if (options.enabled()) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ENABLEs,
                                              (options.enabled() ==
                                               TRI_STATE_TRUE ? 1 : 0)));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  // STP State
  if (options.blocked()) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_STGs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_array_symbol_add(entry_hdl, STATEs, port,
                                           (options.blocked() == TRI_STATE_TRUE
                                            ? &block : &forward),
                                           1));
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                    BCMLT_PRIORITY_NORMAL));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                    BCMLT_PRIORITY_NORMAL));
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  // Speed
  if (options.speed_bps() > 0) {
    LOG(WARNING) << "Changining Speed is not supported.";
  }
  // Serdes lanes
  if (options.num_serdes_lanes() > 0) {
    LOG(WARNING) << "Changining serdes lanes is not supported.";
  }
  // MTU
  if (options.max_frame_size() > 0) {
    CHECK_RETURN_IF_FALSE(options.max_frame_size() > 0);
    RETURN_IF_BCM_ERROR(
        GetFieldMinMaxValue(unit, PC_PORTs, MAX_FRAME_SIZEs, &min, &max));
    if ((options.max_frame_size() > static_cast<int>(max)) ||
        (options.max_frame_size() < static_cast<int>(min))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid mtu (" << options.max_frame_size()
             << "), valid mtu range is "
             << static_cast<int>(min) << " - "
             << static_cast<int>(max) << ".";
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAX_FRAME_SIZEs,
                                              options.max_frame_size()));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  // Linkscan
  if (options.linkscan_mode()) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
    switch (options.linkscan_mode()) {
      case BcmPortOptions::LINKSCAN_MODE_SW:
        RETURN_IF_BCM_ERROR(
            bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, SOFTWAREs));
        break;
      case BcmPortOptions::LINKSCAN_MODE_HW:
        RETURN_IF_BCM_ERROR(
            bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, HARDWAREs));
        break;
      default:
        RETURN_IF_BCM_ERROR(
            bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, NO_SCANs));
        break;
    }
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                    BCMLT_PRIORITY_NORMAL));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                    BCMLT_PRIORITY_NORMAL));
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  // Loopback
  if (options.loopback_mode()) {
    // SDKLT only supports MAC loopback mode.
    const char* loopback;
    if (options.loopback_mode() == LOOPBACK_STATE_NONE) {
      loopback = PC_LPBK_NONEs;
    } else if (options.loopback_mode() == LOOPBACK_STATE_MAC) {
      loopback = PC_LPBK_MACs;
    } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Unsupported loopback mode: "
             << LoopbackState_Name(options.loopback_mode());
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, LOOPBACK_MODEs, loopback));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(
        entry_hdl, BCMLT_OPCODE_UPDATE, BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetPortOptions(int unit, int port,
                                             BcmPortOptions* options) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t port_macro_id;
  const char* linkscan_mode;
  uint64_t enabled;
  uint64_t max_frame_size;
  uint64_t physical_device_port;
  const char* op_mode;
  uint64_t lanemap_array;
  uint32_t actual_count;
  uint64_t speed_array;
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));
  // Linkscan
  options->set_linkscan_mode(BcmPortOptions::LINKSCAN_MODE_UNKNOWN);
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_get(entry_hdl, LINKSCAN_MODEs,
                                                     &linkscan_mode));
    std::string linkscan(linkscan_mode);
    if (linkscan.compare("SOFTWARE") == 0) {
      options->set_linkscan_mode(BcmPortOptions::LINKSCAN_MODE_SW);
    } else if (linkscan.compare("HARDWARE") == 0) {
      options->set_linkscan_mode(BcmPortOptions::LINKSCAN_MODE_HW);
    } else if (linkscan.compare("NO_SCAN") == 0) {
      options->set_linkscan_mode(BcmPortOptions::LINKSCAN_MODE_NONE);
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Port status and max frame size
  options->set_enabled(TRI_STATE_FALSE);
  options->set_max_frame_size(0);
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ENABLEs, &enabled));
    if (enabled) {
      options->set_enabled(TRI_STATE_TRUE);
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, MAX_FRAME_SIZEs, &max_frame_size));
    options->set_max_frame_size(max_frame_size);
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, PC_PHYS_PORT_IDs,
                                              &physical_device_port));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_get(entry_hdl, OPMODEs, &op_mode));
    std::string opmode(op_mode);
    if (opmode.compare(PC_PORT_OPMODE_AUTONEGs) == 0) {
      options->set_autoneg(TRI_STATE_TRUE);
    } else {
      options->set_autoneg(TRI_STATE_FALSE);
    }
    // Loopback status
    const char* loopback;
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_get(entry_hdl, LOOPBACK_MODEs, &loopback));
    std::string loopback_mode(loopback);
    if (loopback_mode == PC_LPBK_NONEs) {
      options->set_loopback_mode(LOOPBACK_STATE_NONE);
    } else if (loopback_mode == PC_LPBK_MACs) {
      options->set_loopback_mode(LOOPBACK_STATE_MAC);
    } else {
      return MAKE_ERROR(ERR_INTERNAL) << "Unknown loopback mode " << loopback;
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Number of serdes lanes and speed
  options->set_num_serdes_lanes(0);
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PHYS_PORTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, PC_PHYS_PORT_IDs, physical_device_port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, PC_PM_IDs, &port_macro_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

    options->set_flex(TRI_STATE_FALSE);
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PMs, &entry_hdl));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, PC_PM_IDs, port_macro_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_array_get(entry_hdl, LANE_MAPs, 0, &lanemap_array,
                                      1, &actual_count));
      int numLanes = __builtin_popcount(lanemap_array);
      options->set_num_serdes_lanes(numLanes);
      if (numLanes > 1) {
        options->set_flex(TRI_STATE_TRUE);
      }
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_array_get(entry_hdl, SPEED_MAXs, 0, &speed_array, 1,
                                      &actual_count));
      options->set_speed_bps(speed_array * kBitsPerMegabit);
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  } else {
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }
  // STP status
  const char* sym_res[140];
  options->set_blocked(TRI_STATE_FALSE);
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_STGs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_symbol_get(entry_hdl, STATEs, 0, sym_res, 140,
                                         &actual_count));
  std::string blocked(sym_res[port + 1]);
  if (blocked.compare(BLOCKs) == 0) {
    options->set_blocked(TRI_STATE_TRUE);
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetPortCounters(int unit, int port,
                                             PortCounters* pc) {
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port))
      << "Port " << port << " does not exit on unit " << unit << ".";
  CHECK_RETURN_IF_FALSE(pc != nullptr);

  uint64 value;
  // Read good counters
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_MACs, &entry_hdl));
  auto cl1 = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, RX_BYTESs, &value));
  pc->set_in_octets(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, RX_UC_PKTs, &value));
  pc->set_in_unicast_pkts(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, RX_BC_PKTs, &value));
  pc->set_in_broadcast_pkts(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, RX_MC_PKTs, &value));
  pc->set_in_multicast_pkts(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TX_BYTESs, &value));
  pc->set_out_octets(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TX_UC_PKTs, &value));
  pc->set_out_unicast_pkts(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TX_BC_PKTs, &value));
  pc->set_out_broadcast_pkts(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TX_MC_PKTs, &value));
  pc->set_out_multicast_pkts(value);

  // Read error counters
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, CTR_MAC_ERRs, &entry_hdl));
  auto cl2 = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, RX_FCS_ERR_PKTs, &value));
  pc->set_in_fcs_errors(value);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TX_ERR_PKTs, &value));
  pc->set_out_errors(value);

  // TODO(max): add missing fields: in_discards, in_errors, in_unknown_protos
  // out_discards

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitCLI() {
  // Initialize system log output
  RETURN_IF_BCM_ERROR(bcma_bslmgmt_init());

  // Initialize cli
  RETURN_IF_BCM_ERROR(bcma_sys_conf_cli_init(isc));

  /* Enable CLI redirection in BSL output hook */
  RETURN_IF_BCM_ERROR(bcma_bslmgmt_redir_hook_set(bcma_sys_conf_cli_redir_bsl));

  /* Add CLI commands for controlling the system log */
  RETURN_IF_BCM_ERROR(bcma_bslcmd_add_cmds(isc->cli));
  RETURN_IF_BCM_ERROR(bcma_bslcmd_add_cmds(isc->dsh));

  /* Add bcmlt commands */
  RETURN_IF_BCM_ERROR(bcma_bcmltcmd_add_cmds(isc->cli));

  /* Add CLI command completion support */
  RETURN_IF_BCM_ERROR(bcma_sys_conf_clirlc_init());

  /* Add CLI commands for base driver to debug shell */
  bcma_bcmbdcmd_add_cmicd_cmds(isc->dsh);
  bcma_bcmbdcmd_add_dev_cmds(isc->dsh);

  /* Add CLI commands for packet I/O driver */
  RETURN_IF_BCM_ERROR(bcma_bcmpktcmd_add_cmds(isc->cli));

  /* Add BCMLT C interpreter (CINT) */
  RETURN_IF_BCM_ERROR(bcma_cintcmd_add_cint_cmd(isc->cli));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartDiagShellServer() {
  //if (bcm_diag_shell_ == nullptr) return ::util::OkStatus();  // sim mode
  //RETURN_IF_ERROR(bcm_diag_shell_->StartServer());

  std::thread t([]() {
    // BCM CLI installs its own signal handler for SIGINT,
    // we have to restore the HAL one afterwards
    sighandler_t h = signal(SIGINT, SIG_IGN);
    bcma_cli_cmd_loop(isc->cli);
    bcma_cli_destroy(isc->cli);
    signal(SIGINT, h);
  });
  t.detach();

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartLinkscan(int unit) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t enable;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Subscribe the link change
  RETURN_IF_BCM_ERROR(
      bcmlt_table_subscribe(unit, LM_LINK_STATEs, &sdk_linkscan_callback,
                            NULL));

  absl::WriterMutexLock l(&data_lock_);
  // // Get logical ports for this unit
  auto logical_ports_map = gtl::FindOrNull(unit_to_logical_ports_, unit);
  CHECK_RETURN_IF_FALSE(logical_ports_map != nullptr)
      << "Logical ports are not identified on the Unit " << unit << ".";

  // Set linkscan mode for all the ports
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
  for (const auto& port : *logical_ports_map) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port.first));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, SOFTWAREs));
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                    BCMLT_PRIORITY_NORMAL));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                    BCMLT_PRIORITY_NORMAL));
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Enable link scan task and interval
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_CONTROLs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, SCAN_INTERVALs,
                                            FLAGS_linkscan_interval_in_usec));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, SCAN_ENABLEs, &enable));
    if (enable == 0) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, SCAN_ENABLEs, 1));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, SCAN_ENABLEs, 1));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopLinkscan(int unit) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Check if entry exists
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_CONTROLs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, SCAN_ENABLEs, 0));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  absl::WriterMutexLock l(&data_lock_);
  auto logical_ports_map = gtl::FindOrNull(unit_to_logical_ports_, unit);
  CHECK_RETURN_IF_FALSE(logical_ports_map != nullptr)
      << "Logical ports are not identified on the Unit " << unit << ".";

  // Disable linkscan mode for all the ports
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
  for (const auto& port : *logical_ports_map) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port.first));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, LINKSCAN_MODEs, NO_SCANs));
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                    BCMLT_PRIORITY_NORMAL));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                    BCMLT_PRIORITY_NORMAL));
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Unsubscribe the link change
  RETURN_IF_BCM_ERROR(bcmlt_table_unsubscribe(unit, LM_LINK_STATEs));
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::RegisterLinkscanEventWriter(
    std::unique_ptr<ChannelWriter<LinkscanEvent>> writer, int priority) {
  absl::WriterMutexLock l(&linkscan_writers_lock_);
  CHECK_RETURN_IF_FALSE(linkscan_event_writers_.size() <
                        static_cast<size_t>(FLAGS_max_num_linkscan_writers))
      << "Can only support " << FLAGS_max_num_linkscan_writers
      << " linkscan event Writers.";

  // Find the next available ID for the Writer.
  int next_id = kInvalidWriterId;
  for (int id = 1; id <= static_cast<int>(linkscan_event_writers_.size()) + 1;
       ++id) {
    auto it = std::find_if(
        linkscan_event_writers_.begin(), linkscan_event_writers_.end(),
        [id](const BcmLinkscanEventWriter& w) { return w.id == id; });
    if (it == linkscan_event_writers_.end()) {
      // This id is free. Pick it up.
      next_id = id;
      break;
    }
  }
  CHECK_RETURN_IF_FALSE(next_id != kInvalidWriterId)
      << "Could not find a new ID for the Writer. next_id=" << next_id << ".";

  linkscan_event_writers_.insert({std::move(writer), priority, next_id});

  return next_id;
}

::util::Status BcmSdkWrapper::UnregisterLinkscanEventWriter(int id) {
  absl::WriterMutexLock l(&linkscan_writers_lock_);
  auto it = std::find_if(
      linkscan_event_writers_.begin(), linkscan_event_writers_.end(),
      [id](const BcmLinkscanEventWriter& h) { return h.id == id; });
  CHECK_RETURN_IF_FALSE(it != linkscan_event_writers_.end())
      << "Could not find a linkscan event Writer with ID " << id << ".";

  linkscan_event_writers_.erase(it);

  return ::util::OkStatus();
}

::util::StatusOr<BcmPortOptions::LinkscanMode>
BcmSdkWrapper::GetPortLinkscanMode(int unit, int port) {
  const char* linkscan_str;
  BcmPortOptions::LinkscanMode linkscan_mode = BcmPortOptions::LINKSCAN_MODE_UNKNOWN;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, LM_PORT_CONTROLs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_get(entry_hdl, LINKSCAN_MODEs, &linkscan_str));
    std::string linkscan(linkscan_str);
    if (linkscan.compare("SOFTWARE") == 0) {
      linkscan_mode = BcmPortOptions::LINKSCAN_MODE_SW;
    } else if (linkscan.compare("HARDWARE") == 0) {
      linkscan_mode = BcmPortOptions::LINKSCAN_MODE_HW;
    } else if (linkscan.compare("NO_SCAN") == 0) {
      linkscan_mode = BcmPortOptions::LINKSCAN_MODE_NONE;
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return linkscan_mode;
}

::util::Status BcmSdkWrapper::SetMtu(int unit, int mtu) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, PC_PORTs, MAX_FRAME_SIZEs, &min, &max));
  if ((mtu > static_cast<int>(max)) ||
      (mtu < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid mtu (" << mtu << "), valid mtu range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  absl::WriterMutexLock l(&data_lock_);
  auto logical_ports_map = gtl::FindOrNull(unit_to_logical_ports_, unit);
  CHECK_RETURN_IF_FALSE(logical_ports_map != nullptr)
      << "Logical ports are not identified on the Unit " << unit << ".";
  CHECK_RETURN_IF_FALSE(unit_to_mtu_.count(unit));
  // Modify mtu for all the interfaces on this unit.
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PC_PORTs, &entry_hdl));
  for (const auto& port : *logical_ports_map) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAX_FRAME_SIZEs, mtu));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  unit_to_mtu_[unit] = mtu;
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3RouterIntf(int unit,
                                                              uint64 router_mac,
                                                              int vlan) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  int mtu = 0;
  {
    absl::ReaderMutexLock l(&data_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_mtu_.count(unit));
    mtu = unit_to_mtu_[unit];
  }
  CHECK_RETURN_IF_FALSE(router_mac);

  // check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, VLANs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  L3Interfaces entry(router_mac, vlan);
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(unit_to_l3_intf != nullptr)
      << "Unit " << unit << "  is not found in l3_interface_ids. Have you "
      << "called InitializeUnit for this unit before?";
  l3_intf_t l3_interface = {0, router_mac, vlan, 0xff, mtu};
  if (unit_to_l3_intf->count(entry)) {
    auto it = unit_to_l3_intf->find(entry);
    if (it != unit_to_l3_intf->end()) {
      l3_interface.l3a_intf_id = it->second;
      VLOG(1) << "L3 intf " << PrintL3RouterIntf(l3_interface)
              << " already exists on unit " << unit << ".";
      return it->second;
    }
  }

  // Check resource limits.
  if (unit_to_l3_intf->size() == unit_to_l3_intf_max_limit_[unit]) {
    return MAKE_ERROR(ERR_INTERNAL) << "L3 interface table full.";
  }

  // entry id
  std::set<int> l3_intf_ids = extract_values(*unit_to_l3_intf);
  int l3a_intf_id = unit_to_l3_intf_min_limit_[unit];
  if (!l3_intf_ids.empty()) {
    l3a_intf_id = *l3_intf_ids.rbegin() + 1;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_EIFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, l3a_intf_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_IDs, (vlan > 0 ? vlan : 1)));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_SAs, router_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, TTLs, 0xff));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // update map
  gtl::InsertOrDie(unit_to_l3_intf, entry, l3a_intf_id);
  l3_interface.l3a_intf_id = l3a_intf_id;
  VLOG(1) << "Created a new L3 router intf: " << PrintL3RouterIntf(l3_interface)
          << " on unit " << unit << ".";

  // update mtu
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_MTUs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_IDs, (vlan > 0 ? vlan : kDefaultVlan)));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_IDs, (vlan > 0 ? vlan : kDefaultVlan)));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_MTUs, mtu));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return l3a_intf_id;
}

::util::Status BcmSdkWrapper::DeleteL3RouterIntf(int unit, int router_intf_id) {
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(unit_to_l3_intf != nullptr)
      << "Unit " << unit << "  is not found in l3_interface_ids. Have you "
      << "called InitializeUnit for this unit before?";
  const L3Interfaces* entry = FindIndexOrNull(*unit_to_l3_intf, router_intf_id);
  if (!entry) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Router ID " << router_intf_id << " not found.";
  }
  // delete entry
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_EIFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, router_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // update map
  unit_to_l3_intf->erase(*entry);
  VLOG(1) << "Router intf with ID " << router_intf_id << " deleted on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3CpuEgressIntf(int unit) {
  bcmlt_entry_handle_t entry_hdl;
  int egress_intf_id = 0;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* l3_intfs = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_intfs != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // get next free slot
  ASSIGN_OR_RETURN(egress_intf_id,
      GetFreeSlot(l3_intfs, "L3 Port egress interface table is full."));
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, COPY_TO_CPUs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, 0x0));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // update map
  ConsumeSlot(l3_intfs, egress_intf_id);
  l3_intf_object_t l3_intf_o = {0, 0x0, 1, 0, 0};
  VLOG(1) << "Created a new L3 CPU egress intf: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3PortEgressIntf(
    int unit, uint64 nexthop_mac, int port, int vlan, int router_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  int egress_intf_id = 0;
  bool found;
  uint64_t max;
  uint64_t min;
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  InUseMap* l3_intfs = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_intfs != nullptr && unit_to_l3_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));

  // Check if router interface is valid
  if (!FindIndexOrNull(*unit_to_l3_intf, router_intf_id)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Router ID " << router_intf_id << " not found.";
  }

  // get next free slot
  ASSIGN_OR_RETURN(egress_intf_id,
      GetFreeSlot(l3_intfs, "L3 Port egress interface table is full."));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, nexthop_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, router_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, KEEP_VLANs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODPORTs, port));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ING_OVIDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // mark slot
  ConsumeSlot(l3_intfs, egress_intf_id);
  l3_intf_object_t l3_intf_o = {router_intf_id, nexthop_mac, vlan, port, 0};
  VLOG(1) << "Created a new L3 port egress intf: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3TrunkEgressIntf(
    int unit, uint64 nexthop_mac, int trunk, int vlan, int router_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  int egress_intf_id = 0;
  bool found;
  uint64_t max;
  uint64_t min;

  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, TRUNK_IDs, &min, &max));
  if ((trunk > static_cast<int>(max)) ||
      (trunk < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid trunk (" << trunk << "), valid trunk range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  InUseMap* l3_intfs = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_intfs != nullptr && unit_to_l3_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // get next free slot
  ASSIGN_OR_RETURN(egress_intf_id,
      GetFreeSlot(l3_intfs, "L3 Trunk egress interface table is full."));

  // Check if router interface is valid
  if (!FindIndexOrNull(*unit_to_l3_intf, router_intf_id)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Router ID " << router_intf_id << " not found.";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, nexthop_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, router_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, KEEP_VLANs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, TRUNK_IDs, trunk));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IS_TRUNKs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // update map
  ConsumeSlot(l3_intfs, egress_intf_id);
  l3_intf_object_t l3_intf_o = {router_intf_id, nexthop_mac, vlan, 0, trunk};
  VLOG(1) << "Created a new L3 trunk egress intf: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3DropIntf(int unit) {
  bcmlt_entry_handle_t entry_hdl;
  int egress_intf_id = 0;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* l3_intfs = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_intfs != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // get next free slot
  ASSIGN_OR_RETURN(egress_intf_id,
      GetFreeSlot(l3_intfs, "L3 Port egress interface table is full."));
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, DROPs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, 0x0));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // update map
  ConsumeSlot(l3_intfs, egress_intf_id);
  l3_intf_object_t l3_intf_o = {0, 0x0, 1, 0, 0};
  VLOG(1) << "Created a new L3 drop egress intf: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return egress_intf_id;
}

::util::Status BcmSdkWrapper::ModifyL3CpuEgressIntf(int unit,
                                                    int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  InUseMap::iterator it;
  uint64_t l3_eif_id;
  uint64_t mac_da;
  uint64_t vlan_id;
  uint64_t is_trunk;
  uint64_t trunk_id;
  uint64_t modport;

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, L3_EIF_IDs, &l3_eif_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, MAC_DAs, &mac_da));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, VLAN_IDs, &vlan_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, IS_TRUNKs, &is_trunk));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TRUNK_IDs, &trunk_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, MODPORTs, &modport));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, COPY_TO_CPUs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  l3_intf_object_t l3_intf_o;
  l3_intf_o.intf = static_cast<int>(l3_eif_id);
  l3_intf_o.mac_addr = mac_da;
  l3_intf_o.vlan = static_cast<int>(vlan_id);
  l3_intf_o.port = (is_trunk ? 0 : static_cast<int>(modport));
  l3_intf_o.trunk = (is_trunk ? static_cast<int>(trunk_id) : 0);

  VLOG(1) << "Modified L3 CPU egress intf while keeping its ID the same: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3PortEgressIntf(int unit,
                                                     int egress_intf_id,
                                                     uint64 nexthop_mac,
                                                     int port, int vlan,
                                                     int router_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  InUseMap::iterator it;
  bool found;
  uint64_t max;
  uint64_t min;
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr && unit_to_l3_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  // Check if router interface is valid
  if (!FindIndexOrNull(*unit_to_l3_intf, router_intf_id)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Router ID " << router_intf_id << " not found.";
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, router_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, nexthop_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODPORTs, port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, KEEP_VLANs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  l3_intf_object_t l3_intf_o;
  l3_intf_o.intf = router_intf_id;
  l3_intf_o.mac_addr = nexthop_mac;
  l3_intf_o.vlan = vlan;
  l3_intf_o.port = port;
  l3_intf_o.trunk = 0;

  VLOG(1) << "Modified L3 port egress intf while keeping its ID the same: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3TrunkEgressIntf(int unit,
                                                      int egress_intf_id,
                                                      uint64 nexthop_mac,
                                                      int trunk, int vlan,
                                                      int router_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  InUseMap::iterator it;
  bool found;
  uint64_t max;
  uint64_t min;
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_UC_NHOPs, TRUNK_IDs, &min, &max));
  if ((trunk > static_cast<int>(max)) ||
      (trunk < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid trunk (" << trunk << "), valid trunk range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  auto unit_to_l3_intf = gtl::FindOrNull(l3_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr && unit_to_l3_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  // Check if router interface is valid
  if (!FindIndexOrNull(*unit_to_l3_intf, router_intf_id)) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Router ID " << router_intf_id << " not found.";
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, L3_EIF_IDs, router_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_DAs, nexthop_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, KEEP_VLANs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, TRUNK_IDs, trunk));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IS_TRUNKs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  l3_intf_object_t l3_intf_o;
  l3_intf_o.intf = router_intf_id;
  l3_intf_o.mac_addr = nexthop_mac;
  l3_intf_o.vlan = vlan;
  l3_intf_o.port = 0;
  l3_intf_o.trunk = trunk;

  VLOG(1) << "Modified L3 trunk egress intf while keeping its ID the same: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3DropIntf(int unit, int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  InUseMap::iterator it;
  uint64_t l3_eif_id;
  uint64_t mac_da;
  uint64_t vlan_id;
  uint64_t is_trunk;
  uint64_t trunk_id;
  uint64_t modport;

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, L3_EIF_IDs, &l3_eif_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, MAC_DAs, &mac_da));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, VLAN_IDs, &vlan_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, IS_TRUNKs, &is_trunk));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, TRUNK_IDs, &trunk_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, MODPORTs, &modport));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, DROPs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODPORTs, 0)); // port
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0)); // module
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  l3_intf_object_t l3_intf_o;
  l3_intf_o.intf = static_cast<int>(l3_eif_id);
  l3_intf_o.mac_addr = mac_da;
  l3_intf_o.vlan = static_cast<int>(vlan_id);
  l3_intf_o.port = (is_trunk ? 0 : static_cast<int>(modport));
  l3_intf_o.trunk = (is_trunk ? static_cast<int>(trunk_id) : 0);

  VLOG(1) << "Modified L3 drop egress intf while keeping its ID the same: "
          << PrintL3EgressIntf(l3_intf_o, egress_intf_id) << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3EgressIntf(int unit, int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  InUseMap::iterator it;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  ReleaseSlot(l3_egress_intf, egress_intf_id);
  VLOG(1) << "Egress intf with ID " << egress_intf_id << " deleted on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindRouterIntfFromEgressIntf(
    int unit, int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  InUseMap::iterator it;
  uint64_t l3_eif_id;
  uint64_t mac_da;
  uint64_t copy_to_cpu;
  uint64_t dst_discard;

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_UC_NHOPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, L3_EIF_IDs, &l3_eif_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, MAC_DAs, &mac_da));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_get(entry_hdl, COPY_TO_CPUs, &copy_to_cpu));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, DROPs, &dst_discard));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if ((mac_da == 0x0) && (copy_to_cpu | dst_discard)) {
    // Returning a negative value to show that the router intf was not
    // created for this egress intf.
    return -1;
  }
  return static_cast<int>(l3_eif_id);
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateEcmpEgressIntf(
    int unit, const std::vector<int>& member_ids) {
  bcmlt_entry_handle_t entry_hdl;
  int ecmp_intf_id = 0;

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  uint64 members_array[kMaxEcmpGroupSize] = {};
  for (size_t i = 0; i < member_ids.size(); ++i) {
    members_array[i] = static_cast<uint64>(member_ids[i]);
  }
  int members_count = static_cast<int>(member_ids.size());

  InUseMap* ecmp_intfs = gtl::FindOrNull(l3_ecmp_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(ecmp_intfs != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // get next free slot
  ASSIGN_OR_RETURN(ecmp_intf_id,
      GetFreeSlot(ecmp_intfs, "ECMP egress interface table is full."));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, ECMPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ECMP_IDs, ecmp_intf_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NUM_PATHSs, members_count));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, NHOP_IDs, 0, members_array,
                                  members_count));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // consume slot
  ConsumeSlot(ecmp_intfs, ecmp_intf_id);
  VLOG(1) << "New ECMP group with ID " << ecmp_intf_id
          << " created with following egress intf IDs as members: "
          << PrintArray(members_array, members_count, ", ") << " on unit "
          << unit << ".";
  return ecmp_intf_id;
}

::util::Status BcmSdkWrapper::ModifyEcmpEgressIntf(
    int unit, int egress_intf_id, const std::vector<int>& member_ids) {
  bcmlt_entry_handle_t entry_hdl;
  InUseMap::iterator it;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  uint64 members_array[kMaxEcmpGroupSize] = {};
  for (size_t i = 0; i < member_ids.size(); ++i) {
    members_array[i] = static_cast<uint64>(member_ids[i]);
  }
  int members_count = static_cast<int>(member_ids.size());

  InUseMap* ecmp_intfs = gtl::FindOrNull(l3_ecmp_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(ecmp_intfs != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = ecmp_intfs->find(egress_intf_id);
  if (it != ecmp_intfs->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "ECMP egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid ECMP egress interface "
           << egress_intf_id << ".";
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, ECMPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NUM_PATHSs, members_count));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, NHOP_IDs, 0, members_array,
                                  members_count));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  VLOG(1) << "ECMP group with ID " << egress_intf_id
          << " modified with following egress intf IDs as members: "
          << PrintArray(members_array, members_count, ", ") << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteEcmpEgressIntf(int unit,
                                                   int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  InUseMap::iterator it;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  InUseMap* ecmp_intfs = gtl::FindOrNull(l3_ecmp_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(ecmp_intfs != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = ecmp_intfs->find(egress_intf_id);
  if (it != ecmp_intfs->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "ECMP egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid ECMP egress interface "
           << egress_intf_id << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, ECMPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // release slot
  ReleaseSlot(ecmp_intfs, egress_intf_id);
  VLOG(1) << "ECMP group with ID " << egress_intf_id << " deleted on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                             uint32 mask, int class_id,
                                             int egress_intf_id,
                                             bool is_intf_multipath) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  int rv;
  InUseMap::iterator it;
  l3_route_t route = {false, vrf, class_id, egress_intf_id, subnet, mask, "", ""};
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(
        GetFieldMinMaxValue(unit, L3_IPV4_UC_ROUTE_VRFs, CLASS_IDs, &min,
                            &max));
    if ((class_id > static_cast<int>(max)) ||
        (class_id < static_cast<int>(min))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid class_id (" << class_id << "), valid class_id range is "
             << static_cast<int>(min) << " - "
             << static_cast<int>(max) << ".";
    }
  }
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV4_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_MASKs,
                                            (!subnet ? 0 : (mask ? mask
                                                                 : 0xffffffff))));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, subnet));
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, is_intf_multipath));
  if (is_intf_multipath) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  }
  rv = bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                 BCMLT_PRIORITY_NORMAL);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (rv == SHR_E_EXISTS) {
    return MAKE_ERROR(ERR_ENTRY_EXISTS)
           << "IPv4 L3 LPM route " << PrintL3Route(route)
           << " already exists on unit " << unit << ".";
  }
  VLOG(1) << "Added IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3RouteIpv6(int unit, int vrf,
                                             const std::string& subnet,
                                             const std::string& mask,
                                             int class_id, int egress_intf_id,
                                             bool is_intf_multipath) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  InUseMap::iterator it;
  l3_route_t route = {true, vrf, class_id, egress_intf_id, 0, 0, "", ""};

  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);

  CHECK_RETURN_IF_FALSE(subnet.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(subnet.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(subnet.substr(8, 16));
  CHECK_RETURN_IF_FALSE(mask.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper_mask = ByteStreamToUint<uint64>(mask.substr(0, 8));
  uint64 ipv6_lower_mask = ByteStreamToUint<uint64>(mask.substr(8, 16));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(
        GetFieldMinMaxValue(unit, L3_IPV6_UC_ROUTE_VRFs, CLASS_IDs, &min,
                            &max));
    if ((class_id > static_cast<int>(max)) ||
        (class_id < static_cast<int>(min))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid class_id (" << class_id << "), valid class_id range is "
             << static_cast<int>(min) << " - "
             << static_cast<int>(max) << ".";
    }
  }
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // TODO(BRCM): fix ipv6, convert string to upper and lower ipv6 address
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV6_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPER_MASKs, ipv6_upper_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWER_MASKs, ipv6_lower_mask));
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, is_intf_multipath));
  if (is_intf_multipath) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  }
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  VLOG(1) << "Added IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                            int class_id, int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  l3_host_t host = {false, vrf, class_id, egress_intf_id, ipv4};
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_HOSTs, NHOP_IDs, &min, &max));
  if ((egress_intf_id > static_cast<int>(max)) ||
      (egress_intf_id < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress interface (" << egress_intf_id
           << "), valid next hop id range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV4_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, ipv4));
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, 0));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  VLOG(1) << "Added IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv6(int unit, int vrf,
                                            const std::string& ipv6,
                                            int class_id, int egress_intf_id) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  l3_host_t host = {true, vrf, class_id, egress_intf_id, 0, ipv6};

  CHECK_RETURN_IF_FALSE(ipv6.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(ipv6.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(ipv6.substr(8, 16));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_HOSTs, NHOP_IDs, &min, &max));
  if ((egress_intf_id > static_cast<int>(max)) ||
      (egress_intf_id < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress interface (" << egress_intf_id
           << "), valid next hop id range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV6_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, 0));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  VLOG(1) << "Added IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask,
                                                int class_id,
                                                int egress_intf_id,
                                                bool is_intf_multipath) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  bool entry_updated = false;
  InUseMap::iterator it;
  l3_route_t route = {false, vrf, class_id, egress_intf_id, subnet, mask, "", ""};
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(
        GetFieldMinMaxValue(unit, L3_IPV4_UC_ROUTE_VRFs, CLASS_IDs, &min,
                            &max));
    if ((class_id > static_cast<int>(max)) ||
        (class_id < static_cast<int>(min))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid class_id (" << class_id << "), valid class_id range is "
             << static_cast<int>(min) << " - "
             << static_cast<int>(max) << ".";
    }
  }
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV4_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_MASKs,
                                            (!subnet ? 0 : (mask ? mask
                                                                 : 0xffffffff))));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, subnet));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_MASKs,
                                              (!subnet ? 0 : (mask ? mask
                                                                   : 0xffffffff))));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, subnet));
    if (class_id > 0) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, is_intf_multipath));
    if (is_intf_multipath) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_updated = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_updated) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv4 L3 LPM route " << PrintL3Route(route)
           << " not found on unit " << unit << ".";
  }
  VLOG(1) << "Modify IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv6(
    int unit, int vrf, const std::string& subnet, const std::string& mask,
    int class_id, int egress_intf_id, bool is_intf_multipath) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  bool entry_updated = false;
  InUseMap::iterator it;
  // TODO(BRCM): fix ipv6, convert string to ipv6 address
  l3_route_t route = {true, vrf, class_id, egress_intf_id, 0, 0, subnet, mask};
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  CHECK_RETURN_IF_FALSE(subnet.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(subnet.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(subnet.substr(8, 16));
  CHECK_RETURN_IF_FALSE(mask.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper_mask = ByteStreamToUint<uint64>(mask.substr(0, 8));
  uint64 ipv6_lower_mask = ByteStreamToUint<uint64>(mask.substr(8, 16));
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  if (class_id > 0) {
    RETURN_IF_BCM_ERROR(
        GetFieldMinMaxValue(unit, L3_IPV6_UC_ROUTE_VRFs, CLASS_IDs, &min,
                            &max));
    if ((class_id > static_cast<int>(max)) ||
        (class_id < static_cast<int>(min))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid class_id (" << class_id << "), valid class_id range is "
             << static_cast<int>(min) << " - "
             << static_cast<int>(max) << ".";
    }
  }
  InUseMap* l3_egress_intf = gtl::FindOrNull(l3_egress_interface_ids_, unit);
  CHECK_RETURN_IF_FALSE(l3_egress_intf != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  // Check if egress interface is valid
  it = l3_egress_intf->find(egress_intf_id);
  if (it != l3_egress_intf->end()) {
    if (!it->second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "L3 Egress interface "
             << egress_intf_id << " is not created.";
    }
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid L3 Egress interface "
           << egress_intf_id << ".";
  }

  // TODO(BRCM): fix ipv6, convert string to upper and lower ipv6 addres
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV6_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPER_MASKs, ipv6_upper_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWER_MASKs, ipv6_lower_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    if (class_id > 0) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, is_intf_multipath));
    if (is_intf_multipath) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, ECMP_IDs, egress_intf_id));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_updated = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_updated) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv6 L3 LPM route " << PrintL3Route(route)
           << " not found on unit " << unit << ".";
  }
  VLOG(1) << "Modify IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                               int class_id,
                                               int egress_intf_id) {
  uint64_t max;
  uint64_t min;
  bool entry_updated = false;
  bcmlt_entry_info_t entry_info;
  bcmlt_entry_handle_t entry_hdl;
  l3_host_t host = {false, vrf, class_id, egress_intf_id, ipv4};
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_HOSTs, NHOP_IDs, &min, &max));
  if ((egress_intf_id > static_cast<int>(max)) ||
      (egress_intf_id < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress interface (" << egress_intf_id
           << "), valid next hop id range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV4_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, ipv4));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, ipv4));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, 0));
    if (class_id > 0) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_updated = true;
  }
  if (!entry_updated) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv4 L3 host " << PrintL3Host(host)
           << " not found on unit " << unit << ".";
  }
  VLOG(1) << "Modify IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6,
                                               int class_id,
                                               int egress_intf_id) {

  uint64_t max;
  uint64_t min;
  bool entry_updated = false;
  bcmlt_entry_info_t entry_info;
  bcmlt_entry_handle_t entry_hdl;
  l3_host_t host = {true, vrf, class_id, egress_intf_id, 0, ipv6};
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  CHECK_RETURN_IF_FALSE(ipv6.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(ipv6.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(ipv6.substr(8, 16));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_HOSTs, NHOP_IDs, &min, &max));
  if ((egress_intf_id > static_cast<int>(max)) ||
      (egress_intf_id < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid egress interface (" << egress_intf_id
           << "), valid next hop id range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // TODO(BRCM): fix ipv6, convert string to upper and lower ipv6 address
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV6_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    if (class_id > 0) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ECMP_NHOPs, 0));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, NHOP_IDs, egress_intf_id));
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_updated = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_updated) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv6 L3 host " << PrintL3Host(host)
           << " not found on unit " << unit << ".";
  }

  VLOG(1) << "Modify IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  uint64_t data;
  bool entry_delete = false;
  l3_route_t route = {false, vrf, 0, 0, subnet, mask, "", ""};
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV4_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_MASKs,
                                            (!subnet ? 0 : (mask ? mask
                                                                 : 0xffffffff))));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, subnet));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, CLASS_IDs, &data));
    route.l3a_lookup_class = static_cast<int>(data);
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_NHOPs, &data));
    if (static_cast<int>(data)) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_IDs, &data));
      route.l3a_intf = static_cast<int>(data);
    } else {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, NHOP_IDs, &data));
      route.l3a_intf = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_delete = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_delete) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv4 L3 LPM route " << PrintL3Route(route)
           << " not found on unit " << unit << ".";
  }
  VLOG(1) << "Deleted IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv6(int unit, int vrf,
                                                const std::string& subnet,
                                                const std::string& mask) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  uint64_t data;
  bool entry_delete = false;
  // TODO(BRCM): fix ipv6, convert string to ipv6 address
  l3_route_t route = {true, vrf, 0, 0, 0, 0, subnet, mask};
  CHECK_RETURN_IF_FALSE(subnet.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(subnet.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(subnet.substr(8, 16));
  CHECK_RETURN_IF_FALSE(mask.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper_mask = ByteStreamToUint<uint64>(mask.substr(0, 8));
  uint64 ipv6_lower_mask = ByteStreamToUint<uint64>(mask.substr(8, 16));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_ROUTE_VRFs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // TODO(BRCM): fix ipv6, convert string to upper and lower ipv6 addres
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L3_IPV6_UC_ROUTE_VRFs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPER_MASKs, ipv6_upper_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWER_MASKs, ipv6_lower_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, CLASS_IDs, &data));
    route.l3a_lookup_class = static_cast<int>(data);
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_NHOPs, &data));
    if (static_cast<int>(data)) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_IDs, &data));
      route.l3a_intf = static_cast<int>(data);
    } else {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, NHOP_IDs, &data));
      route.l3a_intf = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_delete = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_delete) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv6 L3 LPM route " << PrintL3Route(route)
           << " not found on unit " << unit << ".";
  }

  VLOG(1) << "Deleted IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv4(int unit, int vrf, uint32 ipv4) {
  uint64_t max;
  uint64_t min;
  uint64_t data;
  bool entry_delete = false;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  l3_host_t host = {false, vrf, 0, 0, ipv4};
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV4_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV4_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4s, ipv4));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, CLASS_IDs, &data));
    host.l3a_lookup_class = static_cast<int>(data);
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_NHOPs, &data));
    if (static_cast<int>(data)) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_IDs, &data));
      host.l3a_intf = static_cast<int>(data);
    } else {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, NHOP_IDs, &data));
      host.l3a_intf = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_delete = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_delete) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv4 L3 host " << PrintL3Host(host)
           << " not found on unit " << unit << ".";
  }
  VLOG(1) << "Deleted IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6) {
  uint64_t max;
  uint64_t min;
  uint64_t data;
  bool entry_delete = false;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // TODO(BRCM): fix ipv6, convert string to ipv6 address
  l3_host_t host = {true, vrf, 0, 0, 0, ipv6};

  CHECK_RETURN_IF_FALSE(ipv6.size() == 16); // TODO(max): is there a constant for that?
  uint64 ipv6_upper = ByteStreamToUint<uint64>(ipv6.substr(0, 8));
  uint64 ipv6_lower = ByteStreamToUint<uint64>(ipv6.substr(8, 16));

  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L3_IPV6_UC_HOSTs, VRF_IDs, &min, &max));
  if ((vrf > static_cast<int>(max)) ||
      (vrf < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vrf (" << vrf << "), valid vrf range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L3_IPV6_UC_HOSTs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VRF_IDs, vrf));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_UPPERs, ipv6_upper));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_LOWERs, ipv6_lower));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, CLASS_IDs, &data));
    host.l3a_lookup_class = static_cast<int>(data);
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_NHOPs, &data));
    if (static_cast<int>(data)) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ECMP_IDs, &data));
      host.l3a_intf = static_cast<int>(data);
    } else {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, NHOP_IDs, &data));
      host.l3a_intf = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                  BCMLT_PRIORITY_NORMAL));
    entry_delete = true;
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (!entry_delete) {
    return MAKE_ERROR(ERR_ENTRY_NOT_FOUND)
           << "IPv6 L3 host " << PrintL3Host(host)
           << " not found on unit " << unit << ".";
  }

  VLOG(1) << "Deleted IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::AddMyStationEntry(int unit, int priority,
                                                       int vlan, int vlan_mask,
                                                       uint64 dst_mac,
                                                       uint64 dst_mac_mask) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  uint8 mac[6];
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if vlan is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Check if vlan mask is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, VLAN_ID_MASKs, &min, &max));
  if ((vlan_mask > static_cast<int>(max)) ||
      (vlan_mask < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan_mask (" << vlan_mask << "), valid vlan_mask range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Check if priority is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, ENTRY_PRIORITYs, &min, &max));
  if ((priority > static_cast<int>(max)) ||
      (priority < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid priority (" << priority << "), valid priority range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }

  // Check if entry already exists.
  MyStationEntry entry(vlan, vlan_mask, dst_mac, dst_mac_mask);
  auto unit_to_my_stations = gtl::FindOrNull(my_station_ids_, unit);
  CHECK_RETURN_IF_FALSE(unit_to_my_stations != nullptr)
      << "Unit " << unit << "  is not found in unit_to_my_stations. Have you "
      << "called InitializeUnit for this unit before?";
  auto id = gtl::FindOrNull(*unit_to_my_stations, entry);
  if (id) {
    return *id;
  }
  // Check resource limits
  if (unit_to_my_stations->size() == unit_to_my_station_max_limit_[unit]) {
    return MAKE_ERROR(ERR_TABLE_FULL) << "MyStation table full.";
  }
  // insert entry
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_MY_STATIONs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, priority));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_ID_MASKs, vlan_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, dst_mac));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDR_MASKs, dst_mac_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_TERMINATIONs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_TERMINATIONs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // Get new station id
  std::set<int> stations_ids = extract_values(*unit_to_my_stations);
  int station_id = unit_to_my_station_min_limit_[unit];
  if (!stations_ids.empty()) {
    station_id = *stations_ids.rbegin() + 1; // last (=highest) id + 1
  }
  // update map
  gtl::InsertOrDie(unit_to_my_stations, entry, station_id);
  Uint64ToBcmMac(dst_mac, &mac);
  uint8 mac_mask[6];
  Uint64ToBcmMac(dst_mac_mask, &mac_mask);
  VLOG(1) << "Added dst MAC " << BcmMacToStr(mac) << "&&&"
          << BcmMacToStr(mac_mask) << " and VLAN "
          << vlan << " to my station TCAM with priority " << priority
          << " on unit " << unit << ".";
  return station_id;
}

::util::Status BcmSdkWrapper::DeleteMyStationEntry(int unit, int station_id) {
  bcmlt_entry_handle_t entry_hdl;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  auto unit_to_my_stations = gtl::FindOrNull(my_station_ids_, unit);
  CHECK_RETURN_IF_FALSE(unit_to_my_stations != nullptr)
      << "Unit " << unit << "  is not found in unit_to_my_stations. Have you "
      << "called InitializeUnit for this unit before?";
  const MyStationEntry* entry = FindIndexOrNull(*unit_to_my_stations, station_id);
  if (!entry) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Station ID " << station_id << " not found.";
  }
  // delete entry
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_MY_STATIONs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, entry->vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_ID_MASKs, entry->vlan_mask));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, entry->dst_mac));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDR_MASKs, entry->dst_mac_mask));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // delete map
  unit_to_my_stations->erase(*entry);
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL2Entry(int unit, int vlan, uint64 dst_mac,
                            int logical_port, int trunk_port,
                            int l2_mcast_group_id, int class_id,
                            bool copy_to_cpu, bool dst_drop) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_FDB_VLANs, &entry_hdl));
  auto _ = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, dst_mac));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, DEST_TYPEs,
      logical_port ? PORTs : trunk_port ? TRUNKs : L2_MC_GRPs));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, TRUNK_IDs, trunk_port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODIDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODPORTs, logical_port));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L2_MC_GRP_IDs, l2_mcast_group_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, CLASS_IDs, class_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, COPY_TO_CPUs, copy_to_cpu));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, STATICs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, DST_DROPs, dst_drop));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2Entry(int unit, int vlan, uint64 dst_mac) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_FDB_VLANs, &entry_hdl));
  auto _ = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, dst_mac));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL2MulticastEntry(int unit, int priority,
    int vlan, int vlan_mask, uint64 dst_mac, uint64 dst_mac_mask,
    bool copy_to_cpu, bool drop, uint8 l2_mcast_group_id) {
  bcmlt_entry_handle_t entry_hdl;
  uint64_t max;
  uint64_t min;
  uint8 mac[ETHER_ADDR_LEN];
  // Check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if vlan is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Check if vlan mask is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, VLAN_ID_MASKs, &min, &max));
  if ((vlan_mask > static_cast<int>(max)) ||
      (vlan_mask < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan_mask (" << vlan_mask << "), valid vlan_mask range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Check if priority is valid
  RETURN_IF_BCM_ERROR(
      GetFieldMinMaxValue(unit, L2_MY_STATIONs, ENTRY_PRIORITYs, &min, &max));
  if ((priority > static_cast<int>(max)) ||
      (priority < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid priority (" << priority << "), valid priority range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Insert entry
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_MY_STATIONs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, priority));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_ID_MASKs, vlan_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, dst_mac));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDR_MASKs, dst_mac_mask));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV4_TERMINATIONs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, IPV6_TERMINATIONs, 0));
  // Copy and drop are forced to true, because we do not expect the P4 program
  // to actually to set them in the action. This is an implementation detail of
  // of the current software multicast implementation.
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, COPY_TO_CPUs, copy_to_cpu || true));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, DROPs, drop || true));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // update map
  gtl::InsertOrDie(&dst_mac_to_multicast_group_id, dst_mac, l2_mcast_group_id);

  uint8 mac_mask[ETHER_ADDR_LEN];
  Uint64ToBcmMac(dst_mac, &mac);
  Uint64ToBcmMac(dst_mac_mask, &mac_mask);
  VLOG(1) << "Added dst MAC " << BcmMacToStr(mac) << "&&&" << BcmMacToStr(mac_mask)
          << " and VLAN "
          << vlan << " to my station TCAM with priority " << priority
          << " on unit " << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2MulticastEntry(int unit, int vlan, int vlan_mask, uint64 dst_mac,
                                        uint64 dst_mac_mask) {

  bcmlt_entry_handle_t entry_hdl;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // delete entry
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_MY_STATIONs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, VLAN_ID_MASKs, vlan_mask));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDRs, dst_mac));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, MAC_ADDR_MASKs, dst_mac_mask));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  // delete map
  dst_mac_to_multicast_group_id.erase(dst_mac);

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2EntriesByVlan(int unit, int vlan) {
  uint64_t current_vlan;
  uint64_t max;
  uint64_t min;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, VLANs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, L2_FDB_VLANs, &entry_hdl));
  while (SHR_E_NONE == bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_TRAVERSE,
                                          BCMLT_PRIORITY_NORMAL)) {
    if (bcmlt_entry_info_get(entry_hdl, &entry_info) != SHR_E_NONE ||
        entry_info.status != SHR_E_NONE) {
      break;
    }
    if (bcmlt_entry_field_get(entry_hdl, VLAN_IDs, &current_vlan) !=
        SHR_E_NONE) {
      break;
    }
    if (vlan == static_cast<int>(current_vlan)) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                    BCMLT_PRIORITY_NORMAL));
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, L2_FDB_VLAN_STATICs, &entry_hdl));
  while (SHR_E_NONE == bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_TRAVERSE,
                                          BCMLT_PRIORITY_NORMAL)) {
    if (bcmlt_entry_info_get(entry_hdl, &entry_info) != SHR_E_NONE ||
        entry_info.status != SHR_E_NONE) {
      break;
    }
    if (bcmlt_entry_field_get(entry_hdl, VLAN_IDs, &current_vlan) !=
        SHR_E_NONE) {
      break;
    }
    if (vlan == static_cast<int>(current_vlan)) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                    BCMLT_PRIORITY_NORMAL));
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  VLOG(1) << "Removed all L2 entries for VLAN " << vlan << " on unit " << unit
          << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddVlanIfNotFound(int unit, int vlan) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t untagged_members[3] = {0};
  uint64_t members[3] = {0};
  uint64_t max;
  uint64_t min;

  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, VLANs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  // Check if vlan exists
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
    VLOG(1) << "VLAN " << vlan << " already exists on unit " << unit << ".";
    return ::util::OkStatus();
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));

  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  // Make all vlans point to default STG
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_STG_IDs, kDefaultVlanStgId));

  // Include CPU to the member ports
  members[0] = 0xFFFFFFFFFFFFFFFFULL;  // all ports
  members[1] = kuint64max;
  untagged_members[0] = 0xFFFFFFFFFFFFFFFeULL; // exclude cpu port
  untagged_members[1] = kuint64max;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, EGR_MEMBER_PORTSs, 0, members, 3));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, ING_MEMBER_PORTSs, 0, members, 3));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_array_add(entry_hdl, UNTAGGED_MEMBER_PORTSs, 0,
                                  untagged_members, 3));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L3_IIF_IDs, 1));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  VLOG(1) << "Added VLAN " << vlan << " on unit " << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteVlanIfFound(int unit, int vlan) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t max;
  uint64_t min;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, VLANs, VLAN_IDs, &min, &max));
  if ((vlan > static_cast<int>(max)) ||
      (vlan < static_cast<int>(min))) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Invalid vlan (" << vlan << "), valid vlan range is "
           << static_cast<int>(min) << " - "
           << static_cast<int>(max) << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NOT_FOUND) {
    VLOG(1) << "VLAN " << vlan << " already deleted from unit " << unit << ".";
  } else if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
    int retval = bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE,
                                           BCMLT_PRIORITY_NORMAL);
    if (retval != SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
      return MAKE_ERROR(ERR_INTERNAL)
             << "Failed to delete VLAN " << vlan << " from unit " << unit << ".";
    }
    VLOG(1) << "Deleted VLAN " << vlan << " from unit " << unit << ".";
  } else {
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to delete VLAN " << vlan << " from unit " << unit << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureVlanBlock(int unit, int vlan,
                                                 bool block_broadcast,
                                                 bool block_known_multicast,
                                                 bool block_unknown_multicast,
                                                 bool block_unknown_unicast) {
  // TODO(max): the current mapping scheme of taking the lower 7 bits of the
  // vlan ID to create a vlan profile ID can result in collisions.
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64 data;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Get VLAN profile ID associated with VLAN
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
    return MAKE_ERROR(ERR_INVALID_PARAM) << "VLAN " << vlan << " does not exists on unit " << unit << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, VLAN_PROFILE_IDs, &data));
  uint8 profile_id = data;
  if (profile_id == 0) {
    profile_id = vlan & 0x7f; // Profile IDs are 7 bit
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Check if VLAN profile exists, create if needed
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_PROFILEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_PROFILE_IDs, profile_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (entry_info.status == SHR_E_NOT_FOUND) {
    VLOG(1) << "VLAN profile " << (uint16) profile_id << " does not exist.";
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_PROFILEs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_PROFILE_IDs, profile_id));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }

  // Set profile ID to VLAN ID
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_PROFILE_IDs, profile_id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(entry_info.status);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Configure blocking behaviour in profile
  // TODO(max): mapping from boolean args to BCM flags is not clear
  if (block_unknown_unicast ^ block_unknown_multicast) {
    LOG(WARNING) << "blocking does not differentiate between unknown uni and multicast";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_PROFILEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_PROFILE_IDs, profile_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L2_NON_UCAST_DROPs, block_broadcast));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, L2_MISS_DROPs, block_unknown_multicast || block_unknown_unicast));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                          BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureL2Learning(int unit, int vlan,
                                                  bool disable_l2_learning) {
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64 data;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Get VLAN profile ID associated with VLAN
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLANs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_IDs, vlan));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
    return MAKE_ERROR(ERR_INVALID_PARAM) << "VLAN " << vlan << " does not exists on unit " << unit << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, VLAN_PROFILE_IDs, &data));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  uint8 profile_id = data;
  VLOG(1) << "VLAN " << vlan << " has VLAN profile " << (uint16) profile_id;
  if (profile_id == 0) {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "VLAN " << vlan << " has no associated VLAN profile";
  }

  // This assumes the profile exists
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, VLAN_PROFILEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, VLAN_PROFILE_IDs, profile_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, NO_LEARNINGs, disable_l2_learning));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                          BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetL2AgeTimer(int unit, int l2_age_duration_sec) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::ConfigSerdesForPort(
    int unit, int port, uint64 speed_bps, int serdes_core, int serdes_lane,
    int serdes_num_lanes, const std::string& intf_type,
    const SerdesRegisterConfigs& serdes_register_configs,
    const SerdesAttrConfigs& serdes_attr_configs) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::CreateKnetIntf(int unit, int vlan,
                                             std::string* netif_name,
                                             int* netif_id) {
  CHECK_RETURN_IF_FALSE(netif_name != nullptr && netif_id != nullptr)
      << "Null netif_name or netif_id pointers.";
  CHECK_RETURN_IF_FALSE(!netif_name->empty())
      << "Empty netif name for unit " << unit << ".";
  CHECK_RETURN_IF_FALSE(netif_name->length() <=
                            static_cast<size_t>(BCMPKT_DEV_NAME_MAX))
      << "Oversize netif name for unit " << unit << ": " << *netif_name << ".";
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // Create netif
  bcmpkt_netif_t netif;
  memset(&netif, 0, sizeof(netif));
  // TODO(max): A valid VLAN (kDefaultVlan) is needed to get correct packet_in into the ingress pipeline
  // But that adds VLAN tags to direct packet_outs. Maybe if there is a way to stip outgoing VLAN tags.
  // netif.vlan = vlan > 0 ? vlan : kDefaultVlan; // TODO: Do we want VLAN tags on packetIO Tx packets?
  netif.max_frame_size = 1536;
  strncpy(netif.name, netif_name->c_str(), BCMPKT_DEV_NAME_MAX);
  netif.flags = BCMPKT_NETIF_F_RCPU_ENCAP;
  RETURN_IF_BCM_ERROR(bcmpkt_netif_create(unit, &netif));

  // TODO(BRCM): enable if required: Setup UNET
  RETURN_IF_BCM_ERROR(bcmpkt_unet_create(unit, netif.id));

  RETURN_IF_BCM_ERROR(bcmpkt_rx_register(unit, netif.id, 0,
                                         packet_receive_callback, NULL));

  *netif_id = netif.id;
  *netif_name = netif.name;
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DestroyKnetIntf(int unit, int netif_id) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(bcmpkt_rx_unregister(unit, netif_id, packet_receive_callback, 0));
  RETURN_IF_BCM_ERROR(bcmpkt_unet_destroy(unit, netif_id));
  RETURN_IF_BCM_ERROR(bcmpkt_netif_destroy(unit, netif_id));
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::CreateKnetFilter(int unit, int netif_id,
                                                      KnetFilterType type) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  bcmpkt_filter_t filter;
  memset(&filter, 0, sizeof(filter));
  filter.type = BCMPKT_FILTER_T_RX_PKT;
  filter.dest_type = BCMPKT_DEST_T_NETIF;
  filter.dest_id = netif_id;
  filter.dma_chan = 1;

  switch (type) {
    case KnetFilterType::CATCH_NON_SFLOW_FP_MATCH:
      // Send all the non-sflow packets which match an FP rule to controller.
      filter.priority = 0;  // hardcoded. Highest priority.
      snprintf(filter.desc, sizeof(filter.desc), "CATCH_NON_SFLOW_FP_MATCH");
      // TODO(max): For now we want all Rx packets to go to controller,
      // later we can implement more fine grained filtering
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_NONE);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_COUNT);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_SRC);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_DST);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_FLEX);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_CPU_SFLOW_SRC);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_CPU_SFLOW_DST);
      // BCMPKT_RX_REASON_CLEAR(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_CPU_SFLOW_FLEX);
      // BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_FFP);
      // BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_L2CPU);
      // BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_L3CPU);
      // BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_L3_NEXT_HOP);

      // filter.m_fp_rule = 1;  // This is a cookie we use for all the FP rules
      //                        // that send packets to CPU
      // BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_FFP);
      // filter.match_flags |= BCMPKT_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_SFLOW_FROM_INGRESS_PORT:
      // Send all ingress-sampled sflow packets to sflow agent.
      filter.priority = 2;  // hardcoded. Cannot use 1. 1 is reserved.
      snprintf(filter.desc, sizeof(filter.desc),
               "CATCH_SFLOW_FROM_INGRESS_PORT");
      BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_SRC);
      filter.match_flags |= BCMPKT_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_SFLOW_FROM_EGRESS_PORT:
      // Send all egress-sampled sflow packets to sflow agent.
      filter.priority = 3;  // hardcoded. Cannot use 1. 1 is reserved.
      snprintf(filter.desc, sizeof(filter.desc),
               "CATCH_SFLOW_FROM_EGRESS_PORT");
      BCMPKT_RX_REASON_SET(filter.m_reason, BCMPKT_RX_REASON_CPU_SFLOW_DST);
      filter.match_flags |= BCMPKT_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_ALL:
      filter.priority = 10; // hardcoded. Lowest priority.
      snprintf(filter.desc, sizeof(filter.desc), "CATCH_ALL");
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Un-supported KNET filter type.";
  }
  RETURN_IF_BCM_ERROR(bcmpkt_filter_create(unit, &filter));
  return filter.id;
}

::util::Status BcmSdkWrapper::DestroyKnetFilter(int unit, int filter_id) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(bcmpkt_filter_destroy(unit, filter_id));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartRx(int unit, const RxConfig& rx_config) {

  bcmpkt_dma_chan_t chan;
  bcmpkt_dev_init_t cfg;
  bcmpkt_netif_t netif;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // clean up previous configuration
  RETURN_IF_BCM_ERROR(bcmpkt_dev_cleanup(unit));

  // Initialize device
  memset(&cfg, 0, sizeof(cfg));
  cfg.cgrp_size = 4; //
  cfg.cgrp_bmp = 0x7;
  RETURN_IF_BCM_ERROR(bcmpkt_dev_init(unit, &cfg));

  // Configure TX channel
  memset(&chan, 0, sizeof(chan));
  chan.id = 0;
  chan.dir = BCMPKT_DMA_CH_DIR_TX;
  chan.ring_size = 64;
  chan.max_frame_size = 1536;
  RETURN_IF_BCM_ERROR(bcmpkt_dma_chan_set(unit, &chan));

  // Configure RX channel
  memset(&chan, 0, sizeof(chan));
  chan.id = 1;
  chan.dir = BCMPKT_DMA_CH_DIR_RX;
  chan.ring_size = 64;
  chan.max_frame_size = 1536;
  RETURN_IF_BCM_ERROR(bcmpkt_dma_chan_set(unit, &chan));

  // Map all queues to Rx channel
  // We have to store the string in a non-const char array, because the SDKLT
  // API is not const and C++ string literals are "const char*"
  char kCliChannelMapString[] =
      "pktdev chan queuemap 1 highword=0xffff lowword=0xffffffff";
  RETURN_IF_BCM_ERROR(bcma_cli_bshell(unit, kCliChannelMapString));

  // Bringup network device
  RETURN_IF_BCM_ERROR(bcmpkt_dev_enable(unit));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopRx(int unit) {
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetRateLimit(
    int unit, const RateLimitConfig& rate_limit_config) {
  uint64_t max;
  uint64_t min;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, TM_SCHEDULER_SHAPER_CPU_NODEs,
                                          TM_SCHEDULER_SHAPER_CPU_NODE_IDs,
                                          &min, &max));
  // Sanity checking.
  for (const auto& e : rate_limit_config.per_cos_rate_limit_configs) {
    CHECK_RETURN_IF_FALSE(e.first <= static_cast<int32_t>(max));
  }

  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, TM_SHAPER_PORTs, &entry_hdl));
  // hardcoding CPU PORT
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                         BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));

  // Apply global rate limit.
  // hardcoding CPU PORT
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, BANDWIDTH_KBPSs,
                                            rate_limit_config.max_rate_pps));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, BURST_SIZE_KBITSs,
                                            rate_limit_config.max_burst_pkts));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_symbol_add(entry_hdl, SHAPING_MODEs, PACKET_MODEs));
  if (entry_info.status == SHR_E_NONE) {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                  BCMLT_PRIORITY_NORMAL));
  } else {
    RETURN_IF_BCM_ERROR(
        bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                  BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Apply per cos rate limit.
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, TM_SCHEDULER_SHAPER_CPU_NODEs, &entry_hdl));
  for (const auto& e : rate_limit_config.per_cos_rate_limit_configs) {
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, SCHED_NODEs, L1_SCHED_NODEs));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, TM_SCHEDULER_SHAPER_CPU_NODE_IDs,
                              e.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP,
                                           BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    RETURN_IF_BCM_ERROR(bcmlt_entry_clear(entry_hdl));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, SCHED_NODEs, L1_SCHED_NODEs));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, TM_SCHEDULER_SHAPER_CPU_NODE_IDs,
                              e.first));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAX_BURST_SIZE_KBITSs,
                                              e.second.max_burst_pkts));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MAX_BANDWIDTH_KBPSs,
                                              e.second.max_rate_pps));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_symbol_add(entry_hdl, SHAPING_MODEs, PACKET_MODEs));
    if (entry_info.status == SHR_E_NONE) {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE,
                                    BCMLT_PRIORITY_NORMAL));
    } else {
      RETURN_IF_BCM_ERROR(
          bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                    BCMLT_PRIORITY_NORMAL));
    }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetKnetHeaderForDirectTx(int unit, int port,
                                                       int cos, uint64 smac,
                                                       size_t packet_len,
                                                       std::string* header) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  // Check if port is valid
  RETURN_IF_BCM_ERROR(CheckIfPortExists(unit, port));

  CHECK_RETURN_IF_FALSE(header != nullptr);
  header->clear();

  // TODO(max): update this comment
  // Try to find the headers for the packet that goes to a port directly. The
  // format of the packet is the following:
  //  --------------------------------------------------------------------
  // | RCPU header | SOB module header (SOBMH) | unused TX meta | payload |
  //  --------------------------------------------------------------------
  // Note that the total length of TX meta (SOBMH + unused TX meta) is fixed.
  // The header returned from the string will contain RCPU header + TX meta.

  //------------------------------------------
  // RCPU header
  //------------------------------------------
  struct RcpuHeader rcpu_header;
  memset(&rcpu_header, 0, sizeof(rcpu_header));
  static_assert(sizeof(rcpu_header) == BCMPKT_RCPU_HDR_LEN, "sizeof(rcpu_header) != BCMPKT_RCPU_HDR_LEN");

  // For RCPU header, smac is the given smac (read from the KNET netif). dmac
  // is set to 0.
  Uint64ToBcmMac(smac, &rcpu_header.ether_header.ether_shost);
  Uint64ToBcmMac(0, &rcpu_header.ether_header.ether_dhost);

  // RCPU header is always VLAN tagged. We use a fixed special VLAN ID for
  // RCPU headers.
  rcpu_header.ether_header.ether_type = htons(kRcpuVlanEthertype); // bcmpkt_rcpu_hdr_s.tpid
  rcpu_header.vlan_tag.vlan_id = htons(kRcpuVlanId);
  rcpu_header.vlan_tag.type = htons(kRcpuEthertype); // bcmpkt_rcpu_hdr_s.ethertype

  // Now fill up the RCPU data.
  // TODO(max): Return & check if NULL
  bcmdrd_dev_t *dev;
  dev = bcmdrd_dev_get(unit);
  uint16 pci_device = 0;
  if (dev != NULL) {
    pci_device = dev->id.device_id;
  }
  // TODO(BRCM): verify 'pci_device' is valid or not in unit test
  rcpu_header.rcpu_data.rcpu_signature = htons(pci_device & ~0xf);
  rcpu_header.rcpu_data.rcpu_opcode = BCMPKT_RCPU_OP_TX;
  rcpu_header.rcpu_data.rcpu_flags |= kRcpuFlagModhdr;  // we add SOBMH later
  rcpu_header.rcpu_data.rcpu_payloadlen = htons(packet_len);
  rcpu_header.rcpu_data.rcpu_metalen = BCMPKT_TXPMD_SIZE_BYTES;

  std::string s0((char*)&rcpu_header, sizeof(rcpu_header));
  VLOG(2) << "RCPU: " << StringToHex(s0);
  header->assign(reinterpret_cast<const char*>(&rcpu_header),
                 sizeof(rcpu_header));

  //------------------------------------------
  // SOB module header (SOBMH)
  //------------------------------------------
  // The rest of the code is chip-dependent. Need to see which chip we are
  // talking about
  ASSIGN_OR_RETURN(auto chip_type, GetChipType(unit));
  CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK)
      << "Un-supported BCM chip type: " << BcmChip::BcmChipType_Name(chip_type);

  uint32 meta[BCMPKT_TXPMD_SIZE_WORDS];
  memset(meta, 0, sizeof(meta));
  #define TXPMD_START_IHEADER 2
  #define TXPMD_HEADER_TYPE_FROM_CPU 1
  bcmdrd_dev_type_t dev_type;
  RETURN_IF_BCM_ERROR(bcmpkt_dev_type_get(unit, &dev_type));

  RETURN_IF_BCM_ERROR(bcmpkt_txpmd_field_set(dev_type, meta, BCMPKT_TXPMD_START, TXPMD_START_IHEADER));
  RETURN_IF_BCM_ERROR(bcmpkt_txpmd_field_set(dev_type, meta, BCMPKT_TXPMD_HEADER_TYPE, TXPMD_HEADER_TYPE_FROM_CPU));
  RETURN_IF_BCM_ERROR(bcmpkt_txpmd_field_set(dev_type, meta, BCMPKT_TXPMD_UNICAST, 1));
  RETURN_IF_BCM_ERROR(bcmpkt_txpmd_field_set(dev_type, meta, BCMPKT_TXPMD_LOCAL_DEST_PORT, port));
  RETURN_IF_BCM_ERROR(bcmpkt_txpmd_field_set(dev_type, meta, BCMPKT_TXPMD_COS, cos));

  VLOG(2) << "txpmd: " << StringToHex(std::string((char*)meta, sizeof(meta)));
  header->append(reinterpret_cast<const char*>(meta), sizeof(meta));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetKnetHeaderForIngressPipelineTx(
    int unit, uint64 smac, size_t packet_len, std::string* header) {
  CHECK_RETURN_IF_FALSE(header != nullptr);
  header->clear();

  // Try to find the headers for the packet that goes to ingress pipeline.
  // There is no SOBMH (or TX meta) before the payload. The format of the
  // packet is the following:
  //  -----------------------
  // | RCPU header | payload |
  //  -----------------------
  // The header returned from the string will just be the RCPU header.

  //------------------------------------------
  // RCPU header
  //------------------------------------------
  struct RcpuHeader rcpu_header;
  memset(&rcpu_header, 0, sizeof(rcpu_header));

  // For RCPU header, smac is the given smac (read from the KNET netif). dmac
  // is set to 0.
  Uint64ToBcmMac(smac, &rcpu_header.ether_header.ether_shost);
  Uint64ToBcmMac(0, &rcpu_header.ether_header.ether_dhost);

  // RCPU header is always VLAN tagged. We use a fixed special VLAN ID for
  // RCPU headers.
  rcpu_header.ether_header.ether_type = htons(kRcpuVlanEthertype); // bcmpkt_rcpu_hdr_s.tpid
  rcpu_header.vlan_tag.vlan_id = htons(kRcpuVlanId);
  rcpu_header.vlan_tag.type = htons(kRcpuEthertype); // bcmpkt_rcpu_hdr_s.ethertype

  // Now fill up the RCPU data.
  // TODO(max): Return & check if NULL
  bcmdrd_dev_t *dev;
  dev = bcmdrd_dev_get(unit);
  uint16 pci_device = 0;
  if (dev != NULL) {
    pci_device = dev->id.device_id;
  }
  // TODO(BRCM): verify 'pci_device' is valid or not in unit test
  rcpu_header.rcpu_data.rcpu_signature = htons(pci_device & ~0xf);
  rcpu_header.rcpu_data.rcpu_opcode = BCMPKT_RCPU_OP_TX;
  rcpu_header.rcpu_data.rcpu_flags |= kRcpuFlagModhdr;  // we add SOBMH later
  rcpu_header.rcpu_data.rcpu_payloadlen = htons(packet_len);
  rcpu_header.rcpu_data.rcpu_metalen = 0;

  header->assign(reinterpret_cast<const char*>(&rcpu_header), sizeof(rcpu_header));

  return ::util::OkStatus();
}

size_t BcmSdkWrapper::GetKnetHeaderSizeForRx(int unit) {
  return sizeof(RcpuHeader) + kRcpuRxMetaSize;
}

static void dumpRxpmdHeaderRaw(const void* rxpmd) {
  auto words = static_cast<const uint32*>(rxpmd);
  for (int i = 0; i < BCMPKT_RXPMD_SIZE_WORDS; ++i) {
    VLOG(2) << "rxpmd [word " << absl::StrFormat("%02i", i) << "]: " << absl::StrFormat("%08x", words[i]);
  }
}

::util::Status BcmSdkWrapper::ParseKnetHeaderForRx(int unit,
                                                   const std::string& header,
                                                   int* ingress_logical_port,
                                                   int* egress_logical_port,
                                                   int* cos) {
  // The format of the incoming packets is the following:
  //  ----------------------------------
  // | RCPU header | RX meta | payload |
  //  ----------------------------------
  // Note that the total length of RX meta is fixed. The header passed to this
  // method will contain RCPU header + RX meta.
  CHECK_RETURN_IF_FALSE(header.length() == sizeof(RcpuHeader) + kRcpuRxMetaSize)
      << "Invalid KNET header size for RX (" << header.length()
      << " != " << sizeof(RcpuHeader) + kRcpuRxMetaSize << ").";

  // Valid RCPU header. We dont care about src/dst MACs in RCPU header here.
  const struct RcpuHeader* rcpu_header =
      reinterpret_cast<const struct RcpuHeader*>(header.data());
  CHECK_RETURN_IF_FALSE(ntohs(rcpu_header->ether_header.ether_type) ==
                        kRcpuVlanEthertype)
      << ntohs(rcpu_header->ether_header.ether_type)
      << " != " << kRcpuVlanEthertype;
  CHECK_RETURN_IF_FALSE((ntohs(rcpu_header->vlan_tag.vlan_id) & kVlanIdMask) ==
                        kRcpuVlanId)
      << (ntohs(rcpu_header->vlan_tag.vlan_id) & kVlanIdMask)
      << " != " << kRcpuVlanId;
  CHECK_RETURN_IF_FALSE(ntohs(rcpu_header->vlan_tag.type) == kRcpuEthertype)
      << ntohs(rcpu_header->vlan_tag.type) << " != " << kRcpuEthertype;
  CHECK_RETURN_IF_FALSE(rcpu_header->rcpu_data.rcpu_opcode ==
                        kRcpuOpcodeToCpuPkt)
      << rcpu_header->rcpu_data.rcpu_opcode << " != " << kRcpuOpcodeToCpuPkt;
  CHECK_RETURN_IF_FALSE(rcpu_header->rcpu_data.rcpu_flags == kRcpuFlagModhdr)
      << rcpu_header->rcpu_data.rcpu_flags << " != " << kRcpuFlagModhdr;

  // Parse RX meta. The rest of the code is chip-dependent.
  ASSIGN_OR_RETURN(auto chip_type, GetChipType(unit));
  CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK)
      << "Un-supported BCM chip type: " << BcmChip::BcmChipType_Name(chip_type);

  // TODO(max): this is broken the same way parseKnetHeaderForTx is/was
  int src_module = -1, dst_module = -1, src_port = -1, dst_port = -1,
      op_code = -1;

  const char* rxpmd = header.data() + sizeof(RcpuHeader);
  dumpRxpmdHeaderRaw(rxpmd);

  bcmdrd_dev_type_t dev_type;
  uint32 val;
  RETURN_IF_BCM_ERROR(bcmpkt_dev_type_get(unit, &dev_type));
  uint32* meta = reinterpret_cast<uint32*>(const_cast<char*>(&header[0]) + sizeof(RcpuHeader));
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_CPU_COS, &val));
  *cos = val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_SRC_PORT_NUM, &val));
  src_port = val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_QUEUE_NUM, &val));
  VLOG(2) << "queue_num " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_OUTER_VID, &val));
  VLOG(2) << "outer vid " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_MATCHED_RULE, &val));
  VLOG(2) << "matched rule " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_PKT_LENGTH, &val));
  VLOG(2) << "packet length " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_REASON_TYPE, &val));
  VLOG(2) << "reason type " << val;
  bcmpkt_rx_reasons_t reasons;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_reasons_get(dev_type, meta, &reasons));
  // VLOG(1) << "reason: " << reasons; TODO(max)

  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_HGI, &val));
  VLOG(2) << "hgi " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_TIMESTAMP_TYPE, &val));
  VLOG(2) << "timestamp type " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_TIMESTAMP, &val));
  VLOG(2) << "timestamp " << val;
  RETURN_IF_BCM_ERROR(bcmpkt_rxpmd_field_get(dev_type, meta, BCMPKT_RXPMD_TIMESTAMP_HI, &val));
  VLOG(2) << "timestamp hi " << val;
  dst_port = GetRxpmdField<uint8, 4, 7, 0>(meta); // Reverse engineered dst port
  VLOG(2) << "manual pktlen " << GetRxpmdField<uint16, 3, 21, 8>(meta);

  // TODO(max): make checker happy for now by faking the missing values
  src_module = dst_module = 0;
  op_code = 1;

  // TODO(BRCM): hardcoding module to '0'
  int module = 0;
  VLOG(1) << "Parsed metadata: (op_code=" << op_code
          << ", src_mod=" << src_module << ", dst_mod=" << dst_module
          << ", base_mod=" << module << ", src_port=" << src_port
          << ", dst_port=" << dst_port << ", cos=" << *cos << ").";
  // Now do some validation on the parsed metadata. First note that BCM chips
  // can generally support multiple modules per unit. But we do not allow that
  // in our switches. So all the ports on a unit must have "one" module num. We
  // add a check here to make sure this assumption is always correct. Second,
  // for the (dst_module, dst_port) the value received after parsing the header
  // depends on the op_code.
  CHECK_RETURN_IF_FALSE(src_module == module)
      << "Invalid src_module: (op_code=" << op_code
      << ", src_mod=" << src_module << ", dst_mod=" << dst_module
      << ", base_mod=" << module << ", src_port=" << src_port
      << ", dst_port=" << dst_port << ", cos=" << *cos << ").";
  switch (op_code) {
    // TODO(max): use the defines instead of numbers?
    case 1:  // BCMPKT_OPCODE_UC
      CHECK_RETURN_IF_FALSE(dst_module == module)
          << "Invalid dst_module: (op_code=" << op_code
          << ", src_mod=" << src_module << ", dst_mod=" << dst_module
          << ", base_mod=" << module << ", src_port=" << src_port
          << ", dst_port=" << dst_port << ", cos=" << *cos << ").";
      *ingress_logical_port = src_port;
      *egress_logical_port = dst_port;
      break;
    case 0:  // BCMPKT_OPCODE_CPU
    case 2:  // BCMPKT_OPCODE_BC
      // Dont care about dst_module and dst_port.
      *ingress_logical_port = src_port;
      *egress_logical_port = 0;  // CPU port
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Invalid op_code: (op_code=" << op_code
             << ", src_mod=" << src_module << ", dst_mod=" << dst_module
             << ", base_mod=" << module << ", src_port=" << src_port
             << ", dst_port=" << dst_port << ", cos=" << *cos << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitAclHardware(int unit) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclControl(int unit,
                                            const AclControl& acl_control) {
  bcmlt_entry_handle_t entry_hdl;
  // Check if the unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // All ACL stages are by default enabled for all ports
  // Check external port ACL enable flags
  if (acl_control.extern_port_flags.apply) {
    // TODO(BRCM): get external port list and apply flags per port
    // need more clarity on the usage
  }
  // Check internal port ACL enable flags
  if (acl_control.intern_port_flags.apply) {
    // TODO(BRCM): get internal port list and apply flags per port
    // need more clarity on the usage
  }
  LOG(WARNING) << "Currently not explicitly enabling/disabling ACL stages for "
               << "packets ingressing on internal and external ports.";
  // Check CPU port ACL enable flags
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, PORT_FPs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PORT_IDs, kCpuLogicalPort));
  if (acl_control.cpu_port_flags.apply) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLANs, acl_control.cpu_port_flags.vfp_enable));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_INGs, acl_control.cpu_port_flags.ifp_enable));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGRs, acl_control.cpu_port_flags.efp_enable));
    RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT, BCMLT_PRIORITY_NORMAL));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  // Apply intra-slice double wide enable flag
  if (acl_control.intra_double_wide_enable.apply) {
     LOG(WARNING) << "Enabling intra-slice double wide is not supported.";
  }

  // Apply stats collection hardware read-through enable flag (slower)
  if (acl_control.stats_read_through_enable.apply) {
     LOG(WARNING) << "Stats collection hardware read-through is not supported.";
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclUdfChunks(int unit, const BcmUdfSet& udfs) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclUdfChunks(int unit, BcmUdfSet* udfs) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

namespace {

std::vector<std::pair<std::string, std::string>>
     GetPktTypeAndMode(std::vector<std::string> qualifiers) {
  // L2_SINGLE_WIDE, PORT_ANY_PACKET_ANY
  std::vector<std::string> PortAnyPktAnyL2SingleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktAnyL2SingleWide.begin(), PortAnyPktAnyL2SingleWide.end());

  // L3_SINGLE_WIDE, PORT_ANY_PACKET_IPV4
  std::vector<std::string> PortAnyPktIPV4L3SingleWide = {
    QUAL_L4_PKTs, QUAL_INT_PRIs, QUAL_COLORs, QUAL_IP_FLAGS_MFs,
    QUAL_TCP_CONTROL_FLAGSs, QUAL_L4DST_PORTs, QUAL_L4SRC_PORTs,
    QUAL_ICMP_TYPE_CODEs, QUAL_TTLs, QUAL_IP_PROTOCOLs,
    QUAL_DST_IP4s, QUAL_SRC_IP4s, QUAL_TOSs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs, QUAL_EGR_DVP_CLASS_IDs,
    QUAL_FWD_VLAN_IDs, QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs,
    QUAL_INT_CNs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktIPV4L3SingleWide.begin(), PortAnyPktIPV4L3SingleWide.end());

  // L3_DOUBLE_WIDE, PORT_ANY_PACKET_IPV4
  std::vector<std::string> PortAnyPktIPV4L3DoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
    QUAL_IP_FLAGS_MFs, QUAL_TCP_CONTROL_FLAGSs, QUAL_L4DST_PORTs,
    QUAL_L4SRC_PORTs, QUAL_ICMP_TYPE_CODEs, QUAL_TTLs,
    QUAL_IP_PROTOCOLs, QUAL_DST_IP4s, QUAL_SRC_IP4s,
    QUAL_TOSs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktIPV4L3DoubleWide.begin(), PortAnyPktIPV4L3DoubleWide.end());

  // L3_ALT_DOUBLE_WIDE, PORT_ANY_PACKET_IPV4
  std::vector<std::string> PortAnyPktIPV4L3AltDoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
    QUAL_IP_FLAGS_MFs, QUAL_TCP_CONTROL_FLAGSs, QUAL_L4DST_PORTs,
    QUAL_L4SRC_PORTs, QUAL_ICMP_TYPE_CODEs, QUAL_TTLs,
    QUAL_IP_PROTOCOLs, QUAL_DST_IP4s, QUAL_SRC_IP4s,
    QUAL_TOSs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktIPV4L3AltDoubleWide.begin(), PortAnyPktIPV4L3AltDoubleWide.end());

  // L3_ANY_SINGLE_WIDE, PORT_ANY_PACKET_IP
  std::vector<std::string> PortAnyPktIPL3AnySingleWide = {
    QUAL_L4_PKTs, QUAL_INT_PRIs, QUAL_COLORs, QUAL_IP_FLAGS_MFs,
    QUAL_TCP_CONTROL_FLAGSs, QUAL_L4DST_PORTs, QUAL_L4SRC_PORTs,
    QUAL_ICMP_TYPE_CODEs, QUAL_TTLs, QUAL_IP_PROTOCOLs,
    QUAL_DST_IP4s, QUAL_SRC_IP4s, QUAL_TOSs,
    QUAL_INNER_VLAN_IDs, QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs,
    QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs,
    QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs,
    QUAL_OUTPORTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktIPL3AnySingleWide.begin(), PortAnyPktIPL3AnySingleWide.end());

  // L3_SINGLE_WIDE, PORT_ANY_PACKET_NONIP
  std::vector<std::string> PortAnyPktNonIPL3SingleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktNonIPL3SingleWide.begin(), PortAnyPktNonIPL3SingleWide.end());

  // L3_DOUBLE_WIDE, PORT_ANY_PACKET_NONIP
  std::vector<std::string> PortAnyPktNonIPL3DoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs,
    QUAL_DROP_PKTs, QUAL_BYTES_AFTER_L2HEADERs,
  };
  std::sort(PortAnyPktNonIPL3DoubleWide.begin(), PortAnyPktNonIPL3DoubleWide.end());

  // L3_ANY_SINGLE_WIDE, PORT_ANY_PACKET_NONIP
  std::vector<std::string> PortAnyPktNonIPL3AnySingleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs,
    QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs,
    QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktNonIPL3AnySingleWide.begin(), PortAnyPktNonIPL3AnySingleWide.end());

  // L3_ALT_DOUBLE_WIDE, PORT_ANY_PACKET_NONIP
  std::vector<std::string> PortAnyPktNonIPL3AltDoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs, QUAL_INT_PRIs,
    QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs, QUAL_SRC_MACs,
    QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs, QUAL_INNER_VLAN_CFIs,
    QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs, QUAL_INPORTs,
    QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs,
    QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs,
    QUAL_VLAN_OUTER_PRESENTs, QUAL_OUTPORTs, QUAL_CPU_COSs,
    QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs, QUAL_VRFs, QUAL_VPNs,
    QUAL_FWD_TYPEs, QUAL_INT_CNs, QUAL_DROP_PKTs, QUAL_BYTES_AFTER_L2HEADERs,
  };
  std::sort(PortAnyPktNonIPL3AltDoubleWide.begin(), PortAnyPktNonIPL3AltDoubleWide.end());

  // L3_SINGLE_WIDE, PORT_ANY_PACKET_IPV6
  std::vector<std::string> PortAnyPktIPV6L3SingleWide = {
    QUAL_L4_PKTs, QUAL_SRC_IP6_HIGHs, QUAL_DST_IP6_HIGHs, QUAL_TOSs,
    QUAL_INNER_VLAN_IDs, QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs,
    QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs,
    QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_EGR_NHOP_CLASS_IDs,
    QUAL_EGR_L3_INTF_CLASS_IDs, QUAL_OUTPORTs, QUAL_IP_TYPEs,
    QUAL_FWD_VLAN_IDs, QUAL_VRFs, QUAL_VPNs,
    QUAL_FWD_TYPEs, QUAL_INT_CNs,
  };
  std::sort(PortAnyPktIPV6L3SingleWide.begin(), PortAnyPktIPV6L3SingleWide.end());

  // L3_DOUBLE_WIDE, PORT_ANY_PACKET_IPV6
  std::vector<std::string> PortAnyPktIPV6L3DoubleWide = {
    QUAL_L4_PKTs, QUAL_SRC_IP6_HIGHs, QUAL_DST_IP6_HIGHs,
    QUAL_TOSs, QUAL_INNER_VLAN_IDs, QUAL_INPORTs,
    QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs,
    QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs,
    QUAL_VLAN_OUTER_PRESENTs, QUAL_EGR_DVP_CLASS_IDs,
    QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_OUTPORTs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs,
    QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_IP_FLAGS_MFs,
    QUAL_TCP_CONTROL_FLAGSs, QUAL_L4DST_PORTs, QUAL_L4SRC_PORTs,
    QUAL_ICMP_TYPE_CODEs, QUAL_TTLs, QUAL_IP_FIRST_EH_SUBCODEs,
    QUAL_IP_FIRST_EH_PROTOs, QUAL_DROP_PKTs,
  };
  std::sort(PortAnyPktIPV6L3DoubleWide.begin(), PortAnyPktIPV6L3DoubleWide.end());

  // L3_ALT_DOUBLE_WIDE, PORT_ANY_PACKET_IPV6
  std::vector<std::string> PortAnyPktIPV6L3AltDoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs,
    QUAL_OUTPORTs, QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs,
    QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs,
    QUAL_IP_PROTOCOLs, QUAL_SRC_IP6_HIGHs, QUAL_DST_IP6_HIGHs, QUAL_TOSs,
  };
  std::sort(PortAnyPktIPV6L3AltDoubleWide.begin(), PortAnyPktIPV6L3AltDoubleWide.end());

  // L3_ANY_DOUBLE_WIDE, PORT_HIGIG_PACKET_ANY
  std::vector<std::string> PortHigigPktAnyL3AnyDoubleWide = {
    QUAL_INPORTs, QUAL_OUTPORTs, QUAL_EGR_NHOP_CLASS_IDs,
    QUAL_EGR_L3_INTF_CLASS_IDs, QUAL_EGR_DVP_CLASS_IDs,
    QUAL_INT_CNs, QUAL_DROP_PKTs, QUAL_L4_PKTs,
    QUAL_DST_VPs, QUAL_DST_VP_VALIDs, QUAL_INT_PRIs,
    QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs,
    QUAL_INNER_VLAN_IDs, QUAL_L3_ROUTABLE_PKTs,
    QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs,
    QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs,
    QUAL_VLAN_OUTER_PRESENTs, QUAL_CPU_COSs, QUAL_IP_TYPEs,
    QUAL_FWD_VLAN_IDs, QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs,
  };
  std::sort(PortHigigPktAnyL3AnyDoubleWide.begin(), PortHigigPktAnyL3AnyDoubleWide.end());

  // L3_ANY_DOUBLE_WIDE, PORT_FRONT_PACKET_ANY
  std::vector<std::string> PortFrontPktAnyL3AnyDoubleWide = {
    QUAL_L4_PKTs, QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_INT_PRIs, QUAL_COLORs, QUAL_L2_FORMATs, QUAL_ETHERTYPEs,
    QUAL_SRC_MACs, QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs,
    QUAL_INNER_VLAN_CFIs, QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs,
    QUAL_INPORTs, QUAL_L3_ROUTABLE_PKTs, QUAL_MIRR_COPYs,
    QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs, QUAL_OUTER_VLAN_PRIs,
    QUAL_VLAN_INNER_PRESENTs, QUAL_VLAN_OUTER_PRESENTs,
    QUAL_OUTPORTs, QUAL_CPU_COSs, QUAL_IP_TYPEs, QUAL_FWD_VLAN_IDs,
    QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs, QUAL_INT_CNs,
    QUAL_DROP_PKTs, QUAL_IP_FLAGS_MFs, QUAL_TCP_CONTROL_FLAGSs,
    QUAL_L4DST_PORTs, QUAL_L4SRC_PORTs, QUAL_ICMP_TYPE_CODEs,
    QUAL_TTLs, QUAL_IP_PROTOCOLs, QUAL_DST_IP4s,
    QUAL_SRC_IP4s, QUAL_TOSs, QUAL_DROP_PKTs,
  };
  std::sort(PortFrontPktAnyL3AnyDoubleWide.begin(), PortFrontPktAnyL3AnyDoubleWide.end());

  // L3_ANY_DOUBLE_WIDE, PORT_LOOPBACK_PACKET_ANY
  std::vector<std::string> PortLbkPktAnyL3AnyDoubleWide = {
    QUAL_LOOPBACK_QUEUEs, QUAL_LOOPBACK_TYPEs, QUAL_PKT_IS_VISIBLEs,
    QUAL_LOOPBACK_CPU_MSQRD_PKT_PROFs, QUAL_LOOPBACK_COLORs,
    QUAL_LOOPBACK_TRAFFIC_CLASSs, QUAL_LOOPBACK_PKT_PROCESSING_PORTs,
    QUAL_INPORTs, QUAL_OUTPORTs, QUAL_INT_PRIs, QUAL_COLORs,
    QUAL_EGR_NHOP_CLASS_IDs, QUAL_EGR_L3_INTF_CLASS_IDs,
    QUAL_EGR_DVP_CLASS_IDs, QUAL_INT_CNs, QUAL_DROP_PKTs,
    QUAL_L4_PKTs, QUAL_DST_VPs, QUAL_DST_VP_VALIDs,
    QUAL_L2_FORMATs, QUAL_ETHERTYPEs, QUAL_SRC_MACs,
    QUAL_DST_MACs, QUAL_VXLT_LOOKUP_HITs, QUAL_INNER_VLAN_CFIs,
    QUAL_INNER_VLAN_PRIs, QUAL_INNER_VLAN_IDs, QUAL_L3_ROUTABLE_PKTs,
    QUAL_MIRR_COPYs, QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_CFIs,
    QUAL_OUTER_VLAN_PRIs, QUAL_VLAN_INNER_PRESENTs,
    QUAL_VLAN_OUTER_PRESENTs, QUAL_CPU_COSs, QUAL_IP_TYPEs,
    QUAL_FWD_VLAN_IDs, QUAL_VRFs, QUAL_VPNs, QUAL_FWD_TYPEs,
  };
  std::sort(PortLbkPktAnyL3AnyDoubleWide.begin(), PortLbkPktAnyL3AnyDoubleWide.end());
  std::sort(qualifiers.begin(), qualifiers.end());

  // Do not alter the order
  std::vector<std::pair<std::string, std::string>> possible_combination;

  if (std::includes(PortAnyPktAnyL2SingleWide.begin(), PortAnyPktAnyL2SingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L2_SINGLE_WIDEs,PORT_ANY_PACKET_ANYs));
  }

  if (std::includes(PortAnyPktIPV4L3SingleWide.begin(), PortAnyPktIPV4L3SingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_SINGLE_WIDEs, PORT_ANY_PACKET_IPV4s));
  }

  if (std::includes(PortAnyPktIPV6L3SingleWide.begin(), PortAnyPktIPV6L3SingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_SINGLE_WIDEs, PORT_ANY_PACKET_IPV6s));
  }

  if (std::includes(PortAnyPktNonIPL3SingleWide.begin(), PortAnyPktNonIPL3SingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_SINGLE_WIDEs, PORT_ANY_PACKET_NONIPs));
  }

  if (std::includes(PortAnyPktIPV4L3DoubleWide.begin(), PortAnyPktIPV4L3DoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_DOUBLE_WIDEs, PORT_ANY_PACKET_IPV4s));
  }

  if (std::includes(PortAnyPktIPV6L3DoubleWide.begin(), PortAnyPktIPV6L3DoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_DOUBLE_WIDEs, PORT_ANY_PACKET_IPV6s));
  }

  if (std::includes(PortAnyPktNonIPL3DoubleWide.begin(), PortAnyPktNonIPL3DoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_DOUBLE_WIDEs, PORT_ANY_PACKET_NONIPs));
  }

  if (std::includes(PortAnyPktIPL3AnySingleWide.begin(), PortAnyPktIPL3AnySingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ANY_SINGLE_WIDEs, PORT_ANY_PACKET_IPs));
  }

  if (std::includes(PortAnyPktNonIPL3AnySingleWide.begin(), PortAnyPktNonIPL3AnySingleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ANY_SINGLE_WIDEs, PORT_ANY_PACKET_NONIPs));
  }

  if (std::includes(PortHigigPktAnyL3AnyDoubleWide.begin(), PortHigigPktAnyL3AnyDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ANY_DOUBLE_WIDEs, PORT_HIGIG_PACKET_ANYs));
  }

  if (std::includes(PortFrontPktAnyL3AnyDoubleWide.begin(), PortFrontPktAnyL3AnyDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ANY_DOUBLE_WIDEs, PORT_LOOPBACK_PACKET_ANYs));
  }

  if (std::includes(PortLbkPktAnyL3AnyDoubleWide.begin(), PortLbkPktAnyL3AnyDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ANY_DOUBLE_WIDEs, PORT_FRONT_PACKET_ANYs));
  }

  if (std::includes(PortAnyPktIPV4L3AltDoubleWide.begin(), PortAnyPktIPV4L3AltDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ALT_DOUBLE_WIDEs, PORT_ANY_PACKET_IPV4s));
  }

  if (std::includes(PortAnyPktNonIPL3AltDoubleWide.begin(), PortAnyPktNonIPL3AltDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ALT_DOUBLE_WIDEs, PORT_ANY_PACKET_IPV6s));
  }

  if (std::includes(PortAnyPktIPV6L3AltDoubleWide.begin(), PortAnyPktIPV6L3AltDoubleWide.end(),
                    qualifiers.begin(), qualifiers.end())) {
     possible_combination.push_back(std::make_pair(L3_ALT_DOUBLE_WIDEs, PORT_ANY_PACKET_NONIPs));
  }

  return possible_combination;
}

std::pair<std::string, std::string>
HalAclFieldToBcmRule(BcmAclStage stage, BcmField::Type field) {
  // EFP specific field mappings.
  static auto* efp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::pair<std::string, std::string>,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, {QUAL_INPORTs, QUAL_INPORT_MASKs}},
          {BcmField::OUT_PORT, {QUAL_OUTPORTs, QUAL_OUTPORT_MASKs}},
          {BcmField::ETH_TYPE, {QUAL_ETHERTYPEs, QUAL_ETHERTYPE_MASKs}},
          {BcmField::IP_TYPE, {QUAL_L3_TYPEs, ""}},
          {BcmField::ETH_SRC, {QUAL_SRC_MACs, QUAL_SRC_MAC_MASKs}},
          {BcmField::ETH_DST, {QUAL_DST_MACs, QUAL_DST_MAC_MASKs}},
          {BcmField::VLAN_VID, {QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_ID_MASKs}},
          {BcmField::VLAN_PCP, {QUAL_OUTER_VLAN_PRIs, QUAL_OUTER_VLAN_PRI_MASKs}},
          {BcmField::IPV4_SRC, {QUAL_SRC_IP4s, QUAL_SRC_IP4_MASKs}},
          {BcmField::IPV4_DST, {QUAL_DST_IP4s, QUAL_DST_IP4_MASKs}},
          // TODO(BRCM): for the below fields, 4 rules
          // need to be configured, however this map returns only
          // 2 rules, need to address this
          // QUAL_SRC_IP6_UPPER, QUAL_SRC_IP6_MASK_UPPER
          // QUAL_SRC_IP6_LOWER, QUAL_SRC_IP6_MASK_LOWER
          //{BcmField::IPV6_SRC, ??},

          // QUAL_DST_IP6_UPPER, QUAL_DST_IP6_MASK_UPPER
          // QUAL_DST_IP6_LOWER, QUAL_DST_IP6_MASK_LOWER
          //{BcmField::IPV6_DST, ??},
          {BcmField::IPV6_SRC_UPPER_64, {QUAL_SRC_IP6_HIGHs, QUAL_SRC_IP6_HIGH_MASKs}},
          {BcmField::IPV6_DST_UPPER_64, {QUAL_DST_IP6_HIGHs, QUAL_DST_IP6_HIGH_MASKs}},
          {BcmField::VRF, {QUAL_VRFs, QUAL_VRF_MASKs}},
          {BcmField::IP_DSCP_TRAF_CLASS, {QUAL_TOSs, QUAL_TOS_MASKs}},
          {BcmField::IP_TTL_HOP_LIMIT, {QUAL_TTLs, QUAL_TTL_MASKs}},
          {BcmField::IP_PROTO_NEXT_HDR, {QUAL_IP_PROTOCOLs, QUAL_IP_PROTOCOL_MASKs}},
          {BcmField::L4_SRC, {QUAL_L4SRC_PORTs, QUAL_L4SRC_PORT_MASKs}},
          {BcmField::L4_DST, {QUAL_L4DST_PORTs, QUAL_L4DST_PORT_MASKs}},
          {BcmField::TCP_FLAGS, {QUAL_TCP_CONTROL_FLAGSs, QUAL_TCP_CONTROL_FLAGS_MASKs}},
          {BcmField::ICMP_TYPE_CODE, {QUAL_ICMP_TYPE_CODEs, QUAL_ICMP_TYPE_CODE_MASKs}},
      });

  // IFP specific field mappings.
  static auto* ifp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::pair<std::string, std::string>,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, {QUAL_INPORTs, QUAL_INPORT_MASKs}},
          {BcmField::IN_PORT_BITMAP, {QUAL_INPORTSs, QUAL_INPORTS_MASKs}},
          {BcmField::OUT_PORT, {QUAL_DST_PORTs, QUAL_DST_PORT_MASKs}},
          {BcmField::ETH_TYPE, {QUAL_ETHERTYPEs, QUAL_ETHERTYPE_MASKs}},
          {BcmField::IP_TYPE, {QUAL_L3_TYPEs, ""}},
          {BcmField::ETH_SRC, {QUAL_SRC_MACs, QUAL_SRC_MAC_MASKs}},
          {BcmField::ETH_DST, {QUAL_DST_MACs, QUAL_DST_MAC_MASKs}},
          {BcmField::VLAN_VID, {QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_ID_MASKs}},
          {BcmField::VLAN_PCP, {QUAL_OUTER_VLAN_PRIs, QUAL_OUTER_VLAN_PRI_MASKs}},
          {BcmField::IPV4_SRC, {QUAL_SRC_IP4s, QUAL_DST_IP4_MASKs}},
          {BcmField::IPV4_DST, {QUAL_DST_IP4s, QUAL_DST_IP4_MASKs}},
          // TODO(BRCM): for the below fields, 4 rules
          // need to be configured, however this map returns only
          // 2 rules, need to address this
          // QUAL_DST_IP6_BITMAP_UPPER, QUAL_DST_IP6_BITMAP_LOWER
          // QUAL_DST_IP6_UPPER, QUAL_DST_IP6_MASK_UPPER
          // QUAL_DST_IP6_LOWER, QUAL_DST_IP6_MASK_LOWER
          //{BcmField::IPV6_SRC, ??},

          // QUAL_DST_IP6_BITMAP_UPPER, QUAL_DST_IP6_BITMAP_LOWER
          // QUAL_DST_IP6_UPPER, QUAL_DST_IP6_MASK_UPPER
          // QUAL_DST_IP6_LOWER, QUAL_DST_IP6_MASK_LOWER
          //{BcmField::IPV6_DST, ??},
          {BcmField::IPV6_SRC_UPPER_64, {QUAL_SRC_IP6_UPPERs, QUAL_SRC_IP6_MASK_UPPERs}},
          {BcmField::IPV6_DST_UPPER_64, {QUAL_DST_IP6_UPPERs, QUAL_DST_IP6_MASK_UPPERs}},
          {BcmField::VRF, {QUAL_VRFs, QUAL_VRF_MASKs}},
          {BcmField::IP_DSCP_TRAF_CLASS, {QUAL_TOSs, QUAL_TOS_MASKs}},
          {BcmField::IP_TTL_HOP_LIMIT, {QUAL_TTLs, QUAL_TTL_MASKs}},
          {BcmField::IP_PROTO_NEXT_HDR, {QUAL_IP_PROTOCOLs, QUAL_IP_PROTOCOL_MASKs}},
          {BcmField::L4_SRC, {QUAL_L4SRC_PORTs, QUAL_L4SRC_PORT_MASKs}},
          {BcmField::L4_DST, {QUAL_L4DST_PORTs, QUAL_L4DST_PORT_MASKs}},
          {BcmField::TCP_FLAGS, {QUAL_TCP_CONTROL_FLAGSs, QUAL_TCP_CONTROL_FLAGS_MASKs}},
          {BcmField::ICMP_TYPE_CODE, {QUAL_L4DST_PORTs, QUAL_L4DST_PORT_MASKs}},
          {BcmField::VFP_DST_CLASS_ID, {QUAL_FP_VLAN_CLASS0s, QUAL_FP_VLAN_CLASS0_MASKs}},
          {BcmField::L3_DST_CLASS_ID, {QUAL_L3DST_CLASSs, QUAL_L3DST_CLASS_MASKs}},
      });

  // VFP specific field mappings.
  static auto* vfp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::pair<std::string, std::string>,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, {QUAL_INPORTs, QUAL_INPORT_MASKs}},
          {BcmField::ETH_TYPE, {QUAL_ETHERTYPEs, QUAL_ETHERTYPE_MASKs}},
          {BcmField::IP_TYPE, {QUAL_IP_TYPEs, ""}},
          {BcmField::ETH_SRC, {QUAL_SRC_MACs, QUAL_SRC_MAC_MASKs}},
          {BcmField::ETH_DST, {QUAL_DST_MACs, QUAL_DST_MAC_MASKs}},
          {BcmField::VLAN_VID, {QUAL_OUTER_VLAN_IDs, QUAL_OUTER_VLAN_ID_MASKs}},
          {BcmField::VLAN_PCP, {QUAL_OUTER_VLAN_PRIs, QUAL_OUTER_VLAN_PRI_MASKs}},
          {BcmField::IPV4_SRC, {QUAL_SRC_IP4s, QUAL_SRC_IP4_MASKs}},
          {BcmField::IPV4_DST, {QUAL_DST_IP4s, QUAL_DST_IP4_MASKs}},
          // TODO(BRCM): for the below fields, 4 rules
          // need to be configured, however this map returns only
          // 2 rules, need to address this
          // QUAL_SRC_IP6_UPPER, QUAL_SRC_IP6_MASK_UPPER
          // QUAL_SRC_IP6_LOWER, QUAL_SRC_IP6_MASK_LOWER
          //{BcmField::IPV6_SRC, ??},

          // QUAL_DST_IP6_UPPER, QUAL_DST_IP6_MASK_UPPER
          // QUAL_DST_IP6_LOWER, QUAL_DST_IP6_MASK_LOWER
          //{BcmField::IPV6_DST, ??},
          {BcmField::IPV6_SRC_UPPER_64, {QUAL_SRC_IP6_HIGHs, QUAL_SRC_IP6_HIGH_MASKs}},
          {BcmField::IPV6_DST_UPPER_64, {QUAL_DST_IP6_HIGHs, QUAL_DST_IP6_HIGH_MASKs}},
          {BcmField::IP_DSCP_TRAF_CLASS, {QUAL_TOSs, QUAL_TOS_MASKs}},
          {BcmField::IP_TTL_HOP_LIMIT, {QUAL_TTLs, QUAL_TTL_MASKs}},
          {BcmField::IP_PROTO_NEXT_HDR, {QUAL_IP_PROTOCOLs, QUAL_IP_PROTOCOL_MASKs}},
          {BcmField::L4_SRC, {QUAL_L4SRC_PORTs, QUAL_L4SRC_PORT_MASKs}},
          {BcmField::L4_DST, {QUAL_L4DST_PORTs, QUAL_L4DST_PORT_MASKs}},
          {BcmField::TCP_FLAGS, {QUAL_TCP_CONTROL_FLAGSs, QUAL_TCP_CONTROL_FLAGS_MASKs}},
          {BcmField::ICMP_TYPE_CODE, {QUAL_ICMP_TYPE_CODEs, QUAL_ICMP_TYPE_CODE_MASKs}},
      });

  auto* stage_map = efp_field_map;
  if (stage == BCM_ACL_STAGE_EFP) {
     stage_map = efp_field_map;
  } else if (stage == BCM_ACL_STAGE_IFP) {
     stage_map = ifp_field_map;
  } else if (stage == BCM_ACL_STAGE_VFP) {
     stage_map = vfp_field_map;
  } else {
     stage_map = nullptr;
  }
  std::string unknown_qual = BcmField_Type_Name(BcmField::UNKNOWN);
  std::pair<std::string, std::string> rule = std::make_pair(unknown_qual, unknown_qual);
  if (stage_map) {
    rule = gtl::FindWithDefault(*stage_map, field, rule);
  }
  return rule;
}

// Returns the BCM type for given field or else enum count.
std::string HalAclFieldToBcm(BcmAclStage stage, BcmField::Type field) {
  // EFP specific field mappings.
  static auto* efp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::string,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, QUAL_INPORTs},
          {BcmField::OUT_PORT, QUAL_OUTPORTs},
          {BcmField::ETH_TYPE, QUAL_ETHERTYPEs},
          {BcmField::IP_TYPE, QUAL_IP_TYPEs},
          {BcmField::ETH_SRC, QUAL_SRC_MACs},
          {BcmField::ETH_DST, QUAL_DST_MACs},
          {BcmField::VLAN_VID, QUAL_OUTER_VLAN_IDs},
          {BcmField::VLAN_PCP, QUAL_OUTER_VLAN_PRIs},
          {BcmField::IPV4_SRC, QUAL_SRC_IP4s},
          {BcmField::IPV4_DST, QUAL_DST_IP4s},
          {BcmField::IPV6_SRC, QUAL_SRC_IP6s},
          {BcmField::IPV6_DST, QUAL_DST_IP6s},
          {BcmField::IPV6_SRC_UPPER_64, QUAL_SRC_IP6_HIGHs},
          {BcmField::IPV6_DST_UPPER_64, QUAL_DST_IP6_HIGHs},
          {BcmField::VRF, QUAL_VRFs},
          {BcmField::IP_DSCP_TRAF_CLASS, QUAL_TOSs},
          {BcmField::IP_TTL_HOP_LIMIT, QUAL_TTLs},
          {BcmField::IP_PROTO_NEXT_HDR, QUAL_IP_PROTOCOLs},
          {BcmField::L4_SRC, QUAL_L4SRC_PORTs},
          {BcmField::L4_DST, QUAL_L4DST_PORTs},
          {BcmField::TCP_FLAGS, QUAL_TCP_CONTROL_FLAGSs},
          {BcmField::ICMP_TYPE_CODE, QUAL_ICMP_TYPE_CODEs},
      });

  // IFP specific field mappings.
  static auto* ifp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::string,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, QUAL_INPORT_BITMAPs}, // Single port bitmap
          {BcmField::OUT_PORT, QUAL_DST_PORT_BITMAPs},
          {BcmField::ETH_TYPE, QUAL_ETHERTYPE_BITMAPs},
          {BcmField::IP_TYPE, QUAL_L3_TYPE_BITMAPs},
          {BcmField::ETH_SRC, QUAL_SRC_MAC_BITMAPs},
          {BcmField::ETH_DST, QUAL_DST_MAC_BITMAPs},
          {BcmField::VLAN_VID, QUAL_OUTER_VLAN_ID_BITMAPs},
          {BcmField::VLAN_PCP, QUAL_OUTER_VLAN_PRI_BITMAPs},
          {BcmField::IPV4_SRC, QUAL_SRC_IP4_BITMAPs},
          {BcmField::IPV4_DST, QUAL_DST_IP4_BITMAPs},
          // TODO(BRCM): for the below fields, 2 qualifiers
          // need to be configured, however this map returns only
          // one qualifier, need to address this
          //{BcmField::IPV6_SRC, ??}, // QUAL_DST_IP6_BITMAP_UPPER, QUAL_DST_IP6_BITMAP_LOWER
          //{BcmField::IPV6_DST, ??}, // QUAL_DST_IP6_BITMAP_UPPER, QUAL_DST_IP6_BITMAP_LOWER
          {BcmField::IPV6_SRC_UPPER_64, QUAL_SRC_IP6_BITMAP_UPPERs},
          {BcmField::IPV6_DST_UPPER_64, QUAL_DST_IP6_BITMAP_UPPERs},
          {BcmField::VRF, QUAL_VRF_BITMAPs},
          {BcmField::IP_DSCP_TRAF_CLASS, QUAL_TOS_BITMAPs},
          {BcmField::IP_TTL_HOP_LIMIT, QUAL_TTL_BITMAPs},
          {BcmField::IP_PROTO_NEXT_HDR, QUAL_IP_PROTOCOL_BITMAPs},
          {BcmField::L4_SRC, QUAL_L4SRC_PORT_BITMAPs},
          {BcmField::L4_DST, QUAL_L4DST_PORT_BITMAPs},
          {BcmField::TCP_FLAGS, QUAL_TCP_CONTROL_FLAGS_BITMAPs},
          {BcmField::ICMP_TYPE_CODE, QUAL_L4DST_PORT_BITMAPs},
          {BcmField::VFP_DST_CLASS_ID, QUAL_FP_VLAN_CLASS0_BITMAPs},
          {BcmField::L3_DST_CLASS_ID, QUAL_L3DST_CLASS_BITMAPs},
      });

  // VFP specific field mappings.
  static auto* vfp_field_map =
      new absl::flat_hash_map<BcmField::Type, std::string,
                              EnumHash<BcmField::Type>>({
          {BcmField::IN_PORT, QUAL_INPORTs},
          {BcmField::ETH_TYPE, QUAL_ETHERTYPEs},
          {BcmField::IP_TYPE, QUAL_IP_TYPEs},
          {BcmField::ETH_SRC, QUAL_SRC_MACs},
          {BcmField::ETH_DST, QUAL_DST_MACs},
          {BcmField::VLAN_VID, QUAL_OUTER_VLAN_IDs},
          {BcmField::VLAN_PCP, QUAL_OUTER_VLAN_PRIs},
          {BcmField::IPV4_SRC, QUAL_SRC_IP4s},
          {BcmField::IPV4_DST, QUAL_DST_IP4s},
          {BcmField::IPV6_SRC, QUAL_SRC_IP6s},
          {BcmField::IPV6_DST, QUAL_DST_IP6s},
          {BcmField::IPV6_SRC_UPPER_64, QUAL_SRC_IP6_HIGHs},
          {BcmField::IPV6_DST_UPPER_64, QUAL_DST_IP6_HIGHs},
          {BcmField::IP_DSCP_TRAF_CLASS, QUAL_TOSs},
          {BcmField::IP_TTL_HOP_LIMIT, QUAL_TTLs},
          {BcmField::IP_PROTO_NEXT_HDR, QUAL_IP_PROTOCOLs},
          {BcmField::L4_SRC, QUAL_L4SRC_PORTs},
          {BcmField::L4_DST, QUAL_L4DST_PORTs},
          {BcmField::TCP_FLAGS, QUAL_TCP_CONTROL_FLAGSs},
          {BcmField::ICMP_TYPE_CODE, QUAL_ICMP_TYPE_CODEs},
      });

  auto *stage_map = efp_field_map;
  if (stage == BCM_ACL_STAGE_EFP) {
    stage_map = efp_field_map;
  } else if (stage == BCM_ACL_STAGE_IFP) {
    stage_map = ifp_field_map;
  } else if (stage == BCM_ACL_STAGE_VFP) {
    stage_map = vfp_field_map;
  } else {
    return BcmField_Type_Name(BcmField::UNKNOWN);
  }
  return gtl::FindWithDefault(*stage_map, field, BcmField_Type_Name(BcmField::UNKNOWN));
}

::util::StatusOr<int> GetUniqueId(std::map<std::pair<BcmAclStage, int>, int> *table_ids,
                                  int id, int max) {
  int acl_id = 0;
  std::set<int> setOfNumbers = extract_values(*table_ids);
  int totalEntries = static_cast<int>(setOfNumbers.size());
  if (totalEntries == max) {
     return MAKE_ERROR(ERR_INTERNAL) << "ACL table Full.";
  }
  if (!setOfNumbers.empty()) {
    acl_id = *setOfNumbers.rbegin() + 1;
  }
  if (id != -1) {
    //make sure the table id is not present
    if (setOfNumbers.count(id) != 0) {
       return MAKE_ERROR(ERR_INTERNAL) << "Entry with table id " << id << " already exists.";
    } else {
      acl_id = id;
    }
  }
  return acl_id;
}

::util::Status CreateVfpGroup(int unit, int stage_id,
                              const BcmAclTable &table) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_VLAN_GRP_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_GRP_TEMPLATE_IDs, stage_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODE_AUTOs, 1));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, table.priority()));
  for (const auto &field : table.fields()) {
    if (field.udf_chunk_id()) {
      uint32_t index = static_cast<uint32>(field.udf_chunk_id() - 1);
      uint64_t value = static_cast<uint64>(0xffff);
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKSs,
                                      index, &value, 1));
      continue;
    }
    std::string bcm_qual_field = HalAclFieldToBcm(table.stage(), field.type());
    std::string unknown_qual = BcmField_Type_Name(BcmField::UNKNOWN);
    if ((unknown_qual.compare(bcm_qual_field)) == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to create ACL table with invalid predefined "
             << " qualifier: " << field.ShortDebugString() << ".";
    }
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, bcm_qual_field.c_str(), 1));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateIfpGroup(int unit, int stage_id,
                              const BcmAclTable &table) {
  uint64_t max;
  uint64_t min;
  bcmlt_entry_handle_t entry_hdl;

  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_ING_GRP_TEMPLATEs, &entry_hdl));
  auto _ = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_GRP_TEMPLATE_IDs, stage_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, MODE_AUTOs, 1));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, table.priority()));
  for (const auto &field : table.fields()) {
    if (field.udf_chunk_id()) {
      uint32_t index = static_cast<uint32>(field.udf_chunk_id() - 1);
      uint64_t value = static_cast<uint64>(0xffff);
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKS_BITMAPs,
                                      index, &value, 1));
      continue;
    }
    std::string bcm_qual_field = HalAclFieldToBcm(table.stage(), field.type());
    std::string unknown_qual = BcmField_Type_Name(BcmField::UNKNOWN);
    if (bcm_qual_field == BcmField_Type_Name(BcmField::UNKNOWN)) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to create ACL table with invalid predefined "
             << "qualifier: " << field.ShortDebugString() << ".";
    }
    RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, FP_ING_GRP_TEMPLATEs,
                                            bcm_qual_field.c_str(),
                                            &min, &max));
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, bcm_qual_field.c_str(), max));
  }
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                                BCMLT_PRIORITY_NORMAL));
  return ::util::OkStatus();
}

::util::Status CreateEfpGroup(int unit, int stage_id,
                              const BcmAclTable &table) {

  std::vector<std::string> efp_qualifiers;
  bcmlt_entry_handle_t entry_hdl;

  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_EGR_GRP_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_EGR_GRP_TEMPLATE_IDs, stage_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, table.priority()));
  for (const auto &field : table.fields()) {
    if (field.udf_chunk_id()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "UDF is not valid in "
             << BcmAclStage_Name(BCM_ACL_STAGE_EFP) << ".";
    }
    std::string bcm_qual_field = HalAclFieldToBcm(table.stage(), field.type());
    std::string unknown_qual = BcmField_Type_Name(BcmField::UNKNOWN);
    if ((unknown_qual.compare(bcm_qual_field)) == 0) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to create ACL table with invalid predefined "
             << "qualifier: " << field.ShortDebugString() << ".";
    }
    efp_qualifiers.emplace_back(bcm_qual_field.c_str());
    RETURN_IF_BCM_ERROR(
        bcmlt_entry_field_add(entry_hdl, bcm_qual_field.c_str(), 1));
  }
  std::vector<std::pair<std::string, std::string>> possibles =
           GetPktTypeAndMode(efp_qualifiers);
  if (possibles.size() > 0) {
    for (const auto &p : possibles) {
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_symbol_add(entry_hdl, PORT_PKT_TYPEs, p.second.c_str()));
      RETURN_IF_BCM_ERROR(
          bcmlt_entry_field_symbol_add(entry_hdl, MODEs, p.first.c_str()));
      int rv = bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                         BCMLT_PRIORITY_NORMAL);
      if (rv == SHR_E_NONE) {
        break;
      }
    }
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to create ACL Table in "
           << BcmAclStage_Name(BCM_ACL_STAGE_EFP) << ".";
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status
CreateAclGroup(int unit, int id, BcmAclStage stage, const BcmAclTable &table) {
  if (stage == BCM_ACL_STAGE_VFP) {
    RETURN_IF_ERROR(CreateVfpGroup(unit, id, table));
  } else if (stage == BCM_ACL_STAGE_IFP) {
    RETURN_IF_ERROR(CreateIfpGroup(unit, id, table));
  } else if (stage == BCM_ACL_STAGE_EFP) {
    RETURN_IF_ERROR(CreateEfpGroup(unit, id, table));
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempted to create ACL table with invalid pipeline stage: "
           << BcmAclStage_Name(stage) << ".";
  }
  return ::util::OkStatus();
}

}  // namespace

::util::StatusOr<int> BcmSdkWrapper::CreateAclTable(int unit,
                                                    const BcmAclTable& table) {
  int stage_id;
  int table_id;
  InUseMap *group_ids;

  // check if unit exist
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  BcmAclStage stage = table.stage();
  switch (stage) {
    case BCM_ACL_STAGE_VFP:
      group_ids = gtl::FindOrNull(vfp_group_ids_, unit);
      break;
    case BCM_ACL_STAGE_IFP:
      group_ids = gtl::FindOrNull(ifp_group_ids_, unit);
      break;
    case BCM_ACL_STAGE_EFP:
      group_ids = gtl::FindOrNull(efp_group_ids_, unit);
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to create ACL table with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }
  AclGroupIds* table_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  auto it = unit_to_fp_groups_max_limit_.find(unit);
  CHECK_RETURN_IF_FALSE(group_ids != nullptr &&
                        table_ids != nullptr &&
                        it != unit_to_fp_groups_max_limit_.end())
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  int maxEntries = it->second;
  int requested_table_id = (table.id()) ? table.id() : -1;
  ASSIGN_OR_RETURN(table_id, GetUniqueId(table_ids, requested_table_id, maxEntries));
  // get next free slot
  std::string errMsg = absl::StrCat(BcmAclStage_Name(stage), " table is full.");
  ASSIGN_OR_RETURN(stage_id, GetFreeSlot(group_ids, errMsg));
  RETURN_IF_ERROR(CreateAclGroup(unit, stage_id, stage, table));
  // update map
  ConsumeSlot(group_ids, stage_id);
  table_ids->emplace(std::make_pair(stage, stage_id), table_id);
  return table_id;
}

::util::Status BcmSdkWrapper::DestroyAclTable(int unit, int table_id) {
  bool found;
  int rv;
  std::map<int, bool>* group_ids;
  std::pair<BcmAclStage, int> entry;
  bcmlt_entry_handle_t entry_hdl;
  bool entry_deleted = false;

  // check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  auto* table_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  CHECK_RETURN_IF_FALSE(table_ids != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";

  ASSIGN_OR_RETURN(found, FindAndReturnEntry(table_ids, table_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "ACL Table id " << table_id << " not found.";
  }
  BcmAclStage stage = entry.first;
  int stage_id = entry.second;
  if (stage == BCM_ACL_STAGE_VFP) {
     group_ids = gtl::FindOrNull(vfp_group_ids_, unit);
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_GRP_TEMPLATE_IDs, stage_id));
  } else if (stage == BCM_ACL_STAGE_IFP) {
     group_ids = gtl::FindOrNull(ifp_group_ids_, unit);
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_GRP_TEMPLATE_IDs, stage_id));
  } else if (stage == BCM_ACL_STAGE_EFP) {
     group_ids = gtl::FindOrNull(efp_group_ids_, unit);
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_GRP_TEMPLATE_IDs, stage_id));
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "ACL table with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }
  CHECK_RETURN_IF_FALSE(group_ids != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";

  if (SlotExists(group_ids, stage_id)) {
     rv = bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL);
     if (rv == SHR_E_NONE){
        entry_deleted = true;
     }
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  if (entry_deleted) {
     ReleaseSlot(group_ids, stage_id);
     table_ids->erase(entry);
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Failed to delete ACL table with id: " << table_id << " in stage "
             << BcmAclStage_Name(stage) << ".";
  }
  return ::util::OkStatus();
}

namespace {
::util::Status AddAclQualifier(int unit, const bcmlt_entry_handle_t& entry_hdl,
                               const std::pair<std::string, std::string> field_pair,
                               const BcmAclStage& stage,
                               const BcmField& field) {
   uint64_t max_mask64;
   uint64_t min_mask64;
   uint64_t value64;
   uint32_t mask32;
   uint32_t value32;
   std::string field_name = field_pair.first;
   std::string field_mask_name = field_pair.second;

   if (field_mask_name.length() > 0) {
     if (stage == BCM_ACL_STAGE_VFP) {
       RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, FP_VLAN_RULE_TEMPLATEs, field_mask_name.c_str(), &min_mask64, &max_mask64));
     } else if (stage == BCM_ACL_STAGE_IFP) {
       RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, FP_ING_RULE_TEMPLATEs, field_mask_name.c_str(), &min_mask64, &max_mask64));
     } else if (stage == BCM_ACL_STAGE_EFP) {
       RETURN_IF_BCM_ERROR(GetFieldMinMaxValue(unit, FP_EGR_RULE_TEMPLATEs, field_mask_name.c_str(), &min_mask64, &max_mask64));
     } else {
       return MAKE_ERROR(ERR_INVALID_PARAM)
              << "Attempted to create ACL rule with invalid pipeline stage: "
              << BcmAclStage_Name(stage) << ".";
     }
   }

   switch (field.type()) {
     case BcmField::ETH_SRC:
     case BcmField::ETH_DST:
       // TODO(BRCM) check if this is a problem, otherwise use htonll
       value64 = field.value().u64();
       // TODO(BRCM) check if this is a problem, otherwise use htonll
       max_mask64 = field.has_mask() ? field.mask().u64() : max_mask64;
       RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_name.c_str(), value64));
       RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_mask_name.c_str(), max_mask64));
       break;
     case BcmField::IP_TYPE:
       {
         if (field.has_mask()) {
           return MAKE_ERROR(ERR_INVALID_PARAM)
                  << "IpType metadata qualifier contained unexpected mask entry.";
         }
         std::string ip_type = "";
         switch (field.value().u32()) {
           // The case values are EtherType values specified in IEEE 802.3. Please
           // refer to https://en.wikipedia.org/wiki/EtherType.
           case kEtherTypeIPv4:
              // TODO(max): Wrong use of StrCat, use StrAppend
             ip_type = absl::StrCat(ip_type, ANY_IP4s); //bcmFieldIpTypeIpv4Any;
             break;
           case kEtherTypeIPv6:
             ip_type = absl::StrCat(ip_type, ANY_IP6s); //bcmFieldIpTypeIpv6
             break;
           case kEtherTypeArp:
             ip_type = absl::StrCat(ip_type, ARPs); //bcmFieldIpTypeArp;
             break;
           default:
             return MAKE_ERROR(ERR_INVALID_PARAM)
                    << "IpType metadata qualifier contained unsupported EtherType "
                       "value.";
         }
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, field_name.c_str(), ip_type.c_str()));
       }
       break;
     case BcmField::IN_PORT:
     case BcmField::ETH_TYPE:
     case BcmField::OUT_PORT:
     case BcmField::VRF:
     case BcmField::VLAN_VID:
     case BcmField::VLAN_PCP:
     case BcmField::IPV4_SRC:
     case BcmField::IPV4_DST:
     case BcmField::IP_TTL_HOP_LIMIT:
     case BcmField::IP_PROTO_NEXT_HDR:
     case BcmField::L4_SRC:
     case BcmField::L4_DST:
     case BcmField::TCP_FLAGS:
     case BcmField::ICMP_TYPE_CODE:
     //case BcmField::L3_DST_CLASS_ID: Not supported
     //case BcmField::VFP_DST_CLASS_ID: Not supported
     //case BcmField::IP_DSCP_TRAF_CLASS: Not supported
       value32 = static_cast<uint32>(field.value().u32());
       RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_name.c_str(), value32));
       if (field.has_mask()) {
          mask32 = static_cast<uint32>(field.mask().u32());
          RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_mask_name.c_str(), mask32));
       } else {
          RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_mask_name.c_str(), max_mask64));
       }
       break;
     case BcmField::IPV6_SRC:
     case BcmField::IPV6_DST:
     case BcmField::IPV6_SRC_UPPER_64:
     case BcmField::IPV6_DST_UPPER_64:
       {
         std::string ipv6_str = field.value().b();
         std::string ipv6_mask_str = field.mask().b();
         value64 = ByteStreamToUint<uint64>(ipv6_str);
         max_mask64 = ByteStreamToUint<uint64>(ipv6_mask_str);
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_name.c_str(), value64));
         if (field.has_mask()) {
            memcpy(&max_mask64, field.mask().b().data(), sizeof(max_mask64));
         }
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, field_mask_name.c_str(), max_mask64));
       }
       break;
     default:
       return MAKE_ERROR()
              << "Attempted to translate unsupported BcmField::Type: "
              << BcmField::Type_Name(field.type()) << ".";
   }
   return ::util::OkStatus();
}

::util::Status CreateVfpRule(int unit, int rule_id, const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_VLAN_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_RULE_TEMPLATE_IDs, rule_id));
  for (const auto& field : flow.fields()) {
    if (field.udf_chunk_id()) {
       // TODO(BRCM): hardcoding kUdfChunkSize: 2,
       // this should be ok for Tomahawk, revisit if this is a problem
       if (field.value().b().size() != 2 ||
           (field.has_mask() && field.mask().b().size() != 2)) { // kUdfChunkSize: 2
         return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to program flow with UDF chunk "
             << field.udf_chunk_id() << " with value or mask size not equal to "
             << "chunk size " << 2 << ".";
       }
       uint32_t index = static_cast<uint32>(field.udf_chunk_id() - 1);
       std::string value_str = field.value().b();
       uint64_t value64 = ByteStreamToUint<uint64>(value_str);
       // TODO(BRCM): need to use ByteStreamToUint<uint64>(mask_str);?
       // revisit if this is a problem
       uint64_t mask64 = static_cast<uint64>(0xffff);
       if (field.has_mask()) {
          std::string mask_str = field.mask().b();
          mask64 = ByteStreamToUint<uint64>(mask_str);
       }
       RETURN_IF_BCM_ERROR(
           bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKSs, index,
                                       &value64, 1));
       RETURN_IF_BCM_ERROR(
           bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKS_MASKs, index,
                                       &mask64, 1));
       continue;
    }
    std::pair<std::string, std::string> bcm_qual_field = HalAclFieldToBcmRule(flow.acl_stage(), field.type());
    if (bcm_qual_field.first == BcmField_Type_Name(BcmField::UNKNOWN)) {
      LOG(INFO) << "Qual: '" << field.ShortDebugString()
                << "' in " << BcmAclStage_Name(BCM_ACL_STAGE_VFP) << ".";
      return MAKE_ERROR()
             << "Attempted to translate unsupported BcmField::Type: "
             << BcmField::Type_Name(field.type()) << ".";
    }
    RETURN_IF_ERROR(
        AddAclQualifier(unit, entry_hdl, bcm_qual_field,
                        flow.acl_stage(), field));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateIfpRule(int unit, int rule_id, const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_ING_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_RULE_TEMPLATE_IDs, rule_id));
  for (const auto& field : flow.fields()) {
    if (field.udf_chunk_id()) {
       // TODO(BRCM): hardcoding kUdfChunkSize: 2,
       // this should be ok for Tomahawk, revisit if this is a problem
       if (field.value().b().size() != BcmSdkWrapper::kUdfChunkSize ||
           (field.has_mask() && field.mask().b().size() != BcmSdkWrapper::kUdfChunkSize)) {
         return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to program flow with UDF chunk "
             << field.udf_chunk_id() << " with value or mask size not equal to "
             << "chunk size " << BcmSdkWrapper::kUdfChunkSize << ".";
       }
       uint32_t index = static_cast<uint32>(field.udf_chunk_id() - 1);
       std::string value_str = field.value().b();
       uint64_t value64 = ByteStreamToUint<uint64>(value_str);
       // TODO(BRCM): need to use ByteStreamToUint<uint64>(mask_str);?
       // revisit if this is a problem
       uint64_t mask64 = static_cast<uint64>(0xffff);
       if (field.has_mask()) {
          std::string mask_str = field.mask().b();
          mask64 = ByteStreamToUint<uint64>(mask_str);
       }
       RETURN_IF_BCM_ERROR(
           bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKSs, index, &value64, 1));
       RETURN_IF_BCM_ERROR(
           bcmlt_entry_field_array_add(entry_hdl, QUAL_UDF_CHUNKS_MASKs, index, &mask64, 1));
       continue;
    }
    std::pair<std::string, std::string> bcm_qual_field = HalAclFieldToBcmRule(flow.acl_stage(), field.type());
    if (bcm_qual_field.first == BcmField_Type_Name(BcmField::UNKNOWN)) {
      LOG(INFO) << "Qual: '" << field.ShortDebugString()
                << "' in " << BcmAclStage_Name(BCM_ACL_STAGE_IFP) << ".";
      return MAKE_ERROR()
             << "Attempted to translate unsupported BcmField::Type: "
             << BcmField::Type_Name(field.type()) << ".";
    }
    RETURN_IF_ERROR(
        AddAclQualifier(unit, entry_hdl, bcm_qual_field, flow.acl_stage(), field));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateEfpRule(int unit, int rule_id, const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_EGR_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_EGR_RULE_TEMPLATE_IDs, rule_id));
  for (const auto& field : flow.fields()) {
    if (field.udf_chunk_id()) {
       return MAKE_ERROR(ERR_INTERNAL) << "UDF is not valid in "
              << BcmAclStage_Name(BCM_ACL_STAGE_EFP) << ".";
    }

    std::pair<std::string, std::string> bcm_qual_field = HalAclFieldToBcmRule(flow.acl_stage(), field.type());
    if (bcm_qual_field.first == BcmField_Type_Name(BcmField::UNKNOWN)) {
      LOG(INFO) << "Qual: '" << field.ShortDebugString()
                << "' in " << BcmAclStage_Name(BCM_ACL_STAGE_IFP) << ".";
      return MAKE_ERROR()
             << "Attempted to translate unsupported BcmField::Type: "
             << BcmField::Type_Name(field.type()) << ".";
    }
    RETURN_IF_ERROR(
        AddAclQualifier(unit, entry_hdl, bcm_qual_field,
                        flow.acl_stage(), field));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

// Verify the parameters for a given BcmAction based on sets of required and
// optional parameters.
::util::Status VerifyAclActionParams(
    const BcmAction& action,
    const absl::flat_hash_set<BcmAction::Param::Type,
                              EnumHash<BcmAction::Param::Type>>& required,
    const absl::flat_hash_set<BcmAction::Param::Type,
                              EnumHash<BcmAction::Param::Type>>& optional) {
  auto req_params = required, opt_params = optional;
  // Check each parameter in action with the given set of parameters.
  for (const auto& param : action.params()) {
    if (!(req_params.erase(param.type()) || opt_params.erase(param.type()))) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
          << "Invalid or duplicate parameter for "
          << BcmAction::Type_Name(action.type()) << ": "
          << BcmAction::Param::Type_Name(param.type()) << ".";
    }
  }
  // Return error if any unmatched parameters are required.
  if (!req_params.empty()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
        << "Unmatched parameter(s) for action: " << action.ShortDebugString()
        << ".";
  }
  return ::util::OkStatus();
}

::util::Status AddAclAction(int unit, const bcmlt_entry_handle_t& entry_hdl,
                            int policy_id, const BcmAclStage& stage,
                            const BcmAction& action) {
  // Sets of required and optional action parameters.
  absl::flat_hash_set<BcmAction::Param::Type, EnumHash<BcmAction::Param::Type>>
      required;
  absl::flat_hash_set<BcmAction::Param::Type, EnumHash<BcmAction::Param::Type>>
      optional;
  switch (action.type()) {
    case BcmAction::DROP: {
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      // bcmFieldActionDrop
      if (action.params().empty()) {  // No params, just drop
         if (stage == BCM_ACL_STAGE_VFP) {
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_DROPs, 1));
         } else if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_DROPs, 1));
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_DROPs, 1));
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_DROPs, 1));
         }
         break;
      }
      switch (action.params(0).value().u32()) {
        case BcmAction::Param::GREEN:
          if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
             // bcmFieldActionGpDrop
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_DROPs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        case BcmAction::Param::YELLOW:
          // bcmFieldActionYpDrop
          if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_DROPs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        case BcmAction::Param::RED:
          // bcmFieldActionRpDrop
          if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_DROPs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                << "Invalid color parameter for DROP action: "
                << action.params(0).value().u32() << ".";
      }
    }  break;
    case BcmAction::OUTPUT_PORT: {
      required.insert(BcmAction::Param::LOGICAL_PORT);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (stage == BCM_ACL_STAGE_IFP) {
        // bcmFieldActionRedirect
        uint32 port = static_cast<uint32>(action.params(0).value().u32());
        RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_REDIRECT_TO_PORTs, port));
        RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_REDIRECT_TO_MODULEs, 0));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << " in "
               << BcmAclStage_Name(stage) << ".";
      }
    }  break;
    case BcmAction::OUTPUT_TRUNK: {
      // bcmFieldActionRedirectTrunk;
      required.insert(BcmAction::Param::TRUNK_PORT);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (stage == BCM_ACL_STAGE_IFP) {
         uint32 trunk = static_cast<uint32>(action.params(0).value().u32());
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_REDIRECT_TO_TRUNKs, trunk));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << " in "
               << BcmAclStage_Name(stage) << ".";
      }
    } break;
    case BcmAction::OUTPUT_L3: {
      // bcmFieldActionL3Switch;
      required.insert(BcmAction::Param::EGRESS_INTF_ID);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (stage == BCM_ACL_STAGE_IFP) {
         uint32 egress_intf_id = static_cast<uint32>(action.params(0).value().u32());
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_SWITCH_TO_L3UCs, egress_intf_id));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << " in "
               << BcmAclStage_Name(stage) << ".";
      }
    } break;
    case BcmAction::COPY_TO_CPU: {
      // bcmFieldActionCopyToCpu;
      required.insert(BcmAction::Param::QUEUE);
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bool colorFound = false;
      for (const auto& param : action.params()) {
        switch (param.type()) {
          case BcmAction::Param::QUEUE:
            if (stage == BCM_ACL_STAGE_VFP || stage == BCM_ACL_STAGE_IFP) {
               uint32 queue = static_cast<uint32>(param.value().u32());
               RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_NEW_CPU_COSs, queue));
            } else {
               return MAKE_ERROR(ERR_INVALID_PARAM)
                     << "Attempted to translate unsupported BcmAction::Type: "
                     << BcmAction::Type_Name(action.type()) << " in "
                     << BcmAclStage_Name(stage) << ".";
            }
            break;
          case BcmAction::Param::COLOR:
            switch (param.value().u32()) {
              case BcmAction::Param::GREEN:
                // bcmFieldActionGpCopyToCpu;
                if (stage == BCM_ACL_STAGE_IFP) {
                   colorFound = true;
                   RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_COPY_TO_CPUs, 1));
                } else {
                   return MAKE_ERROR(ERR_INVALID_PARAM)
                         << "Attempted to translate unsupported BcmAction::Type: "
                         << BcmAction::Type_Name(action.type()) << " in "
                         << BcmAclStage_Name(stage) << ".";
                }
                break;
              case BcmAction::Param::YELLOW:
                // bcmFieldActionYpCopyToCpu;
                if (stage == BCM_ACL_STAGE_IFP) {
                   colorFound = true;
                   RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_COPY_TO_CPUs, 1));
                } else {
                   return MAKE_ERROR(ERR_INVALID_PARAM)
                         << "Attempted to translate unsupported BcmAction::Type: "
                         << BcmAction::Type_Name(action.type()) << " in "
                         << BcmAclStage_Name(stage) << ".";
                }
                break;
              case BcmAction::Param::RED:
                // bcmFieldActionRpCopyToCpu;
                if (stage == BCM_ACL_STAGE_IFP) {
                   colorFound = true;
                   RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_COPY_TO_CPUs, 1));
                } else {
                   return MAKE_ERROR(ERR_INVALID_PARAM)
                         << "Attempted to translate unsupported BcmAction::Type: "
                         << BcmAction::Type_Name(action.type()) << " in "
                         << BcmAclStage_Name(stage) << ".";
                }
                break;
              default:
                return MAKE_ERROR(ERR_INVALID_PARAM)
                      << "Invalid color parameter for COPY_TO_CPU action: "
                      << param.value().u32() << ".";
            }
            break;
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                  << "Invalid parameter type for COPY_TO_CPU action: "
                  << BcmAction::Param::Type_Name(param.type()) << ".";
        }
      }
      if (!colorFound) {
         if (stage == BCM_ACL_STAGE_VFP) {
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_COPY_TO_CPUs, 1));
         } else if (stage == BCM_ACL_STAGE_IFP) {
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_COPY_TO_CPUs, 1));
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_COPY_TO_CPUs, 1));
            RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_COPY_TO_CPUs, 1));
         } else {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                  << "Attempted to translate unsupported BcmAction::Type: "
                  << BcmAction::Type_Name(action.type()) << " in "
                  << BcmAclStage_Name(stage) << ".";
         }
      }
    } break;
    case BcmAction::CANCEL_COPY_TO_CPU: {
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (action.params().empty()) {  // No params, just drop
        // bcmFieldActionCopyToCpuCancel
        if (stage == BCM_ACL_STAGE_VFP) {
           RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_COPY_TO_CPU_CANCELs, 1));
        } else if (stage == BCM_ACL_STAGE_IFP) {
           RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_COPY_TO_CPU_CANCELs, 1));
           RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_COPY_TO_CPU_CANCELs, 1));
           RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_COPY_TO_CPU_CANCELs, 1));
        } else {
            return MAKE_ERROR(ERR_INVALID_PARAM)
                  << "Attempted to translate unsupported BcmAction::Type: "
                  << BcmAction::Type_Name(action.type()) << " in "
                  << BcmAclStage_Name(stage) << ".";
        }
        break;
      }
      switch (action.params(0).value().u32()) {
        case BcmAction::Param::GREEN:
          // bcmFieldActionGpCopyToCpuCancel
          if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_G_COPY_TO_CPU_CANCELs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        case BcmAction::Param::YELLOW:
          // bcmFieldActionYpCopyToCpuCancel
          if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_Y_COPY_TO_CPU_CANCELs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        case BcmAction::Param::RED:
          // bcmFieldActionRpCopyToCpuCancel
          if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_R_COPY_TO_CPU_CANCELs, 1));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Attempted to translate unsupported BcmAction::Type: "
                   << BcmAction::Type_Name(action.type()) << " in "
                   << BcmAclStage_Name(stage) << ".";
          }
          break;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                << "Invalid color parameter for CANCEL_COPY_TO_CPU "
                << "action: " << action.params(0).value().u32() << ".";
      }
    } break;
    case BcmAction::SET_COLOR: {
      required.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      uint32 color = action.params(0).value().u32();
      // bcmFieldActionDropPrecedence
      switch (color) {
        case BcmAction::Param::GREEN:
          if (stage == BCM_ACL_STAGE_VFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_COLORs, GREENs));
          } else if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_Y_COLORs, GREENs));
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_R_COLORs, GREENs));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid color parameter for SET_COLOR action: " << color
                   << ".";
          }
          break;
        case BcmAction::Param::YELLOW:
          if (stage == BCM_ACL_STAGE_VFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_COLORs, YELLOWs));
          } else if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_R_COLORs, YELLOWs));
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_G_COLORs, YELLOWs));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid color parameter for SET_COLOR action: " << color
                   << ".";
          }
          break;
        case BcmAction::Param::RED:
          if (stage == BCM_ACL_STAGE_VFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_COLORs, REDs));
          } else if (stage == BCM_ACL_STAGE_IFP) {
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_Y_COLORs, REDs));
             RETURN_IF_BCM_ERROR(bcmlt_entry_field_symbol_add(entry_hdl, ACTION_NEW_G_COLORs, REDs));
          } else {
             return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid color parameter for SET_COLOR action: " << color
                   << ".";
          }
          break;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                << "Invalid color parameter for SET_COLOR action: " << color
                << ".";
      }
    } break;
    case BcmAction::SET_VRF: {
      required.insert(BcmAction::Param::VRF);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      // bcmFieldActionVrfSet
      uint32 vrf = action.params(0).value().u32();
      if (stage == BCM_ACL_STAGE_VFP) {
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_VRF_SETs, vrf));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << ".";
      }
    }  break;
    case BcmAction::SET_VFP_DST_CLASS_ID: {
      required.insert(BcmAction::Param::VFP_DST_CLASS_ID);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      // bcmFieldActionClassDestSet
      uint32 class_id = action.params(0).value().u32();
      if (stage == BCM_ACL_STAGE_VFP) {
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_DST_CLASS_ID_SETs, class_id));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << ".";
      }
    } break;
    case BcmAction::SET_IP_DSCP: {
      required.insert(BcmAction::Param::IP_DSCP);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      // bcmFieldActionDscpNew
      uint32 dscp = action.params(0).value().u32();
      if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_NEW_G_DSCPs, dscp));
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_NEW_Y_DSCPs, dscp));
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_NEW_R_DSCPs, dscp));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << ".";
      }
    } break;
    case BcmAction::ADD_OUTER_VLAN: {
      required.insert(BcmAction::Param::VLAN_VID);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      // bcmFieldActionOuterVlanAdd
      uint32 outer_vlan = action.params(0).value().u32();
      if (stage == BCM_ACL_STAGE_EFP) {
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_NEW_OUTER_VLANIDs, outer_vlan));
      } else if (stage == BCM_ACL_STAGE_VFP) {
         RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ACTION_ADD_OUTER_TAGs, outer_vlan));
      } else {
         return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Attempted to translate unsupported BcmAction::Type: "
               << BcmAction::Type_Name(action.type()) << ".";
      }
    } break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
            << "Attempted to translate unsupported BcmAction::Type: "
            << BcmAction::Type_Name(action.type()) << ".";
  }
  return ::util::OkStatus();
}

::util::Status CreateVfpPolicy(int unit, int policy_id,
                               const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_VLAN_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_POLICY_TEMPLATE_IDs, policy_id));
  // Translate actions and add to new flow entry.
  for (const auto& action : flow.actions()) {
    RETURN_IF_ERROR(AddAclAction(unit, entry_hdl, policy_id,
                                 flow.acl_stage(), action));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}
::util::Status CreateIfpPolicy(int unit, int policy_id,
                               const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_ING_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_POLICY_TEMPLATE_IDs, policy_id));
  // Translate actions and add to new flow entry.
  for (const auto& action : flow.actions()) {
    RETURN_IF_ERROR(AddAclAction(unit, entry_hdl, policy_id,
                                 flow.acl_stage(), action));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}
::util::Status CreateEfpPolicy(int unit, int policy_id,
                               const BcmFlowEntry& flow) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_allocate(unit, FP_EGR_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_EGR_POLICY_TEMPLATE_IDs, policy_id));
  // Translate actions and add to new flow entry.
  for (const auto& action : flow.actions()) {
    RETURN_IF_ERROR(AddAclAction(unit, entry_hdl, policy_id,
                                 flow.acl_stage(), action));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

// Create and attach policer to the specified flow with the given rate and burst
// parameters.
::util::Status AddAclPolicer(int unit, int meter_id, int stage,
                             const BcmMeterConfig& meter) {
  bcmlt_entry_handle_t entry_hdl;
  if (stage == BCM_ACL_STAGE_IFP) {
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_allocate(unit, METER_FP_ING_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, METER_FP_ING_TEMPLATE_IDs, meter_id));
  } else if (stage == BCM_ACL_STAGE_EFP) {
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_allocate(unit, METER_FP_EGR_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, meter_id));
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
            << "TODO(BRCM): Add proper error message";
  }
  // Determine whether the meter is to be configured for a single rate (two
  // colors) or for trTCM mode.
  if ((meter.committed_rate() == meter.peak_rate()) &&
      (meter.committed_burst() == meter.peak_burst())) {
    RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_symbol_add(entry_hdl, METER_MODEs, FLOWs));
  } else {
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_symbol_add(entry_hdl, METER_MODEs, TRTCMs));
     uint32 peak_rate = meter.peak_rate();
     uint32 peak_burst = meter.peak_burst();
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, MAX_RATE_KBPSs, peak_rate));
     RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, MAX_BURST_SIZE_KBITSs, peak_burst));
  }
  uint32 committed_rate = meter.committed_rate();
  uint32 committed_burst = meter.committed_burst();
  RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, MIN_RATE_KBPSs, committed_rate));
  RETURN_IF_BCM_ERROR(
         bcmlt_entry_field_add(entry_hdl, MIN_BURST_SIZE_KBITSs,
                               committed_burst));

  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, PKT_MODEs, 0));
  RETURN_IF_BCM_ERROR(
         bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                   BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateVfpEntry(int unit, int acl_id, int priority, int group_id,
                              int rule_id, int policy_id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_ENTRY_IDs, acl_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_GRP_TEMPLATE_IDs, group_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_RULE_TEMPLATE_IDs, rule_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_VLAN_POLICY_TEMPLATE_IDs, policy_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, priority));
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateIfpEntry(int unit, int acl_id, int priority, int group_id,
                              int rule_id, int policy_id, int meter_id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
  auto _ = gtl::MakeCleanup([entry_hdl]() { bcmlt_entry_free(entry_hdl); });
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_ENTRY_IDs, acl_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_GRP_TEMPLATE_IDs, group_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_RULE_TEMPLATE_IDs, rule_id));
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, FP_ING_POLICY_TEMPLATE_IDs, policy_id));
  if (meter_id > 0) {
     RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, METER_FP_ING_TEMPLATE_IDs, meter_id));
  }
  RETURN_IF_BCM_ERROR(
      bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, priority));
  RETURN_IF_BCM_ERROR(
      bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT,
                                BCMLT_PRIORITY_NORMAL));
  // RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status CreateEfpEntry(int unit, int acl_id, int priority, int group_id, int rule_id, int policy_id, int meter_id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_ENTRY_IDs, acl_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_GRP_TEMPLATE_IDs, group_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_RULE_TEMPLATE_IDs, rule_id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_POLICY_TEMPLATE_IDs, policy_id));
  if (meter_id > 0) {
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, meter_id));
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, ENTRY_PRIORITYs, priority));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_INSERT, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

} // namespace

::util::StatusOr<int> BcmSdkWrapper::InsertAclFlow(int unit,
                                                   const BcmFlowEntry& flow,
                                                   bool add_stats,
                                                   bool color_aware) {
  int rule_id = 0;
  int policy_id = 0;
  int meter_id = 0;
  int acl_id = 0;
  int rule_table_id = 0;
  int policy_table_id = 0;
  int meter_table_id = 0;
  int acl_table_id = 0;
  InUseMap *rule_ids;
  InUseMap *policy_ids;
  InUseMap *meter_ids;
  InUseMap *acl_ids;
  bool found;

  // check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  std::pair<BcmAclStage, int> entry;
  auto* group_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(group_ids,
                                             static_cast<int>(flow.bcm_acl_table_id()),
                                             &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "ACL Table id " << flow.bcm_acl_table_id() << " not found.";
  }
  BcmAclStage stage = flow.acl_stage();
  CHECK_RETURN_IF_FALSE(stage == entry.first)
      << "Invalid valid group, stage used for group is "
      << BcmAclStage_Name(entry.first)
      << " stage used for the flow is "
      << BcmAclStage_Name(stage);

  int group_id = entry.second;
  switch (stage) {
    case BCM_ACL_STAGE_VFP:
      rule_ids = gtl::FindOrNull(vfp_rule_ids_, unit);
      policy_ids = gtl::FindOrNull(vfp_policy_ids_, unit);
      acl_ids = gtl::FindOrNull(vfp_acl_ids_, unit);
      break;
    case BCM_ACL_STAGE_IFP:
      rule_ids = gtl::FindOrNull(ifp_rule_ids_, unit);
      policy_ids = gtl::FindOrNull(ifp_policy_ids_, unit);
      meter_ids = gtl::FindOrNull(ifp_meter_ids_, unit);
      acl_ids = gtl::FindOrNull(ifp_acl_ids_, unit);
      break;
    case BCM_ACL_STAGE_EFP:
      rule_ids = gtl::FindOrNull(efp_rule_ids_, unit);
      policy_ids = gtl::FindOrNull(efp_policy_ids_, unit);
      meter_ids = gtl::FindOrNull(efp_meter_ids_, unit);
      acl_ids = gtl::FindOrNull(efp_acl_ids_, unit);
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to create ACL flow with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }

  if (!rule_ids || !policy_ids || !acl_ids) {
     return MAKE_ERROR(ERR_INTERNAL)
            << "TODO(BRCM): Add proper error message";
  }

  int maxAcls = unit_to_fp_max_limit_[unit];
  ASSIGN_OR_RETURN(acl_id, GetFreeSlot(acl_ids, "ACL table is full."));
  AclIds* acl_table_ids = gtl::FindPtrOrNull(fp_acl_ids_, unit);
  ASSIGN_OR_RETURN(acl_table_id, GetUniqueId(acl_table_ids, -1, maxAcls));

  // get next free slot, this id is used to create entry in the LT table
  std::string errMsg = absl::StrCat(BcmAclStage_Name(stage), " table is full.");
  ASSIGN_OR_RETURN(rule_id, GetFreeSlot(rule_ids, errMsg));

  ASSIGN_OR_RETURN(policy_id, GetFreeSlot(policy_ids, errMsg));

  int maxRules = unit_to_fp_rules_max_limit_[unit];
  AclRuleIds* rule_table_ids = gtl::FindPtrOrNull(fp_rule_ids_, unit);
  ASSIGN_OR_RETURN(rule_table_id, GetUniqueId(rule_table_ids, -1, maxRules));

  int maxPolicies = unit_to_fp_policy_max_limit_[unit];
  AclPolicyIds* policy_table_ids = gtl::FindPtrOrNull(fp_policy_ids_, unit);
  ASSIGN_OR_RETURN(policy_table_id, GetUniqueId(policy_table_ids, -1,
                                                maxPolicies));

  if (stage == BCM_ACL_STAGE_VFP) {
     RETURN_IF_ERROR(CreateVfpRule(unit, rule_id, flow));
     RETURN_IF_ERROR(CreateVfpPolicy(unit, policy_id, flow));
  } else if (stage == BCM_ACL_STAGE_IFP) {
     RETURN_IF_ERROR(CreateIfpRule(unit, rule_id, flow));
     RETURN_IF_ERROR(CreateIfpPolicy(unit, policy_id, flow));
  } else if (stage == BCM_ACL_STAGE_EFP) {
     RETURN_IF_ERROR(CreateEfpRule(unit, rule_id, flow));
     RETURN_IF_ERROR(CreateEfpPolicy(unit, policy_id, flow));
  } else {
     return MAKE_ERROR(ERR_INVALID_PARAM)
            << "Attempted to create ACL flow with invalid pipeline stage: "
            << BcmAclStage_Name(stage) << ".";
  }

  // Add policer if meter config is specified.
  int maxMeters = unit_to_fp_meter_max_limit_[unit];
  AclMeterIds* meter_table_ids = nullptr;
  if (flow.has_meter()) {
    if (stage == BCM_ACL_STAGE_IFP || stage == BCM_ACL_STAGE_EFP) {
       if (!meter_ids){
          return MAKE_ERROR(ERR_INTERNAL)
                 << "TODO(BRCM): Add proper error message";
       }
       ASSIGN_OR_RETURN(meter_id, GetFreeSlot(meter_ids, errMsg))
       meter_table_ids = gtl::FindPtrOrNull(fp_meter_ids_, unit);
       ASSIGN_OR_RETURN(meter_table_id, GetUniqueId(meter_table_ids, -1,
                                                    maxMeters));
       RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, flow.meter()));
    } else {
       return MAKE_ERROR(ERR_INVALID_PARAM)
              << "TODO(BRCM): Add proper error message";
    }
  }

  if (stage == BCM_ACL_STAGE_VFP) {
     RETURN_IF_ERROR(
         CreateVfpEntry(unit, acl_id, flow.priority(),
                        group_id, rule_id, policy_id));
  } else if (stage == BCM_ACL_STAGE_IFP) {
     RETURN_IF_ERROR(
         CreateIfpEntry(unit, acl_id, flow.priority(),
                        group_id, rule_id, policy_id, meter_id));
  } else {
     RETURN_IF_ERROR(
         CreateEfpEntry(unit, acl_id, flow.priority(),
                        group_id, rule_id, policy_id, meter_id));
  }

  // update rule map
  ConsumeSlot(rule_ids, rule_id);
  rule_table_ids->emplace(std::make_pair(stage, rule_id), rule_table_id);

  // update policy map
  ConsumeSlot(policy_ids, policy_id);
  policy_table_ids->emplace(std::make_pair(stage, policy_id), policy_table_id);

  // update meter map
  if (flow.has_meter()) {
    ConsumeSlot(meter_ids, meter_id);
    meter_table_ids->emplace(std::make_pair(stage, meter_id), meter_table_id);
  }

  // update acl map
  ConsumeSlot(acl_ids, acl_id);
  acl_table_ids->emplace(std::make_pair(stage, acl_id), acl_table_id);
  return acl_table_id;
}

namespace {
::util::Status RemoveVfpRule(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_RULE_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveVfpPolicy(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_POLICY_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveVfpEntry(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveIfpRule(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_RULE_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveIfpPolicy(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_POLICY_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveIfpMeter(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, METER_FP_ING_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, METER_FP_ING_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveIfpEntry(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status DetachIfpMeter(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, METER_FP_ING_TEMPLATE_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveEfpRule(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_RULE_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_RULE_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveEfpPolicy(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_POLICY_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_POLICY_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveEfpMeter(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, METER_FP_EGR_TEMPLATEs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveEfpEntry(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_DELETE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status DetachEfpMeter(int unit, int id) {
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, 0));
  RETURN_IF_BCM_ERROR(bcmlt_custom_entry_commit(entry_hdl, BCMLT_OPCODE_UPDATE, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status RemoveVfpFlow(int unit, int rule_id, int policy_id, int entry_id) {
  RETURN_IF_ERROR(RemoveVfpRule(unit, rule_id));
  RETURN_IF_ERROR(RemoveVfpPolicy(unit, policy_id));
  RETURN_IF_ERROR(RemoveVfpEntry(unit, entry_id));
  return ::util::OkStatus();
}

::util::Status RemoveIfpFlow(int unit, int rule_id, int policy_id, int meter_id, int entry_id) {
  if (meter_id > 0) {
     RETURN_IF_ERROR(DetachIfpMeter(unit, entry_id));
     RETURN_IF_ERROR(RemoveIfpMeter(unit, meter_id));
  }
  RETURN_IF_ERROR(RemoveIfpEntry(unit, entry_id));
  RETURN_IF_ERROR(RemoveIfpRule(unit, rule_id));
  RETURN_IF_ERROR(RemoveIfpPolicy(unit, policy_id));
  return ::util::OkStatus();
}

::util::Status RemoveEfpFlow(int unit, int rule_id, int policy_id, int meter_id, int entry_id) {
  if (meter_id > 0) {
     RETURN_IF_ERROR(DetachEfpMeter(unit, entry_id));
     RETURN_IF_ERROR(RemoveEfpMeter(unit, meter_id));
  }
  RETURN_IF_ERROR(RemoveEfpEntry(unit, entry_id));
  RETURN_IF_ERROR(RemoveEfpRule(unit, rule_id));
  RETURN_IF_ERROR(RemoveEfpPolicy(unit, policy_id));
  return ::util::OkStatus();
}

::util::Status GetVfpEntry(int unit, int id, int* rule_id, int* policy_id) {
  uint64_t data;
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_VLAN_RULE_TEMPLATE_IDs, &data));
  *rule_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_VLAN_POLICY_TEMPLATE_IDs, &data));
  *policy_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status GetIfpEntry(int unit, int id, int* rule_id, int* policy_id, int* meter_id) {
  uint64_t data;
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_ING_RULE_TEMPLATE_IDs, &data));
  *rule_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_ING_POLICY_TEMPLATE_IDs, &data));
  *policy_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, METER_FP_ING_TEMPLATE_IDs, &data));
  *meter_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

::util::Status GetEfpEntry(int unit, int id, int* rule_id, int* policy_id, int* meter_id) {
  uint64_t data;
  bcmlt_entry_handle_t entry_hdl;
  RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_ENTRY_IDs, id));
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_EGR_RULE_TEMPLATE_IDs, &data));
  *rule_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_EGR_POLICY_TEMPLATE_IDs, &data));
  *policy_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, &data));
  *meter_id = static_cast<int>(data);
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  return ::util::OkStatus();
}

} // namespace

::util::Status BcmSdkWrapper::ModifyAclFlow(int unit, int flow_id,
                                            const BcmFlowEntry& flow) {
  bool found;
  std::pair<BcmAclStage, int> entry;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  auto* acl_table_ids = gtl::FindPtrOrNull(fp_acl_ids_, unit);
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(acl_table_ids, flow_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Flow id " << flow_id << " not found.";
  }

  BcmAclStage stage = flow.acl_stage();
  CHECK_RETURN_IF_FALSE(stage == entry.first)
      << "Stage used for the folw is not matching.";
  int acl_entry_id = entry.second;

  auto* group_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(group_ids, static_cast<int>(flow.bcm_acl_table_id()), &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "ACL Table id " << flow.bcm_acl_table_id() << " not found.";
  }
  CHECK_RETURN_IF_FALSE(stage == entry.first)
      << "Stage used for the group is not matching with the flow stage.";

  int group_id = entry.second;
  int rule_id = 0;
  int policy_id = 0;
  int meter_id = 0;
  int meter_table_id = 0;
  auto *fp_meters = gtl::FindPtrOrNull(fp_meter_ids_, unit);
  std::map<int, bool>* ifp_meter_ids = gtl::FindOrNull(ifp_meter_ids_, unit);;
  std::map<int, bool>* efp_meter_ids = gtl::FindOrNull(efp_meter_ids_, unit);;

  // Add policer if meter config is specified.
  int maxMeters = unit_to_fp_meter_max_limit_[unit];

  if (stage == BCM_ACL_STAGE_VFP) {
     RETURN_IF_ERROR(GetVfpEntry(unit, acl_entry_id, &rule_id, &policy_id));
     // TODO(BRCM): need to check if the flow is valid,
     // otherwise we should not delete existing flow
     RETURN_IF_ERROR(RemoveVfpEntry(unit, acl_entry_id));
     RETURN_IF_ERROR(RemoveVfpPolicy(unit, policy_id));
     RETURN_IF_ERROR(CreateVfpPolicy(unit, policy_id, flow));
     RETURN_IF_ERROR(CreateVfpEntry(unit, acl_entry_id, flow.priority(), group_id, rule_id, policy_id));
  } else if (stage == BCM_ACL_STAGE_IFP) {
     // TODO(BRCM): need to check if the flow is valid,
     // otherwise we should not delete existing flow
     RETURN_IF_ERROR(GetIfpEntry(unit, acl_entry_id, &rule_id, &policy_id, &meter_id));
     bool meter_deleted = false;
     if (meter_id > 0) {
       RETURN_IF_ERROR(DetachIfpMeter(unit, acl_entry_id));
       RETURN_IF_ERROR(RemoveIfpMeter(unit, meter_id));
       meter_deleted = true;
     }
     RETURN_IF_ERROR(RemoveIfpPolicy(unit, policy_id));
     RETURN_IF_ERROR(RemoveIfpEntry(unit, acl_entry_id));
     RETURN_IF_ERROR(CreateIfpPolicy(unit, policy_id, flow));
     bool need_map_update = false;
     if (flow.has_meter()) {
       if (meter_id == 0) {
          if (!ifp_meter_ids) {
             return MAKE_ERROR(ERR_INTERNAL)
                 << "TODO(BRCM): Add proper error message";
          }
          ASSIGN_OR_RETURN(meter_id, GetFreeSlot(ifp_meter_ids, "TODO(BRCM): add proper error message"));
          if (!fp_meters) {
             return MAKE_ERROR(ERR_INTERNAL)
                 << "TODO(BRCM): Add proper error message";
          }
          ASSIGN_OR_RETURN(meter_table_id, GetUniqueId(fp_meters, -1, maxMeters));
          need_map_update = true;
       }
       RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, flow.meter()));
       if (need_map_update) {
          ConsumeSlot(ifp_meter_ids, meter_id);
          fp_meters->emplace(std::make_pair(stage, meter_id), meter_table_id);
       }
     } else {
       if (meter_deleted) {
          ReleaseSlot(ifp_meter_ids, meter_id);
          fp_meters->erase(std::make_pair(stage, meter_id));
       }
       meter_id = 0;
     }
     RETURN_IF_ERROR(CreateIfpEntry(unit, acl_entry_id, flow.priority(), group_id, rule_id, policy_id, meter_id));
  } else if (stage == BCM_ACL_STAGE_EFP) {
     // TODO(BRCM): need to check if the flow is valid,
     // otherwise we should not delete existing flow
     RETURN_IF_ERROR(GetEfpEntry(unit, acl_entry_id, &rule_id, &policy_id, &meter_id));
     bool meter_deleted = false;
     if (meter_id > 0) {
       RETURN_IF_ERROR(DetachEfpMeter(unit, acl_entry_id));
       RETURN_IF_ERROR(RemoveEfpMeter(unit, meter_id));
       meter_deleted = true;
     }
     RETURN_IF_ERROR(RemoveEfpPolicy(unit, policy_id));
     RETURN_IF_ERROR(RemoveEfpEntry(unit, acl_entry_id));
     RETURN_IF_ERROR(CreateEfpPolicy(unit, policy_id, flow));
     bool need_map_update = false;
     if (flow.has_meter()) {
       if (meter_id == 0) {
          if (!efp_meter_ids) {
             return MAKE_ERROR(ERR_INTERNAL)
                 << "TODO(BRCM): Add proper error message";
          }
          ASSIGN_OR_RETURN(meter_id, GetFreeSlot(efp_meter_ids, "TODO(BRCM): add proper error message"));
          if (!fp_meters) {
             return MAKE_ERROR(ERR_INTERNAL)
                 << "TODO(BRCM): Add proper error message";
          }
          ASSIGN_OR_RETURN(meter_table_id, GetUniqueId(fp_meters, -1, maxMeters));
          need_map_update = true;
       }
       RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, flow.meter()));
       if (need_map_update) {
          ConsumeSlot(efp_meter_ids, meter_id);
          fp_meters->emplace(std::make_pair(stage, meter_id), meter_table_id);
       }
     } else {
       if (meter_deleted) {
          ReleaseSlot(ifp_meter_ids, meter_id);
          fp_meters->erase(std::make_pair(stage, meter_id));
       }
       meter_id = 0;
     }
     RETURN_IF_ERROR(CreateEfpEntry(unit, acl_entry_id, flow.priority(), group_id, rule_id, policy_id, meter_id));
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Flow table with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RemoveAclFlow(int unit, int flow_id) {
  // check if unit is valid
  bool found;
  std::pair<BcmAclStage, int> entry;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  // TODO(BRCM): check if flow id is valid
  auto* acl_table_ids = gtl::FindPtrOrNull(fp_acl_ids_, unit);

  ASSIGN_OR_RETURN(found, FindAndReturnEntry(acl_table_ids, flow_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Flow id " << flow_id << " not found.";
  }

  BcmAclStage stage = entry.first;
  int acl_entry_id = entry.second;
  int rule_id;
  int policy_id;
  int meter_id;
  std::map<int, bool>* rule_ids = nullptr;
  std::map<int, bool>* policy_ids = nullptr;
  std::map<int, bool>* meter_ids = nullptr;
  std::map<int, bool>* entry_ids = nullptr;
  auto *fp_rules = gtl::FindPtrOrNull(fp_rule_ids_, unit);
  auto *fp_policies = gtl::FindPtrOrNull(fp_policy_ids_, unit);
  auto *fp_meters = gtl::FindPtrOrNull(fp_meter_ids_, unit);

  if (stage == BCM_ACL_STAGE_VFP) {
     RETURN_IF_ERROR(GetVfpEntry(unit, acl_entry_id, &rule_id, &policy_id));
     RETURN_IF_ERROR(RemoveVfpFlow(unit, rule_id, policy_id, acl_entry_id));
     // remove vfp map
     rule_ids = gtl::FindOrNull(vfp_rule_ids_, unit);
     policy_ids = gtl::FindOrNull(vfp_policy_ids_, unit);
     entry_ids = gtl::FindOrNull(vfp_acl_ids_, unit);
  } else if (stage == BCM_ACL_STAGE_IFP) {
     RETURN_IF_ERROR(GetIfpEntry(unit, acl_entry_id, &rule_id, &policy_id, &meter_id));
     RETURN_IF_ERROR(RemoveIfpFlow(unit, rule_id, policy_id, meter_id, acl_entry_id));
     // remove ifp map
     rule_ids = gtl::FindOrNull(ifp_rule_ids_, unit);
     policy_ids = gtl::FindOrNull(ifp_policy_ids_, unit);
     meter_ids = gtl::FindOrNull(ifp_meter_ids_, unit);
     entry_ids = gtl::FindOrNull(ifp_acl_ids_, unit);
  } else if (stage == BCM_ACL_STAGE_EFP) {
     RETURN_IF_ERROR(GetEfpEntry(unit, acl_entry_id, &rule_id, &policy_id, &meter_id));
     RETURN_IF_ERROR(RemoveEfpFlow(unit, rule_id, policy_id, meter_id, acl_entry_id));
     // remove efp map
     rule_ids = gtl::FindOrNull(efp_rule_ids_, unit);
     policy_ids = gtl::FindOrNull(efp_policy_ids_, unit);
     meter_ids = gtl::FindOrNull(efp_meter_ids_, unit);
     entry_ids = gtl::FindOrNull(efp_acl_ids_, unit);
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Flow table with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }

  // remove acl_entry map
  acl_table_ids->erase(entry);
  ReleaseSlot(entry_ids, acl_entry_id);

  // remove fp_rule map
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(fp_rules, flow_id, &entry));
  if (found) {
     fp_rules->erase(entry);
     ReleaseSlot(rule_ids, entry.second);
  }

  // remove fp_policies map
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(fp_policies, flow_id, &entry));
  if (found) {
     fp_policies->erase(entry);
     ReleaseSlot(policy_ids, entry.second);
  }

  if (meter_id > 0) {
    // remove fp_meters map
    ASSIGN_OR_RETURN(found, FindAndReturnEntry(fp_meters, flow_id, &entry));
    if (found) {
       fp_meters->erase(entry);
       ReleaseSlot(meter_ids, entry.second);
    }
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclPolicer(int unit, int flow_id,
                                            const BcmMeterConfig& meter) {
  bool found;
  std::pair<BcmAclStage, int> entry;
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));

  auto* acl_table_ids = gtl::FindPtrOrNull(fp_acl_ids_, unit);
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(acl_table_ids, flow_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Flow id " << flow_id << " not found.";
  }
  BcmAclStage stage = entry.first;
  CHECK_RETURN_IF_FALSE((stage == BCM_ACL_STAGE_IFP) || (stage == BCM_ACL_STAGE_EFP))
      << "Meters can not be created/modified in the stage "
      << BcmAclStage_Name(stage) << ".";

  int acl_entry_id = entry.second;

  // get group
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t data;
  int priority = 0;
  int group_id = 0;
  int rule_id = 0;
  int policy_id = 0;
  int meter_id = 0;
  auto *fp_meters = gtl::FindPtrOrNull(fp_meter_ids_, unit);
  std::map<int, bool>* ifp_meter_ids = gtl::FindOrNull(ifp_meter_ids_, unit);;
  std::map<int, bool>* efp_meter_ids = gtl::FindOrNull(efp_meter_ids_, unit);;
  int maxMeters = unit_to_fp_meter_max_limit_[unit];
  int meter_table_id = 0;

  if (stage == BCM_ACL_STAGE_IFP) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_ENTRY_IDs, acl_entry_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    if (entry_info.status == SHR_E_NONE)
    {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ENTRY_PRIORITYs, &data));
      priority = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_ING_GRP_TEMPLATE_IDs, &data));
      group_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_ING_RULE_TEMPLATE_IDs, &data));
      rule_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_ING_POLICY_TEMPLATE_IDs, &data));
      policy_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, METER_FP_ING_TEMPLATE_IDs, &data));
      meter_id = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  } else if (stage == BCM_ACL_STAGE_EFP) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_ENTRY_IDs, acl_entry_id));
    RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
    RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
    if (entry_info.status == SHR_E_NONE)
    {
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ENTRY_PRIORITYs, &data));
      priority = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_EGR_GRP_TEMPLATE_IDs, &data));
      group_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_EGR_RULE_TEMPLATE_IDs, &data));
      rule_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, FP_EGR_POLICY_TEMPLATE_IDs, &data));
      policy_id = static_cast<int>(data);
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, METER_FP_EGR_TEMPLATE_IDs, &data));
      meter_id = static_cast<int>(data);
    }
    RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));
  }

  if (meter_id > 0) {
    if (stage == BCM_ACL_STAGE_IFP) {
      RETURN_IF_ERROR(DetachIfpMeter(unit, acl_entry_id));
      RETURN_IF_ERROR(RemoveIfpMeter(unit, meter_id));
      RETURN_IF_ERROR(RemoveIfpEntry(unit, acl_entry_id));
      RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, meter));
      RETURN_IF_ERROR(CreateIfpEntry(unit, acl_entry_id, priority, group_id, rule_id, policy_id, meter_id));
    } else if (BCM_ACL_STAGE_EFP) {
       RETURN_IF_ERROR(DetachEfpMeter(unit, acl_entry_id));
       RETURN_IF_ERROR(RemoveEfpMeter(unit, meter_id));
       RETURN_IF_ERROR(RemoveEfpEntry(unit, acl_entry_id));
       RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, meter));
       RETURN_IF_ERROR(CreateEfpEntry(unit, acl_entry_id, priority, group_id, rule_id, policy_id, meter_id));
    }
  } else {
    if (stage == BCM_ACL_STAGE_IFP) {
      if (!ifp_meter_ids) {
         return MAKE_ERROR(ERR_INTERNAL)
             << "TODO(BRCM): Add proper error message";
      }
      ASSIGN_OR_RETURN(meter_id, GetFreeSlot(ifp_meter_ids, "TODO(BRCM): add proper error message"));
      if (!fp_meters) {
         return MAKE_ERROR(ERR_INTERNAL)
             << "TODO(BRCM): Add proper error message";
      }
      ASSIGN_OR_RETURN(meter_table_id, GetUniqueId(fp_meters, -1, maxMeters));
      RETURN_IF_ERROR(RemoveIfpEntry(unit, acl_entry_id));
      RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, meter));
      RETURN_IF_ERROR(CreateIfpEntry(unit, acl_entry_id, priority, group_id, rule_id, policy_id, meter_id));
      ConsumeSlot(ifp_meter_ids, meter_id);
      fp_meters->emplace(std::make_pair(stage, meter_id), meter_table_id);
    } else if (BCM_ACL_STAGE_EFP) {
      if (!efp_meter_ids) {
         return MAKE_ERROR(ERR_INTERNAL)
             << "TODO(BRCM): Add proper error message";
      }
      ASSIGN_OR_RETURN(meter_id, GetFreeSlot(efp_meter_ids, "TODO(BRCM): add proper error message"));
      if (!fp_meters) {
         return MAKE_ERROR(ERR_INTERNAL)
             << "TODO(BRCM): Add proper error message";
      }
      ASSIGN_OR_RETURN(meter_table_id, GetUniqueId(fp_meters, -1, maxMeters));
      RETURN_IF_ERROR(RemoveIfpEntry(unit, acl_entry_id));
      RETURN_IF_ERROR(AddAclPolicer(unit, meter_id, stage, meter));
      RETURN_IF_ERROR(CreateIfpEntry(unit, acl_entry_id, priority, group_id, rule_id, policy_id, meter_id));
      ConsumeSlot(efp_meter_ids, meter_id);
      fp_meters->emplace(std::make_pair(stage, meter_id), meter_table_id);
    }
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InsertPacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {

  // Check pre-conditions
  CHECK_RETURN_IF_FALSE(entry.has_multicast_group_entry())
      << "Bcm does only support multicast groups for now";
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(entry.unit()));
  auto mcast_entry = entry.multicast_group_entry();
  CHECK_RETURN_IF_FALSE(!gtl::ContainsKey(multicast_group_id_to_replicas_,
      mcast_entry.multicast_group_id())) << "multicast group already exists";
  std::vector<int> ports;
  for (auto const& port : mcast_entry.ports()) {
    RETURN_IF_BCM_ERROR(CheckIfPortExists(entry.unit(), port));
    ports.push_back(port);
  }
  gtl::InsertOrDie(&multicast_group_id_to_replicas_, mcast_entry.multicast_group_id(), ports);

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeletePacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {
  // Check pre-conditions
  CHECK_RETURN_IF_FALSE(entry.has_multicast_group_entry());
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(entry.unit()));
  auto mcast_entry = entry.multicast_group_entry();
  auto ports = gtl::FindOrNull(
      multicast_group_id_to_replicas_, mcast_entry.multicast_group_id());
  CHECK_RETURN_IF_FALSE(ports != nullptr);

  multicast_group_id_to_replicas_.erase(mcast_entry.multicast_group_id());
  return ::util::OkStatus();
}

namespace {
::util::Status GetGroupDetails(int unit, int stage_id, int table_id,
                               BcmAclStage stage, BcmAclTable* table) {
  uint64_t value = 0;
  uint32_t configured;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;

  if (stage == BCM_ACL_STAGE_VFP) {
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_VLAN_GRP_TEMPLATE_IDs, stage_id));
  } else if (stage == BCM_ACL_STAGE_IFP) {
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_ING_GRP_TEMPLATE_IDs, stage_id));
  } else if (stage == BCM_ACL_STAGE_EFP) {
     RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_GRP_TEMPLATEs, &entry_hdl));
     RETURN_IF_BCM_ERROR(bcmlt_entry_field_add(entry_hdl, FP_EGR_GRP_TEMPLATE_IDs, stage_id));
  } else {
    return MAKE_ERROR(ERR_INVALID_PARAM) << "Invalid ACL stage " << BcmAclStage_Name(stage);
  }
  RETURN_IF_BCM_ERROR(bcmlt_entry_commit(entry_hdl, BCMLT_OPCODE_LOOKUP, BCMLT_PRIORITY_NORMAL));
  RETURN_IF_BCM_ERROR(bcmlt_entry_info_get(entry_hdl, &entry_info));
  if (entry_info.status == SHR_E_NONE) {
    // Get table pre-defined qualifiers.
    std::string unknown_qual = BcmField_Type_Name(BcmField::UNKNOWN);
    table->clear_fields();
    for (int i = BcmField::UNKNOWN + 1; i <= BcmField::Type_MAX; ++i) {
      auto field = static_cast<BcmField::Type>(i);
      std::string bcm_qual_field = HalAclFieldToBcm(stage, field);
      if((unknown_qual.compare(bcm_qual_field)) == 0) {
         continue;
      }
      RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, bcm_qual_field.c_str(), &value));
      configured = value & 0xffffffff;
      if (configured) {
         table->add_fields()->set_type(field);
      }
    }
    // Get table priority.
    value = 0;
    RETURN_IF_BCM_ERROR(bcmlt_entry_field_get(entry_hdl, ENTRY_PRIORITYs, &value));
    table->set_priority(static_cast<uint32>(value));

    uint64_t chunk_array[16];
    uint32_t num_chunks = 0;
    if (stage == BCM_ACL_STAGE_VFP) {
       RETURN_IF_BCM_ERROR(bcmlt_entry_field_array_get(entry_hdl, QUAL_UDF_CHUNKSs, 0, chunk_array, 16, &num_chunks));
    } else if (stage == BCM_ACL_STAGE_IFP) {
       RETURN_IF_BCM_ERROR(bcmlt_entry_field_array_get(entry_hdl, QUAL_UDF_CHUNKS_BITMAPs, 0, chunk_array, 16, &num_chunks));
    }
    if (num_chunks) {
      for (uint32_t i = 0; i < num_chunks; ++i) {
         configured = chunk_array[i] & 0xffffffff;
         if (configured) {
           table->add_fields()->set_udf_chunk_id(i+1);
           break;
         }
      }
    }
  }
  // Populate table id.
  table->set_id(table_id);
  table->set_stage(stage);

  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  return ::util::OkStatus();
}

} //namespace

::util::Status BcmSdkWrapper::GetAclTable(int unit, int table_id,
                                          BcmAclTable* table) {
  bool found;
  std::pair<BcmAclStage, int> entry;

  // check if unit is valid
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  auto* table_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  CHECK_RETURN_IF_FALSE(table_ids != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(table_ids, table_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "ACL Table id " << table_id << " not found.";
  }
  BcmAclStage stage = entry.first;
  int stage_id = entry.second;
  if ((stage == BCM_ACL_STAGE_VFP) ||
      (stage == BCM_ACL_STAGE_IFP) ||
      (stage == BCM_ACL_STAGE_EFP)) {
    RETURN_IF_ERROR(GetGroupDetails(unit, stage_id, table_id, stage, table));
  } else {
     return MAKE_ERROR(ERR_INTERNAL)
            << "ACL table with invalid pipeline stage: "
            << BcmAclStage_Name(stage) << ".";
  }
  return ::util::OkStatus();
}

namespace {

// Call Broadcom SDK function to get a specific qualifier field's value and mask
// for a flow entry given by flow_id. F denotes the type of the function. T
// denotes the SDK type for the match field which resolves to some integer type
// within 32 bits. F is of the form:
//   int (*bcm_field_qualify_<QualifierString>_get)(
//       int unit, bcm_field_entry_t flow_id, T* value, T* mask)
template <typename T, typename F>
inline int bcm_get_field_u32(F func, int unit, int flow_id, uint32* value,
                             uint32* mask) {
  T t_value, t_mask;
  int retval = func(unit, flow_id, &t_value, &t_mask);
  if (SHR_SUCCESS(retval)) {
    *value = static_cast<uint32>(t_value);
    *mask = static_cast<uint32>(t_mask);
  }
  return retval;
}

}  // namespace

::util::Status BcmSdkWrapper::GetAclFlow(int unit, int flow_id,
                                         BcmFlowEntry* flow) {
  // TODO: Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<std::string> BcmSdkWrapper::MatchAclFlow(
    int unit, int flow_id, const BcmFlowEntry& flow) {
  // TODO: Implement this function.
  return std::string();
}

::util::Status BcmSdkWrapper::GetAclTableFlowIds(int unit, int table_id,
                                                 std::vector<int>* flow_ids) {
  RETURN_IF_BCM_ERROR(CheckIfUnitExists(unit));
  auto* table_ids = gtl::FindPtrOrNull(fp_group_ids_, unit);
  CHECK_RETURN_IF_FALSE(table_ids != nullptr)
      << "Unit " << unit
      << " not initialized yet. Call InitializeUnit first.";

  std::pair<BcmAclStage, int> entry;
  bool found;
  ASSIGN_OR_RETURN(found, FindAndReturnEntry(table_ids, table_id, &entry));
  if(!found) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "ACL Table id " << table_id << " not found.";
  }
  BcmAclStage stage = entry.first;
  int hw_id = entry.second;

  const char *grp_id_str;
  const char *entry_id_str;
  bcmlt_entry_handle_t entry_hdl;
  bcmlt_entry_info_t entry_info;
  uint64_t e_id;
  uint64_t g_id;
  int rv;
  std::vector<std::pair<BcmAclStage, int>> keys;
  if (stage == BCM_ACL_STAGE_VFP) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_VLAN_ENTRYs, &entry_hdl));
    grp_id_str = "FP_VLAN_GRP_TEMPLATE_ID";
    entry_id_str = "FP_VLAN_ENTRY_ID";
  } else if (stage == BCM_ACL_STAGE_IFP) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_ING_ENTRYs, &entry_hdl));
    grp_id_str = "FP_ING_GRP_TEMPLATE_ID";
    entry_id_str = "FP_ING_ENTRY_ID";
  } else if (stage == BCM_ACL_STAGE_EFP) {
    RETURN_IF_BCM_ERROR(bcmlt_entry_allocate(unit, FP_EGR_ENTRYs, &entry_hdl));
    grp_id_str = "FP_EGR_GRP_TEMPLATE_ID";
    entry_id_str = "FP_EGR_ENTRY_ID";
  } else {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "ACL table with invalid pipeline stage: "
             << BcmAclStage_Name(stage) << ".";
  }
  while ((rv = bcmlt_entry_commit(entry_hdl,
                                  BCMLT_OPCODE_TRAVERSE,
                                  BCMLT_PRIORITY_NORMAL)) == SHR_E_NONE) {
      if (bcmlt_entry_info_get(entry_hdl, &entry_info) != SHR_E_NONE ||
                               entry_info.status != SHR_E_NONE) {
         break;
      }
      if (bcmlt_entry_field_get(entry_hdl, grp_id_str, &g_id) != SHR_E_NONE) {
         break;
      }
      if (bcmlt_entry_field_get(entry_hdl, entry_id_str, &e_id) != SHR_E_NONE) {
         break;
      }
      if (hw_id == static_cast<int>(g_id)) {
         keys.push_back(std::make_pair(stage, static_cast<int>(e_id)));
      }
    }
  RETURN_IF_BCM_ERROR(bcmlt_entry_free(entry_hdl));

  auto* acl_table_ids = gtl::FindPtrOrNull(fp_acl_ids_, unit);
  for (auto k : keys) {
    auto it = acl_table_ids->find(k);
    if (it != acl_table_ids->end()) {
      flow_ids->push_back(it->second);
    }
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddAclStats(int unit, int table_id, int flow_id,
                                          bool color_aware) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::RemoveAclStats(int unit, int flow_id) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::GetAclStats(int unit, int flow_id,
                                          BcmAclStats* stats) {
  // TODO(max): implement real function. This dummy just satisfies
  // the callers so that reading of ACL table entries is possible
  // return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
  stats->mutable_total()->set_bytes(0);
  stats->mutable_total()->set_packets(0);
  return ::util::OkStatus();
}

BcmSdkWrapper* BcmSdkWrapper::CreateSingleton(BcmDiagShell* bcm_diag_shell) {
  absl::WriterMutexLock l(&init_lock_);
  if (!singleton_) {
    singleton_ = new BcmSdkWrapper(bcm_diag_shell);
  }

  return singleton_;
}

BcmSdkWrapper* BcmSdkWrapper::GetSingleton() {
  absl::ReaderMutexLock l(&init_lock_);
  return singleton_;
}

::util::StatusOr<int> BcmSdkWrapper::GetSdkCheckpointFd(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

pthread_t BcmSdkWrapper::GetDiagShellThreadId() const {
  if (bcm_diag_shell_ == nullptr) return 0;  // sim mode
  return bcm_diag_shell_->GetDiagShellThreadId();
}

::util::Status BcmSdkWrapper::CleanupKnet(int unit) {
  // Cleanup existing KNET filters and KNET intfs.
  RETURN_IF_BCM_ERROR(
      bcmpkt_filter_traverse(unit, knet_filter_remover, nullptr));
  RETURN_IF_BCM_ERROR(
      bcmpkt_netif_traverse(unit, knet_intf_remover, nullptr));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::OpenSdkCheckpointFile(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::CreateSdkCheckpointFile(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::RegisterSdkCheckpointFile(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::StatusOr<std::string> BcmSdkWrapper::FindSdkCheckpointFilePath(
    int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::StatusOr<int> BcmSdkWrapper::FindSdkCheckpointFileSize(int unit) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::StatusOr<BcmChip::BcmChipType> BcmSdkWrapper::GetChipType(int unit) {
  absl::ReaderMutexLock l(&data_lock_);
  auto it = unit_to_chip_type_.find(unit);
  if (it == unit_to_chip_type_.end()) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Unit " << unit << "  is not found in unit_to_chip_type_. Have you "
           << "called FindUnit for this unit before?";
  }
  return it->second;
}

::util::Status BcmSdkWrapper::SetIntfAndConfigurePhyForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, uint64 speed_bps,
    const std::string& intf_type) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::SetSerdesRegisterForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, int serdes_lane,
    uint32 reg, uint32 value) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

::util::Status BcmSdkWrapper::SetSerdesAttributeForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, const std::string& attr,
    uint32 value) {
  return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE) << "Not supported.";
}

void BcmSdkWrapper::OnLinkscanEvent(int unit, int port, PortState linkstatus) {
  /* Create LinkscanEvent message. */
  PortState state;
  if (linkstatus == PORT_STATE_UP) {
    state = PORT_STATE_UP;
  } else if (linkstatus == PORT_STATE_DOWN) {
    state = PORT_STATE_DOWN;
  } else {
    state = PORT_STATE_UNKNOWN;
  }
  LinkscanEvent event = {unit, port, state};

  {
    absl::ReaderMutexLock l(&linkscan_writers_lock_);
    // Invoke the Writers based on priority.
    for (const auto& w : linkscan_event_writers_) {
      w.writer->Write(event, kWriteTimeout).IgnoreError();
    }
  }
}

int BcmSdkWrapper::CheckIfPortExists(int unit, int port) {
  absl::WriterMutexLock l(&data_lock_);
  auto logical_ports_map = gtl::FindOrNull(unit_to_logical_ports_, unit);
  if (!logical_ports_map) {
    LOG(ERROR) << "Logical ports are not identified on the Unit " << unit << ".";
    return SHR_E_INIT;
  }
  if (logical_ports_map->count(port)) {
    return SHR_E_NONE;
  } else {
    return SHR_E_NOT_FOUND;
  }
}

int BcmSdkWrapper::CheckIfUnitExists(int unit) {
  if (!bcmdrd_dev_exists(unit)) {
    LOG(ERROR) << "Unit " << unit << " is not found.";
    return SHR_E_UNIT;
  }
  return SHR_E_NONE;
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
