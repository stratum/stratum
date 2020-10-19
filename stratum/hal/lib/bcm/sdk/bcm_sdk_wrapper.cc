// Copyright 2018-2019 Google LLC
// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

/*
 * The Broadcom Switch API header code upon which this file depends is:
 * Copyright 2007-2020 Broadcom Inc.
 *
 * This file depends on Broadcom's OpenNSA SDK.
 * Additional license terms for OpenNSA are available from Broadcom or online:
 *     https://www.broadcom.com/products/ethernet-connectivity/software/opennsa
 */

#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"  // NOLINT

#include <arpa/inet.h>
#include <byteswap.h>
#include <endian.h>
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
#include <thread>  // NOLINT
#include <utility>

extern "C" {
#include "stratum/hal/lib/bcm/sdk_build_undef.h"  // NOLINT
#include "sdk_build_flags.h"                      // NOLINT
// TODO(bocon) we might be able to prune some of these includes
#include "appl/diag/bslmgmt.h"
#include "appl/diag/opennsa_diag.h"
#include "bcm/error.h"
#include "bcm/init.h"
#include "bcm/knet.h"
#include "bcm/l2.h"
#include "bcm/l3.h"
#include "bcm/link.h"
#include "bcm/policer.h"
#include "bcm/port.h"
#include "bcm/stack.h"
#include "bcm/stat.h"
#include "bcm/types.h"
#include "kcom.h"  // NOLINT
#include "sal/appl/config.h"
#include "sal/appl/sal.h"
#include "sal/core/boot.h"
#include "sal/core/libc.h"
#include "shared/bsl.h"
#include "shared/bslext.h"
#include "soc/cmext.h"
#include "soc/opensoc.h"
#include "stratum/hal/lib/bcm/sdk_build_undef.h"  // NOLINT

static_assert(SYS_BE_PIO == 0, "SYS_BE_PIO == 0");
static_assert(sizeof(COMPILER_UINT64) == 8, "sizeof(COMPILER_UINT64) == 8");
static_assert(sizeof(uint64) == 8, "sizeof(uint64) == 8");

/* Functions defined in src/diag/demo_opennsa_init.c */
ibde_t* bde = nullptr;
int bde_create(void) {
  linux_bde_bus_t bus;
  bus.be_pio = SYS_BE_PIO;
  bus.be_packet = SYS_BE_PACKET;
  bus.be_other = SYS_BE_OTHER;
  return linux_bde_create(&bus, &bde);
}
extern int soc_knet_config(void*);
extern int bde_icid_get(int d, uint8* data, int len);

/* Function defined in linux-user-bde.c */
extern int bde_icid_get(int d, uint8* data, int len);

// Over shadow the OpenNSA default symbol.
void sal_config_init_defaults(void) {}

// From OpenBCM systems/linux/kernel/modules/include/bcm-knet-kcom.h
extern void* bcm_knet_kcom_open(char* name);
extern int bcm_knet_kcom_close(void* handle);
extern int bcm_knet_kcom_msg_send(void* handle, void* msg, unsigned int len,
                                  unsigned int bufsz);
extern int bcm_knet_kcom_msg_recv(void* handle, void* msg, unsigned int bufsz);

// From OpenBCM systems/linux/user/common/socdiag.c
extern int bde_irq_mask_set(int unit, uint32 addr, uint32 mask);
extern int bde_hw_unit_get(int unit, int inverse);

// From OpenBCM include/soc/knet.h
typedef struct soc_knet_vectors_s {
  kcom_chan_t kcom;
  int (*irq_mask_set)(int unit, uint32 addr, uint32 mask);
  int (*hw_unit_get)(int unit, int inverse);
} soc_knet_vectors_t;

static soc_knet_vectors_t knet_vect_bcm_knet = {
    {
        bcm_knet_kcom_open,
        bcm_knet_kcom_close,
        bcm_knet_kcom_msg_send,
        bcm_knet_kcom_msg_recv,
    },
    bde_irq_mask_set,
    bde_hw_unit_get,
};

// From OpenBcm include/soc/drv.h
typedef bcm_switch_event_t soc_switch_event_t;
typedef void (*soc_event_cb_t)(int unit, soc_switch_event_t event, uint32 arg1,
                               uint32 arg2, uint32 arg3, void* userdata);
extern int soc_event_register(int unit, soc_event_cb_t cb, void* userdata);
extern int soc_esw_hw_qnum_get(int unit, int port, int cos, int* qnum);

// From OpenNSA 6.5.17 include/bcm/field.h
/* Set or get a field control value. */
extern int bcm_field_control_set(int unit, bcm_field_control_t control,
                                 uint32 state);

/* Add packet format-based offset to data qualifier object. */
extern int bcm_field_data_qualifier_packet_format_add(
    int unit, int qual_id, bcm_field_data_packet_format_t* packet_format);

/* bcm_field_qualify_DstClassField */
extern int bcm_field_qualify_DstClassField(int unit, bcm_field_entry_t entry,
                                           uint32 data, uint32 mask);

/*
 * Get match criteria for bcmFieldQualifyDstClassField
 *                qualifier from the field entry.
 */
extern int bcm_field_qualify_DstClassField_get(int unit,
                                               bcm_field_entry_t entry,
                                               uint32* data, uint32* mask);

/* bcm_field_qualify_IcmpTypeCode */
extern int bcm_field_qualify_IcmpTypeCode(int unit, bcm_field_entry_t entry,
                                          uint16 data, uint16 mask);

/*
 * Get match criteria for bcmFieldQualifyIcmpTypeCode
 *                qualifier from the field entry.
 */
extern int bcm_field_qualify_IcmpTypeCode_get(int unit, bcm_field_entry_t entry,
                                              uint16* data, uint16* mask);
}  // extern "C"

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

DEFINE_int64(linkscan_interval_in_usec, 200000, "Linkscan interval in usecs.");
DEFINE_int64(port_counters_interval_in_usec, 100 * 1000,
             "Port counter interval in usecs.");
DEFINE_int32(max_num_linkscan_writers, 10,
             "Max number of linkscan event Writers supported.");
DECLARE_string(bcm_sdk_checkpoint_dir);

using ::google::protobuf::util::MessageDifferencer;

// TODO(unknown): There are many CHECK_RETURN_IF_FALSE in this file which will
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
constexpr bcm_field_stat_t
    BcmSdkWrapper::kUncoloredStatEntry[kUncoloredStatCount];
constexpr int BcmSdkWrapper::kRedCounterIndex;
constexpr int BcmSdkWrapper::kGreenCounterIndex;
constexpr bcm_field_stat_t BcmSdkWrapper::kColoredStatEntry[kColoredStatCount];

// All the C style functions and vars used to work with BCM sdk need to be
// put into the following unnamed namespace.
namespace {

// A wrapper around BcmSdkWrapper::GetBde() for easier access and remove code
// repeatation. It logs the error and returns nullptr if there is any error.
ibde_t* GetBde() {
  BcmSdkWrapper* bcm_sdk_wrapper = BcmSdkWrapper::GetSingleton();
  if (!bcm_sdk_wrapper) {
    LOG(ERROR) << "BcmSdkWrapper singleton instance is not initialized.";
    return nullptr;
  }

  ::util::StatusOr<ibde_t*> ret = bcm_sdk_wrapper->GetBde();
  if (!ret.ok()) {
    LOG(ERROR) << "BcmSdkWrapper::GetBde() failed: " << ret.status();
    return nullptr;
  }

  return ret.ConsumeValueOrDie();
}

// Callback functions registered to the sdk in soc_cm_device_init()
// for low level system access services.  Most handlers defer the calls
// to the default bde implementation that performs its tasks using
// IOCTL to the kernel modules.
extern "C" char* sdk_config_var_get(soc_cm_dev_t* dev, const char* property) {
  COMPILER_REFERENCE(dev);
  return sal_config_get(property);
}

extern "C" int sdk_interrupt_connect(soc_cm_dev_t* dev,
                                     soc_cm_isr_func_t handler, void* data) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return bde->interrupt_connect(dev_num, handler, data);
}

extern "C" int sdk_interrupt_disconnect(soc_cm_dev_t* dev) { return 0; }

extern "C" uint32 sdk_read(soc_cm_dev_t* dev, uint32 addr) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return bde->read(dev_num, addr);
}

extern "C" void sdk_write(soc_cm_dev_t* dev, uint32 addr, uint32 data) {
  ibde_t* bde = GetBde();
  if (!bde) return;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  bde->write(dev_num, addr, data);
}

extern "C" uint32 sdk_pci_conf_read(soc_cm_dev_t* dev, uint32 addr) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return bde->pci_conf_read(dev_num, addr);
}

extern "C" void sdk_pci_conf_write(soc_cm_dev_t* dev, uint32 addr,
                                   uint32 data) {
  ibde_t* bde = GetBde();
  if (!bde) return;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  bde->pci_conf_write(dev_num, addr, data);
}

extern "C" void* sdk_salloc(soc_cm_dev_t* dev, int size, const char* name) {
  ibde_t* bde = GetBde();
  if (!bde) return nullptr;
  COMPILER_REFERENCE(name);
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return bde->salloc(dev_num, size, name);
}

extern "C" void sdk_sfree(soc_cm_dev_t* dev, void* ptr) {
  ibde_t* bde = GetBde();
  if (!bde) return;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  bde->sfree(dev_num, ptr);
}

extern "C" int sdk_sinval(soc_cm_dev_t* dev, void* addr, int length) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return (bde->sinval) ? bde->sinval(dev_num, addr, length) : 0;
}

extern "C" int sdk_sflush(soc_cm_dev_t* dev, void* addr, int length) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return (bde->sflush) ? bde->sflush(dev_num, addr, length) : 0;
}

extern "C" sal_paddr_t sdk_l2p(soc_cm_dev_t* dev, void* addr) {
  ibde_t* bde = GetBde();
  if (!bde) return 0;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return (bde->l2p) ? bde->l2p(dev_num, addr) : 0;
}

extern "C" void* sdk_p2l(soc_cm_dev_t* dev, sal_paddr_t addr) {
  ibde_t* bde = GetBde();
  if (!bde) return nullptr;
  int dev_num = *reinterpret_cast<int*>(dev->cookie);
  return (bde->p2l) ? bde->p2l(dev_num, addr) : 0;
}

extern "C" int sdk_i2c_device_read(soc_cm_dev_t* dev, uint32 addr,
                                   uint32* value) {
  ibde_t* bde = GetBde();
  if (!bde) return -1;
  return (bde->i2c_device_read) ? bde->i2c_device_read(dev->dev, addr, value)
                                : -1;
}

extern "C" int sdk_i2c_device_write(soc_cm_dev_t* dev, uint32 addr,
                                    uint32 value) {
  ibde_t* bde = GetBde();
  if (!bde) return -1;
  return (bde->i2c_device_write) ? bde->i2c_device_write(dev->dev, addr, value)
                                 : -1;
}

// Callback function registered to the sdk for receiving switch events
extern "C" void sdk_event_handler(int unit, bcm_switch_event_t event,
                                  uint32 arg1, uint32 arg2, uint32 arg3,
                                  void* userdata) {
  // TODO(unknown): Act upon different SDK events.
  switch (event) {
    case BCM_SWITCH_EVENT_IO_ERROR:
      break;
    case BCM_SWITCH_EVENT_PARITY_ERROR:
      switch (arg1 & 0xffff) {
        case SOC_SWITCH_EVENT_DATA_ERROR_PARITY:
        case SOC_SWITCH_EVENT_DATA_ERROR_ECC:
        case SOC_SWITCH_EVENT_DATA_ERROR_UNSPECIFIED:
        case SOC_SWITCH_EVENT_DATA_ERROR_FATAL:
          break;
        case SOC_SWITCH_EVENT_DATA_ERROR_CORRECTED:
        case SOC_SWITCH_EVENT_DATA_ERROR_AUTO_CORRECTED:
          break;
      }
      break;
    case BCM_SWITCH_EVENT_THREAD_ERROR:
      break;
    case BCM_SWITCH_EVENT_ACCESS_ERROR:
      break;
    case BCM_SWITCH_EVENT_ASSERT_ERROR:
      break;
    case BCM_SWITCH_EVENT_MODID_CHANGE:
      break;
    case BCM_SWITCH_EVENT_DOS_ATTACK:
      break;
    case BCM_SWITCH_EVENT_STABLE_FULL:
      break;
    case BCM_SWITCH_EVENT_STABLE_ERROR:
      break;
    case BCM_SWITCH_EVENT_UNCONTROLLED_SHUTDOWN:
      break;
    case BCM_SWITCH_EVENT_WARM_BOOT_DOWNGRADE:
      break;
    default:
      break;
  }
}

// A wrapper around BcmSdkWrapper::GetSdkCheckpointFd() for easier access and
// remove code repeatation. It logs the error and returns -1 if there is any
// error.
int GetSdkCheckpointFd(int unit) {
  BcmSdkWrapper* bcm_sdk_wrapper = BcmSdkWrapper::GetSingleton();
  if (!bcm_sdk_wrapper) {
    LOG(ERROR) << "BcmSdkWrapper singleton instance is not initialized.";
    return -1;
  }

  ::util::StatusOr<int> ret = bcm_sdk_wrapper->GetSdkCheckpointFd(unit);
  if (!ret.ok()) {
    LOG(ERROR) << "BcmSdkWrapper::GetSdkCheckpointFd() failed: "
               << ret.status();
    return -1;
  }

  return ret.ConsumeValueOrDie();
}

// Callback for reading SDK checkpoint file read.
extern "C" int sdk_checkpoint_file_read(int unit, uint8* buf, int offset,
                                        int nbytes) {
  int fd = GetSdkCheckpointFd(unit);
  if (fd == -1) return SOC_E_FAIL;

  if (lseek(fd, offset, SEEK_SET) == -1) {
    LOG(ERROR) << "lseek() failed on SDK checkpoint FD for unit " << unit
               << ".";
    return SOC_E_FAIL;
  }
  if (read(fd, buf, nbytes) != nbytes) {
    LOG(ERROR) << "read() failed to read " << nbytes << " from SDK checkpoint "
               << "FD for unit " << unit << ".";
    return SOC_E_FAIL;
  }

  return SOC_E_NONE;
}

// Callback for writing SDK checkpoint file write.
extern "C" int sdk_checkpoint_file_write(int unit, uint8* buf, int offset,
                                         int nbytes) {
  int fd = GetSdkCheckpointFd(unit);
  if (fd == -1) return SOC_E_FAIL;

  if (lseek(fd, offset, SEEK_SET) == -1) {
    LOG(ERROR) << "lseek() failed on SDK checkpoint FD for unit " << unit
               << ".";
    return SOC_E_FAIL;
  }
  if (write(fd, buf, nbytes) != nbytes) {
    LOG(ERROR) << "write() failed to write " << nbytes << " to SDK checkpoint "
               << "FD for unit " << unit << ".";
    return SOC_E_FAIL;
  }
  if (fdatasync(fd) == -1) {
    LOG(ERROR) << "fdatasync() failed on SDK checkpoint FD for unit " << unit
               << ".";
    return SOC_E_FAIL;
  }

  return SOC_E_NONE;
}

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
    if (meta->source != bslSourceEcho) {
      rc = bsl_vprintf(format, args);
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
        meta->xtra == (BSL_LS_BCMAPI_CUSTOM | BSL_DEBUG)) {
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
  bsl_severity_t severity =
      static_cast<bsl_severity_t>(BSL_SEVERITY_GET(meta_pack));
  // TODO(max): fix
  return 1;
}

// Configuration used by the BSL (Broadcom System Logging) module.
bsl_config_t sdk_bsl_config = {bsl_out_hook, bsl_check_hook};

// Declaration of the KNET-related SDK APIs. These APIs are implemented by SDK.
// TODO(unknown): add or replace

// Callback for removing KNET intf.
extern "C" int knet_intf_remover(int unit, bcm_knet_netif_t* netif,
                                 void* dummy) {
  return bcm_knet_netif_destroy(unit, netif->id);
}

// Callback for removing KNET filter.
extern "C" int knet_filter_remover(int unit, bcm_knet_filter_t* filter,
                                   void* dummy) {
  return bcm_knet_filter_destroy(unit, filter->id);
}

// A callback function executed in BCM linkscan thread context.
extern "C" void sdk_linkscan_callback(int unit, bcm_port_t port,
                                      bcm_port_info_t* info) {
  BcmSdkWrapper* bcm_sdk_wrapper = BcmSdkWrapper::GetSingleton();
  if (!bcm_sdk_wrapper) {
    LOG(ERROR) << "BcmSdkWrapper singleton instance is not initialized.";
    return;
  }
  LOG(INFO) << "Unit: " << unit << " Port: " << port << " Link: "
            << "changed.";
  // Forward the event.
  bcm_sdk_wrapper->OnLinkscanEvent(unit, port, info);
}

extern "C" bcm_rx_t packet_receive_callback(int unit, bcm_pkt_t* packet,
                                            void* packet_io_manager_cookie) {
  // Not handled at this point as we are using KNET.
  VLOG(1) << "PacketIn on unit " << unit << ".";
  return BCM_RX_NOT_HANDLED;
}

// Converts MAC address as uint64 in host order to byte array.
void Uint64ToBcmMac(uint64 mac, uint8 (*bcm_mac)[6]) {
  // uint64 nw_order_mac = htobe64(mac);
  for (int i = 5; i >= 0; --i) {
    (*bcm_mac)[i] = mac & 0xff;
    mac >>= 8;
  }
}

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

std::string BcmIpv4ToStr(const bcm_ip_t ipv4) {
  return absl::Substitute("$0.$1.$2.$3", (ipv4 >> 24) & 0xff,
                          (ipv4 >> 16) & 0xff, (ipv4 >> 8) & 0xff, ipv4 & 0xff);
}

std::string BcmIpv6ToStr(const bcm_ip6_t ipv6) {
  return absl::Substitute(
      "$0:$1:$2:$3:$4:$5:$6:$7",
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[0]) << 8) | ipv6[1])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[2]) << 8) | ipv6[3])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[4]) << 8) | ipv6[5])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[6]) << 8) | ipv6[7])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[8]) << 8) | ipv6[9])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[10]) << 8) | ipv6[11])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[12]) << 8) | ipv6[13])),
      absl::StrCat(absl::Hex((static_cast<uint32>(ipv6[14]) << 8) | ipv6[15])));
}

// Pretty prints an L3 intf object.
std::string PrintL3RouterIntf(const bcm_l3_intf_t& l3_intf) {
  std::stringstream buffer;
  buffer << "(vlan: " << l3_intf.l3a_vid << ", ";
  buffer << "ttl: " << l3_intf.l3a_ttl << ", ";
  buffer << "mtu: " << l3_intf.l3a_mtu << ", ";
  buffer << "src_mac: " << BcmMacToStr(l3_intf.l3a_mac_addr) << ", ";
  buffer << "router_intf_id: " << l3_intf.l3a_intf_id << ")";

  return buffer.str();
}

// Pretty prints an L3 egress object.
std::string PrintL3EgressIntf(const bcm_l3_egress_t& l3_egress,
                              int egress_intf_id) {
  std::stringstream buffer;
  if (l3_egress.trunk > 0) {
    buffer << "(trunk: " << l3_egress.trunk << ", ";
  } else {
    buffer << "(port: " << l3_egress.port << ", ";
  }
  buffer << "vlan: " << l3_egress.vlan << ", ";
  buffer << "router_intf_id: " << l3_egress.intf << ", ";
  buffer << "dst_mac: " << BcmMacToStr(l3_egress.mac_addr) << ", ";
  buffer << "egress_intf_id: " << egress_intf_id << ")";

  return buffer.str();
}

// Pretty prints an L3 route.
std::string PrintL3Route(const bcm_l3_route_t& route) {
  std::stringstream buffer;
  if (route.l3a_flags & BCM_L3_IP6) {
    buffer << "IPv6 LPM route (";
    buffer << "subnet: " << BcmIpv6ToStr(route.l3a_ip6_net) << ", ";
    buffer << "prefix: " << BcmIpv6ToStr(route.l3a_ip6_mask) << ", ";
  } else {
    buffer << "IPv4 LPM route (";
    buffer << "subnet: " << BcmIpv4ToStr(route.l3a_subnet) << ", ";
    buffer << "prefix: " << BcmIpv4ToStr(route.l3a_ip_mask) << ", ";
  }
  buffer << "vrf: " << route.l3a_vrf << ", ";
  buffer << "class_id: " << route.l3a_lookup_class << ", ";
  buffer << "egress_intf_id: " << route.l3a_intf << ")";

  return buffer.str();
}

// Pretty prints an L3 host.
std::string PrintL3Host(const bcm_l3_host_t& host) {
  std::stringstream buffer;
  if (host.l3a_flags & BCM_L3_IP6) {
    buffer << "IPv6 host route (";
    buffer << "subnet: " << BcmIpv6ToStr(host.l3a_ip6_addr) << ", ";
  } else {
    buffer << "IPv4 host route (";
    buffer << "subnet: " << BcmIpv4ToStr(host.l3a_ip_addr) << ", ";
  }
  buffer << "vrf: " << host.l3a_vrf << ", ";
  buffer << "class_id: " << host.l3a_lookup_class << ", ";
  buffer << "egress_intf_id: " << host.l3a_intf << ")";

  return buffer.str();
}

// Wrapper around SDK calls to see if the L3 intf object exists. If not, try to
// create it.
int FindOrCreateL3RouterIntfHelper(int unit, bcm_l3_intf_t* l3_intf) {
  int rv = bcm_l3_intf_find(unit, l3_intf);
  if (BCM_SUCCESS(rv)) {
    VLOG(1) << "L3 intf " << PrintL3RouterIntf(*l3_intf)
            << " already exists on unit " << unit << ".";
  } else {
    rv = bcm_l3_intf_create(unit, l3_intf);
    if (BCM_SUCCESS(rv)) {
      VLOG(1) << "Created a new L3 router intf: " << PrintL3RouterIntf(*l3_intf)
              << " on unit " << unit << ".";
    }
  }

  return rv;
}

// Wrapper around SDK calls to see if L3 egress object exists. If not, try to
// create it.
int FindOrCreateL3EgressIntfHelper(int unit, bcm_l3_egress_t* l3_egress,
                                   bcm_if_t* egress_intf_id) {
  // TODO(unknown): We decide to not look for existing entries and always
  // create new ones.

  // Note that we do not use flag BCM_L3_WITH_ID and let the SDK find the
  // egress intf ID. This call will create a new egress intf regardless of
  // whether the nexthop exists.
  int rv = bcm_l3_egress_create(unit, 0, l3_egress, egress_intf_id);
  if (BCM_SUCCESS(rv)) {
    VLOG(1) << "Created a new L3 egress intf: "
            << PrintL3EgressIntf(*l3_egress, *egress_intf_id) << " on unit "
            << unit << ".";
  }

  return rv;
}

// Wrapper around SDK calls to modify an existing L3 egress object.
int ModifyL3EgressIntfHelper(int unit, bcm_if_t egress_intf_id,
                             bcm_l3_egress_t* l3_egress) {
  // Here we explicitly use BCM_L3_WITH_ID and BCM_L3_REPLACE to replace the
  // existing egress intf while keepting the ID the same.
  int rv = bcm_l3_egress_create(unit, BCM_L3_WITH_ID | BCM_L3_REPLACE,
                                l3_egress, &egress_intf_id);
  if (BCM_SUCCESS(rv)) {
    VLOG(1) << "Modified L3 egress intf while keeping its ID the same: "
            << PrintL3EgressIntf(*l3_egress, egress_intf_id) << " on unit "
            << unit << ".";
  }

  return rv;
}

// Wrapper around SDK calls to see if an ECMP group with the given set of egress
// intf IDs exists. If not, try to create it.
int FindOrCreateEcmpEgressIntfHelper(int unit,
                                     bcm_l3_egress_ecmp_t* l3_egress_ecmp,
                                     int members_count,
                                     bcm_if_t* members_array) {
  // TODO(unknown): We decide to not look for existing entries and always
  // create new ones.

  // This call will create a new egress intf regardless of whether a group with
  // the exact same members exists.
  int rv = bcm_l3_egress_ecmp_create(unit, l3_egress_ecmp, members_count,
                                     members_array);
  if (BCM_SUCCESS(rv)) {
    VLOG(1) << "New ECMP group with ID " << l3_egress_ecmp->ecmp_intf
            << " created with following egress intf IDs as members: "
            << PrintArray(members_array, members_count, ", ") << " on unit "
            << unit << ".";
  }

  return rv;
}

// Wrapper around SDK calls to see if an ECMP group with the given set of egress
// intf IDs exists. If not, try to create it.
int ModifyEcmpEgressIntfHelper(int unit, bcm_l3_egress_ecmp_t* l3_egress_ecmp,
                               int members_count, bcm_if_t* members_array) {
  // Here we explicitly use BCM_L3_WITH_ID and BCM_L3_REPLACE to replace the
  // existing egress intf while keepting the ID the same.
  l3_egress_ecmp->flags |= BCM_L3_WITH_ID;
  l3_egress_ecmp->flags |= BCM_L3_REPLACE;
  int rv = bcm_l3_egress_ecmp_create(unit, l3_egress_ecmp, members_count,
                                     members_array);
  if (BCM_SUCCESS(rv)) {
    VLOG(1) << "ECMP group with ID " << l3_egress_ecmp->ecmp_intf
            << " modified with following egress intf IDs as members: "
            << PrintArray(members_array, members_count, ", ") << " on unit "
            << unit << ".";
  }

  return rv;
}

// Maps the special VRFs set by controller to its BCM equivalent.
bcm_vrf_t ControllerVrfToBcmVrf(int vrf) {
  if (vrf == kVrfDefault) {
    return BCM_L3_VRF_DEFAULT;
  } else if (vrf == kVrfOverride) {
    return BCM_L3_VRF_OVERRIDE;
  } else if (vrf == kVrfFallback) {
    return BCM_L3_VRF_GLOBAL;
  } else {
    return vrf;
  }
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
  uint8 reserved[4];
} __attribute__((__packed__));

struct RcpuHeader {
  struct ether_header ether_header;
  struct VlanTag vlan_tag;
  struct RcpuData rcpu_data;
} __attribute__((__packed__));

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
// NOTE: This function was copied from
// sandcastle/stack/lib/packetio/dcb_helper-impl.h with almost no change.
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
// NOTE: This function was copied from
// sandcastle/stack/lib/packetio/dcb_helper-impl.h with almost no change.
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
// NOTE: This function was copied from
// sandcastle/stack/lib/packetio/dcb_helper-impl.h with almost no change.
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
// NOTE: This function was copied from
// sandcastle/stack/lib/packetio/dcb_helper-impl.h with almost no change.
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

}  // namespace

BcmSdkWrapper* BcmSdkWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BcmSdkWrapper::init_lock_(absl::kConstInit);

BcmSdkWrapper::BcmSdkWrapper(BcmDiagShell* bcm_diag_shell)
    : bde_(nullptr),
      unit_to_chip_type_(),
      unit_to_soc_device_(),
      bcm_diag_shell_(bcm_diag_shell),
      linkscan_event_writers_() {
  // For consistency, we make sure some of the default values Stratum stack
  // uses internally match the SDK equivalents. This will make sure we dont
  // have inconsistent defaults in different places.
  static_assert(kDefaultVlan == BCM_VLAN_DEFAULT,
                "kDefaultVlan != BCM_VLAN_DEFAULT");
  static_assert(kDefaultCos == BCM_COS_DEFAULT,
                "kDefaultCos != BCM_COS_DEFAULT");
  static_assert(kMaxCos == BCM_COS_MAX, "kMaxCos != BCM_COS_MAX");
}

BcmSdkWrapper::~BcmSdkWrapper() { ShutdownAllUnits().IgnoreError(); }

::util::StatusOr<std::string> BcmSdkWrapper::GenerateBcmConfigFile(
    const BcmChassisMap& base_bcm_chassis_map,
    const BcmChassisMap& target_bcm_chassis_map, OperationMode mode) {
  std::stringstream buffer;

  // initialize the port mask. The total number of chips supported comes from
  // base_bcm_chassis_map.
  const size_t max_num_units = base_bcm_chassis_map.bcm_chips_size();
  std::vector<uint64> xe_pbmp_mask0(max_num_units, 0);
  std::vector<uint64> xe_pbmp_mask1(max_num_units, 0);
  std::vector<uint64> xe_pbmp_mask2(max_num_units, 0);
  std::vector<bool> is_chip_oversubscribed(max_num_units, false);

  // Chassis-level SDK properties.
  if (target_bcm_chassis_map.has_bcm_chassis()) {
    const auto& bcm_chassis = target_bcm_chassis_map.bcm_chassis();
    for (const std::string& sdk_property : bcm_chassis.sdk_properties()) {
      buffer << sdk_property << std::endl;
    }
    // In addition to SDK properties in the config, in the sim mode we need to
    // also add properties to disable DMA.
    if (mode == OPERATION_MODE_SIM) {
      buffer << "tdma_intr_enable=0" << std::endl;
      buffer << "tslam_dma_enable=0" << std::endl;
      buffer << "table_dma_enable=0" << std::endl;
    }
    buffer << std::endl;
  }

  // Chip-level SDK properties.
  for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
    int unit = bcm_chip.unit();
    if (bcm_chip.sdk_properties_size()) {
      for (const std::string& sdk_property : bcm_chip.sdk_properties()) {
        buffer << sdk_property << std::endl;
      }
      buffer << std::endl;
    }
    if (bcm_chip.is_oversubscribed()) {
      is_chip_oversubscribed[unit] = true;
    }
  }

  // XE port maps.
  // TODO(unknown): See if there is some BCM macros to work with pbmp's.
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    if (bcm_port.type() == BcmPort::XE || bcm_port.type() == BcmPort::CE) {
      int idx = bcm_port.logical_port();
      int unit = bcm_port.unit();
      if (idx < 64) {
        xe_pbmp_mask0[unit] |= static_cast<uint64>(0x01) << idx;
      } else if (idx < 128) {
        xe_pbmp_mask1[unit] |= static_cast<uint64>(0x01) << (idx - 64);
      } else {
        xe_pbmp_mask2[unit] |= static_cast<uint64>(0x01) << (idx - 128);
      }
    }
  }
  for (size_t i = 0; i < max_num_units; ++i) {
    if (xe_pbmp_mask1[i] || xe_pbmp_mask0[i] || xe_pbmp_mask2[i]) {
      std::stringstream mask(std::stringstream::in | std::stringstream::out);
      std::stringstream t0(std::stringstream::in | std::stringstream::out);
      std::stringstream t1(std::stringstream::in | std::stringstream::out);
      if (xe_pbmp_mask2[i]) {
        t0 << std::hex << std::uppercase << xe_pbmp_mask0[i];
        t1 << std::hex << std::uppercase << xe_pbmp_mask1[i];
        mask << std::hex << std::uppercase << xe_pbmp_mask2[i]
             << std::string(2 * sizeof(uint64) - t1.str().length(), '0')
             << t1.str()
             << std::string(2 * sizeof(uint64) - t0.str().length(), '0')
             << t0.str();
      } else if (xe_pbmp_mask1[i]) {
        t0 << std::hex << std::uppercase << xe_pbmp_mask0[i];
        mask << std::hex << std::uppercase << xe_pbmp_mask1[i]
             << std::string(2 * sizeof(uint64) - t0.str().length(), '0')
             << t0.str();
      } else {
        mask << std::hex << std::uppercase << xe_pbmp_mask0[i];
      }
      buffer << "pbmp_xport_xe." << i << "=0x" << mask.str() << std::endl;
      if (is_chip_oversubscribed[i]) {
        buffer << "pbmp_oversubscribe." << i << "=0x" << mask.str()
               << std::endl;
      }
    }
  }
  buffer << std::endl;

  // Port properties. Before that we create a map from chip-type to
  // map of channel to speed_bps for the flex ports.
  std::map<BcmChip::BcmChipType, std::map<int, uint64>>
      flex_chip_to_channel_to_speed = {{BcmChip::TOMAHAWK,
                                        {{1, kHundredGigBps},
                                         {2, kTwentyFiveGigBps},
                                         {3, kFiftyGigBps},
                                         {4, kTwentyFiveGigBps}}},
                                       {BcmChip::TRIDENT2,
                                        {{1, kFortyGigBps},
                                         {2, kTenGigBps},
                                         {3, kTwentyGigBps},
                                         {4, kTenGigBps}}}};
  for (const auto& bcm_port : target_bcm_chassis_map.bcm_ports()) {
    uint64 speed_bps = 0;
    if (bcm_port.type() == BcmPort::XE || bcm_port.type() == BcmPort::CE ||
        bcm_port.type() == BcmPort::GE) {
      // Find the type of the chip hosting this port. Then find the speed
      // which we need to set in the config.bcm, which depends on whether
      // the port is flex or not. We dont use GetBcmChip as unit_to_bcm_chip_
      // may not be populated when this function is called.
      BcmChip::BcmChipType chip_type = BcmChip::UNKNOWN;
      for (const auto& bcm_chip : target_bcm_chassis_map.bcm_chips()) {
        if (bcm_chip.unit() == bcm_port.unit()) {
          chip_type = bcm_chip.type();
          break;
        }
      }
      if (bcm_port.flex_port()) {
        CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK ||
                              chip_type == BcmChip::TRIDENT2)
            << "Un-supported BCM chip type: "
            << BcmChip::BcmChipType_Name(chip_type);
        CHECK_RETURN_IF_FALSE(bcm_port.channel() >= 1 &&
                              bcm_port.channel() <= 4)
            << "Flex-port with no channel: " << bcm_port.ShortDebugString();
        speed_bps =
            flex_chip_to_channel_to_speed[chip_type][bcm_port.channel()];
      } else {
        speed_bps = bcm_port.speed_bps();
      }
    } else if (bcm_port.type() == BcmPort::MGMT) {
      CHECK_RETURN_IF_FALSE(!bcm_port.flex_port())
          << "Mgmt ports cannot be flex.";
      speed_bps = bcm_port.speed_bps();
    } else {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Un-supported BCM port type: " << bcm_port.type() << " in "
             << bcm_port.ShortDebugString();
    }

    // Port speed and diag port setting.
    buffer << "portmap_" << bcm_port.logical_port() << "." << bcm_port.unit()
           << "=" << bcm_port.physical_port() << ":"
           << speed_bps / kBitsPerGigabit;
    if (bcm_port.flex_port() && bcm_port.serdes_lane()) {
      buffer << ":i";
    }
    buffer << std::endl;
    buffer << "dport_map_port_" << bcm_port.logical_port() << "."
           << bcm_port.unit() << "=" << bcm_port.diag_port() << std::endl;
    // Lane remapping handling.
    if (bcm_port.tx_lane_map() > 0) {
      buffer << "xgxs_tx_lane_map_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.tx_lane_map() << std::dec << std::nouppercase
             << std::endl;
    }
    if (bcm_port.rx_lane_map() > 0) {
      buffer << "xgxs_rx_lane_map_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.rx_lane_map() << std::dec << std::nouppercase
             << std::endl;
    }
    // XE ports polarity flip handling for RX and TX.
    if (bcm_port.tx_polarity_flip() > 0) {
      buffer << "phy_xaui_tx_polarity_flip_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.tx_polarity_flip() << std::dec << std::nouppercase
             << std::endl;
    }
    if (bcm_port.rx_polarity_flip() > 0) {
      buffer << "phy_xaui_rx_polarity_flip_" << bcm_port.logical_port() << "."
             << bcm_port.unit() << "=0x" << std::hex << std::uppercase
             << bcm_port.rx_polarity_flip() << std::dec << std::nouppercase
             << std::endl;
    }
    // Port-level SDK properties.
    if (bcm_port.sdk_properties_size()) {
      for (const std::string& sdk_property : bcm_port.sdk_properties()) {
        buffer << sdk_property << std::endl;
      }
    }
    buffer << std::endl;
  }

  return buffer.str();
}

::util::Status BcmSdkWrapper::InitializeSdk(
    const std::string& config_file_path,
    const std::string& config_flush_file_path,
    const std::string& bcm_shell_log_file_path) {
  // Strip out config parameters not understood by OpenNSA.
  {
    std::string config;
    std::string param = "os=unix";
    RETURN_IF_ERROR(ReadFileToString(config_file_path, &config));
    auto pos = config.find(param);
    if (pos != std::string::npos) {
      config.replace(pos, param.size(), "# " + param);
    }
    RETURN_IF_ERROR(WriteStringToFile(config, config_file_path, false));
  }

  // Initialize SDK components.
  RETURN_IF_BCM_ERROR(sal_config_file_set(config_file_path.c_str(),
                                          config_flush_file_path.c_str()));
  RETURN_IF_BCM_ERROR(sal_config_init());
  RETURN_IF_BCM_ERROR(sal_core_init());
  RETURN_IF_BCM_ERROR(sal_appl_init());
  soc_chip_info_vectors_t chip_info_vect = {bde_icid_get};
  RETURN_IF_BCM_ERROR(soc_chip_info_vect_config(&chip_info_vect));
  RETURN_IF_BCM_ERROR(bslmgmt_init());
  // TODO(max): fix, hangs forever
  // RETURN_IF_BCM_ERROR(bsl_init(&sdk_bsl_config));
  RETURN_IF_BCM_ERROR(soc_cm_init());
  RETURN_IF_BCM_ERROR(soc_knet_config(&knet_vect_bcm_knet));

  if (!bde_) {
    linux_bde_bus_t bus;
    bus.be_pio = SYS_BE_PIO;
    bus.be_packet = SYS_BE_PACKET;
    bus.be_other = SYS_BE_OTHER;
    RETURN_IF_BCM_ERROR(linux_bde_create(&bus, &bde_));
  }

  diag_init();
  cmdlist_init();

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::FindUnit(int unit, int pci_bus, int pci_slot,
                                       BcmChip::BcmChipType chip_type) {
  CHECK_RETURN_IF_FALSE(bde_)
      << "BDE not initialized yet. Call InitializeSdk() first.";

  // see: sysconf_probe()
  int num_devices = bde_->num_devices(BDE_ALL_DEVICES);
  for (int dev_num = 0; dev_num < num_devices; ++dev_num) {
    const ibde_dev_t* dev = bde_->get_dev(dev_num);
    const char* dev_name = soc_cm_get_device_name(dev->device, dev->rev);
    uint detected_pci_bus = 0, detected_pci_slot = 0, detected_pci_func = 0;
    // TODO(max): find replacement for linux_bde_get_pci_info
    // RETURN_IF_BCM_ERROR(linux_bde_get_pci_info(
    //     dev_num, &detected_pci_bus, &detected_pci_slot, &detected_pci_func));
    detected_pci_bus = pci_bus;
    detected_pci_slot = pci_slot;
    RETURN_IF_BCM_ERROR(soc_cm_device_supported(dev->device, dev->rev));
    if (detected_pci_bus == static_cast<unsigned int>(pci_bus) &&
        detected_pci_slot == static_cast<unsigned int>(pci_slot)) {
      int handle = -1;
      {
        absl::WriterMutexLock l(&data_lock_);
        // TODO(unknown): Add validation to make sure chip_type matches the
        // device we found here.
        unit_to_chip_type_[unit] = chip_type;
        unit_to_soc_device_[unit] = new BcmSocDevice();
        unit_to_soc_device_[unit]->dev_num = dev_num;
        handle = soc_cm_device_create_id(
            dev->device, dev->rev,
            reinterpret_cast<void*>(&unit_to_soc_device_[unit]->dev_num), unit);
      }
      CHECK_RETURN_IF_FALSE(handle == unit)
          << "Unit " << unit << " was not assigned to SOC device " << dev_name
          << " found on PCI bus " << pci_bus << ", PCI slot " << pci_slot
          << ". The device handle for this SOC device (" << handle
          << ") does not match the unit number.";
      LOG(INFO) << "Unit " << unit << " is assigned to SOC device " << dev_name
                << " found on PCI bus " << pci_bus << ", PCI slot " << pci_slot
                << ".";
      return ::util::OkStatus();
    }
  }

  return MAKE_ERROR(ERR_INTERNAL)
         << "Could not find any SOC device on PCI bus " << pci_bus
         << ", PCI slot " << pci_slot << ".";
}

::util::Status BcmSdkWrapper::InitializeUnit(int unit, bool warm_boot) {
  CHECK_RETURN_IF_FALSE(bde_)
      << "BDE not initialized yet. Call InitializeSdk() first.";

  // SOC device init.
  {
    absl::WriterMutexLock l(&data_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_.count(unit))
        << "Unit " << unit << " has not been assigned to any SOC device.";
    CHECK_RETURN_IF_FALSE(!unit_to_soc_device_[unit]->dev_vec)
        << "Unit " << unit << " has been already initialized.";
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_[unit]->dev_num == unit)
        << "dev_num does not match unit";
    soc_cm_device_vectors_t* dev_vec = new soc_cm_device_vectors_t();
    bde_->pci_bus_features(
        unit_to_soc_device_[unit]->dev_num, &dev_vec->big_endian_pio,
        &dev_vec->big_endian_packet, &dev_vec->big_endian_other);
    dev_vec->config_var_get = sdk_config_var_get;
    dev_vec->interrupt_connect = sdk_interrupt_connect;
    dev_vec->interrupt_disconnect = sdk_interrupt_disconnect;
    dev_vec->read = sdk_read;
    dev_vec->write = sdk_write;
    dev_vec->pci_conf_read = sdk_pci_conf_read;
    dev_vec->pci_conf_write = sdk_pci_conf_write;
    dev_vec->salloc = sdk_salloc;
    dev_vec->sfree = sdk_sfree;
    dev_vec->sinval = sdk_sinval;
    dev_vec->sflush = sdk_sflush;
    dev_vec->l2p = sdk_l2p;
    dev_vec->p2l = sdk_p2l;
    dev_vec->i2c_device_read = sdk_i2c_device_read;
    dev_vec->i2c_device_write = sdk_i2c_device_write;
    dev_vec->base_address =
        bde_->get_dev(unit_to_soc_device_[unit]->dev_num)->base_address;
    // dev_vec->bus_type = SOC_DEV_BUS_MSI | bde_->get_dev_type(unit);
    dev_vec->bus_type = bde_->get_dev_type(unit);

    // max test
    // dev_vec = new soc_cm_device_vectors_t();
    // RETURN_IF_BCM_ERROR(soc_cm_device_init(unit, dev_vec));
    //

    RETURN_IF_BCM_ERROR(soc_cm_device_init(unit, dev_vec));
    RETURN_IF_BCM_ERROR(soc_event_register(unit, sdk_event_handler, nullptr));
    unit_to_soc_device_[unit]->dev_vec = dev_vec;
    // Set MTU for all the L3 intf of this unit to the default value.
    unit_to_mtu_[unit] = kDefaultMtu;
  }

  // Finish the warm_boot or cold_boot initialization.
  if (warm_boot) {
    // Open the SDK checkpoint file in case of warmboot.
    RETURN_IF_ERROR(OpenSdkCheckpointFile(unit));
    SOC_WARM_BOOT_START(unit);
    RETURN_IF_BCM_ERROR(soc_init(unit));
    RETURN_IF_BCM_ERROR(soc_misc_init(unit));
    RETURN_IF_BCM_ERROR(soc_mmu_init(unit));
    RETURN_IF_BCM_ERROR(bcm_init(unit));
    RETURN_IF_BCM_ERROR(bcm_l2_init(unit));
    RETURN_IF_BCM_ERROR(bcm_l3_init(unit));
    RETURN_IF_BCM_ERROR(bcm_switch_control_set(unit, bcmSwitchL3EgressMode, 1));
    RETURN_IF_BCM_ERROR(
        bcm_switch_control_set(unit, bcmSwitchL3IngressInterfaceMapSet, 1));
    RETURN_IF_BCM_ERROR(bcm_stat_init(unit));
  } else {
    // Create a new SDK checkpoint file in case of coldboot.
    RETURN_IF_ERROR(CreateSdkCheckpointFile(unit));
    RETURN_IF_BCM_ERROR(soc_reset_init(unit));
    RETURN_IF_BCM_ERROR(soc_misc_init(unit));
    RETURN_IF_BCM_ERROR(soc_mmu_init(unit));
    // Workaround for OpenNSA.
    RETURN_IF_BCM_ERROR(soc_stable_size_set(unit, 1024 * 1024 * 128));
    RETURN_IF_BCM_ERROR(bcm_attach(unit, nullptr, nullptr, unit));
    RETURN_IF_BCM_ERROR(bcm_init(unit));
    RETURN_IF_BCM_ERROR(bcm_l2_init(unit));
    RETURN_IF_BCM_ERROR(bcm_l3_init(unit));
    RETURN_IF_BCM_ERROR(bcm_switch_control_set(unit, bcmSwitchL3EgressMode, 1));
    RETURN_IF_BCM_ERROR(
        bcm_switch_control_set(unit, bcmSwitchL3IngressInterfaceMapSet, 1));
    RETURN_IF_BCM_ERROR(bcm_stat_init(unit));
  }
  RETURN_IF_ERROR(CleanupKnet(unit));

  LOG(INFO) << "Unit " << unit << " initialized successfully (warm_boot: "
            << (warm_boot ? "YES" : "NO") << ").";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ShutdownUnit(int unit) {
  if (!unit_to_soc_device_.count(unit) || !unit_to_soc_device_[unit]->dev_vec) {
    return ::util::OkStatus();
  }

  // Perform all the shutdown procedures. Continue if an error happens. Also,
  // to make sure KNET keeps working while we are performing shutdown, we ignore
  // KNET hw reset during the shutdown process.
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_ERROR(status, StopLinkscan(unit));
  APPEND_STATUS_IF_BCM_ERROR(
      status, bcm_switch_event_unregister(unit, sdk_event_handler, nullptr));
  APPEND_STATUS_IF_BCM_ERROR(status, _bcm_shutdown(unit));
  APPEND_STATUS_IF_BCM_ERROR(status, soc_shutdown(unit));
  APPEND_STATUS_IF_BCM_ERROR(status, soc_cm_device_destroy(unit));

  // Remove the unit from unit_to_soc_device_ map.
  delete unit_to_soc_device_[unit];
  unit_to_soc_device_.erase(unit);

  // Remove the unit from unit_to_mtu_ map.
  unit_to_mtu_.erase(unit);  // NOOP if unit is not present for some reason.

  return status;
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
  RETURN_IF_BCM_ERROR(bcm_stk_my_modid_set(unit, module));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitializePort(int unit, int port) {
  RETURN_IF_BCM_ERROR(bcm_linkscan_mode_set(unit, port, BCM_LINKSCAN_MODE_SW));
  RETURN_IF_BCM_ERROR(bcm_port_enable_set(unit, port, 0));
  RETURN_IF_BCM_ERROR(bcm_port_stp_set(unit, port, BCM_STG_STP_BLOCK));
  RETURN_IF_BCM_ERROR(bcm_port_frame_max_set(unit, port, kDefaultMaxFrameSize));
  RETURN_IF_BCM_ERROR(bcm_port_l3_enable_set(unit, port, true));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetPortOptions(int unit, int port,
                                             const BcmPortOptions& options) {
  if (options.enabled()) {
    RETURN_IF_BCM_ERROR(bcm_port_enable_set(
        unit, port, options.enabled() == TRI_STATE_TRUE ? 1 : 0));
  }
  if (options.blocked()) {
    RETURN_IF_BCM_ERROR(bcm_port_stp_set(unit, port,
                                         options.blocked() == TRI_STATE_TRUE
                                             ? BCM_STG_STP_BLOCK
                                             : BCM_STG_STP_FORWARD));
  }
  if (options.speed_bps() > 0) {
    CHECK_RETURN_IF_FALSE(options.speed_bps() % kBitsPerMegabit == 0);
    RETURN_IF_BCM_ERROR(
        bcm_port_speed_set(unit, port, options.speed_bps() / kBitsPerMegabit));
  }
  if (options.max_frame_size() > 0) {
    CHECK_RETURN_IF_FALSE(options.max_frame_size() > 0);
    RETURN_IF_BCM_ERROR(
        bcm_port_frame_max_set(unit, port, options.max_frame_size()));
  }
  if (options.num_serdes_lanes() > 0) {
    RETURN_IF_BCM_ERROR(bcm_port_control_set(unit, port, bcmPortControlLanes,
                                             options.num_serdes_lanes()));
  }
  if (options.linkscan_mode()) {
    int mode;
    switch (options.linkscan_mode()) {
      case BcmPortOptions::LINKSCAN_MODE_SW:
        mode = BCM_LINKSCAN_MODE_SW;
        break;
      case BcmPortOptions::LINKSCAN_MODE_HW:
        mode = BCM_LINKSCAN_MODE_HW;
        break;
      default:
        mode = BCM_LINKSCAN_MODE_NONE;
        break;
    }
    RETURN_IF_BCM_ERROR(bcm_linkscan_mode_set(unit, port, mode));
  }
  if (options.autoneg()) {
    RETURN_IF_BCM_ERROR(bcm_port_autoneg_set(
        unit, port, options.autoneg() == TRI_STATE_TRUE ? 1 : 0));
  }
  if (options.loopback_mode()) {
    int mode;
    switch (options.loopback_mode()) {
      case LOOPBACK_STATE_MAC:
        mode = BCM_PORT_LOOPBACK_MAC;
        break;
      case LOOPBACK_STATE_PHY:
        mode = BCM_PORT_LOOPBACK_PHY;
        break;
      case LOOPBACK_STATE_NONE:
      default:
        mode = BCM_PORT_LOOPBACK_NONE;
    }
    RETURN_IF_BCM_ERROR(bcm_port_loopback_set(unit, port, mode));
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetPortOptions(int unit, int port,
                                             BcmPortOptions* options) {
  int speed_mbps = 0;
  RETURN_IF_BCM_ERROR(bcm_port_speed_get(unit, port, &speed_mbps));
  CHECK_RETURN_IF_FALSE(speed_mbps > 0);
  options->set_speed_bps(speed_mbps * kBitsPerMegabit);

  int loopback_mode = BCM_PORT_LOOPBACK_NONE;
  RETURN_IF_BCM_ERROR(bcm_port_loopback_get(unit, port, &loopback_mode));
  switch (loopback_mode) {
    case BCM_PORT_LOOPBACK_NONE:
      options->set_loopback_mode(LOOPBACK_STATE_NONE);
      break;
    case BCM_PORT_LOOPBACK_MAC:
      options->set_loopback_mode(LOOPBACK_STATE_MAC);
      break;
    case BCM_PORT_LOOPBACK_PHY:
      options->set_loopback_mode(LOOPBACK_STATE_PHY);
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Unknown loopback mode " << loopback_mode;
  }

  // TODO(unknown): Return the rest of the port options.

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetPortCounters(int unit, int port,
                                              PortCounters* pc) {
  CHECK_RETURN_IF_FALSE(pc);
  pc->Clear();
  uint64 val;
  // in
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInOctets, &val));
  pc->set_in_octets(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInUcastPkts, &val));
  pc->set_in_unicast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInMulticastPkts, &val));
  pc->set_in_multicast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInBroadcastPkts, &val));
  pc->set_in_broadcast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInDiscards, &val));
  pc->set_in_discards(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInErrors, &val));
  pc->set_in_errors(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfInUnknownProtos, &val));
  pc->set_in_unknown_protos(val);
  // out
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutOctets, &val));
  pc->set_out_octets(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutUcastPkts, &val));
  pc->set_out_unicast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutMulticastPkts, &val));
  pc->set_out_multicast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutBroadcastPkts, &val));
  pc->set_out_broadcast_pkts(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutDiscards, &val));
  pc->set_out_discards(val);
  RETURN_IF_BCM_ERROR(bcm_stat_get(unit, port, snmpIfOutErrors, &val));
  pc->set_out_errors(val);

  VLOG(2) << "Port counter from port " << port << ":\n" << pc->DebugString();

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartDiagShellServer() {
  if (bcm_diag_shell_ == nullptr) return ::util::OkStatus();  // sim mode

  std::thread t([]() {
    // BCM CLI installs its own signal handler for SIGINT,
    // we have to restore the HAL one afterwards
    sighandler_t h = signal(SIGINT, SIG_IGN);
    sh_process(-1, "BCM", TRUE);
    signal(SIGINT, h);
  });
  t.detach();

  // RETURN_IF_ERROR(bcm_diag_shell_->StartServer());

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartLinkscan(int unit) {
  int usec;
  RETURN_IF_BCM_ERROR(bcm_linkscan_enable_get(unit, &usec));
  if (usec > 0) {
    // linkscan already enabled.
    LOG(INFO) << "Linkscan already enabled for unit " << unit << ".";
    return ::util::OkStatus();
  }

  RETURN_IF_BCM_ERROR(bcm_linkscan_register(unit, sdk_linkscan_callback));
  RETURN_IF_BCM_ERROR(
      bcm_linkscan_enable_set(unit, FLAGS_linkscan_interval_in_usec));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopLinkscan(int unit) {
  RETURN_IF_BCM_ERROR(bcm_linkscan_enable_set(unit, 0));
  RETURN_IF_BCM_ERROR(bcm_linkscan_unregister(unit, sdk_linkscan_callback));

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
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "not implemented";
}

::util::Status BcmSdkWrapper::SetMtu(int unit, int mtu) {
  absl::WriterMutexLock l(&data_lock_);
  CHECK_RETURN_IF_FALSE(unit_to_mtu_.count(unit));
  // TODO(unknown): Modify mtu for all the interfaces on this unit.
  unit_to_mtu_[unit] = mtu;

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3RouterIntf(int unit,
                                                              uint64 router_mac,
                                                              int vlan) {
  int mtu = 0;
  {
    absl::ReaderMutexLock l(&data_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_mtu_.count(unit));
    mtu = unit_to_mtu_[unit];
  }
  CHECK_RETURN_IF_FALSE(router_mac);
  bcm_l3_intf_t l3_intf;
  bcm_l3_intf_t_init(&l3_intf);
  l3_intf.l3a_vid = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  l3_intf.l3a_ttl = 0xff;
  l3_intf.l3a_mtu = mtu;
  Uint64ToBcmMac(router_mac, &l3_intf.l3a_mac_addr);
  RETURN_IF_BCM_ERROR(FindOrCreateL3RouterIntfHelper(unit, &l3_intf));
  CHECK_RETURN_IF_FALSE(l3_intf.l3a_intf_id > 0);

  return l3_intf.l3a_intf_id;
}

::util::Status BcmSdkWrapper::DeleteL3RouterIntf(int unit, int router_intf_id) {
  bcm_l3_intf_t l3_intf;
  bcm_l3_intf_t_init(&l3_intf);
  l3_intf.l3a_intf_id = router_intf_id;
  RETURN_IF_BCM_ERROR(bcm_l3_intf_delete(unit, &l3_intf));

  VLOG(1) << "Router intf with ID " << router_intf_id << " deleted on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3CpuEgressIntf(int unit) {
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  // We dont care about nexthop_mac, port, vlan, and router_intf_id in this
  // case. After BCM_L3_L2TOCPU is set, all the packets will be trapped to
  // CPU and skip the L3 modification.
  Uint64ToBcmMac(0ULL, &l3_egress.mac_addr);
  l3_egress.flags |= BCM_L3_L2TOCPU;
  int egress_intf_id = 0;
  RETURN_IF_BCM_ERROR(
      FindOrCreateL3EgressIntfHelper(unit, &l3_egress, &egress_intf_id));
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);

  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3PortEgressIntf(
    int unit, uint64 nexthop_mac, int port, int vlan, int router_intf_id) {
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  Uint64ToBcmMac(nexthop_mac, &l3_egress.mac_addr);
  RETURN_IF_BCM_ERROR(bcm_port_gport_get(unit, port, &l3_egress.port));
  l3_egress.module = 0;
  l3_egress.vlan = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  l3_egress.intf = router_intf_id;
  l3_egress.flags |= BCM_L3_KEEP_VLAN;  // VLAN hashing enabled by default.
  int egress_intf_id = 0;
  RETURN_IF_BCM_ERROR(
      FindOrCreateL3EgressIntfHelper(unit, &l3_egress, &egress_intf_id));
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);

  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3TrunkEgressIntf(
    int unit, uint64 nexthop_mac, int trunk, int vlan, int router_intf_id) {
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  Uint64ToBcmMac(nexthop_mac, &l3_egress.mac_addr);
  l3_egress.trunk = trunk;
  l3_egress.vlan = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  l3_egress.intf = router_intf_id;
  l3_egress.flags |= BCM_L3_KEEP_VLAN;  // VLAN hashing enabled by default.
  l3_egress.flags |= BCM_L3_TGID;
  int egress_intf_id = 0;
  RETURN_IF_BCM_ERROR(
      FindOrCreateL3EgressIntfHelper(unit, &l3_egress, &egress_intf_id));
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);

  return egress_intf_id;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3DropIntf(int unit) {
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  // We dont care about nexthop_mac, port, vlan, and router_intf_id in this
  // case. BCM_L3_DST_DISCARD flag discards all the packets.
  Uint64ToBcmMac(0ULL, &l3_egress.mac_addr);
  l3_egress.flags |= BCM_L3_DST_DISCARD;  // Drop the packets.
  int egress_intf_id = 0;
  RETURN_IF_BCM_ERROR(
      FindOrCreateL3EgressIntfHelper(unit, &l3_egress, &egress_intf_id));
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);

  return egress_intf_id;
}

::util::Status BcmSdkWrapper::ModifyL3CpuEgressIntf(int unit,
                                                    int egress_intf_id) {
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  // We dont care about nexthop_mac, port, vlan, and router_intf_id in this
  // case. After BCM_L3_L2TOCPU is set, all the packets will be trapped to
  // CPU and skip the L3 modification.
  Uint64ToBcmMac(0ULL, &l3_egress.mac_addr);
  l3_egress.flags |= BCM_L3_L2TOCPU;
  RETURN_IF_BCM_ERROR(
      ModifyL3EgressIntfHelper(unit, egress_intf_id, &l3_egress));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3PortEgressIntf(int unit,
                                                     int egress_intf_id,
                                                     uint64 nexthop_mac,
                                                     int port, int vlan,
                                                     int router_intf_id) {
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  Uint64ToBcmMac(nexthop_mac, &l3_egress.mac_addr);
  RETURN_IF_BCM_ERROR(bcm_port_gport_get(unit, port, &l3_egress.port));
  l3_egress.module = 0;
  l3_egress.vlan = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  l3_egress.intf = router_intf_id;
  l3_egress.flags |= BCM_L3_KEEP_VLAN;  // VLAN hashing enabled by default.
  RETURN_IF_BCM_ERROR(
      ModifyL3EgressIntfHelper(unit, egress_intf_id, &l3_egress));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3TrunkEgressIntf(int unit,
                                                      int egress_intf_id,
                                                      uint64 nexthop_mac,
                                                      int trunk, int vlan,
                                                      int router_intf_id) {
  CHECK_RETURN_IF_FALSE(nexthop_mac);
  CHECK_RETURN_IF_FALSE(router_intf_id > 0);
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  Uint64ToBcmMac(nexthop_mac, &l3_egress.mac_addr);
  l3_egress.trunk = trunk;
  l3_egress.vlan = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  l3_egress.intf = router_intf_id;
  l3_egress.flags |= BCM_L3_KEEP_VLAN;  // VLAN hashing enabled by default.
  l3_egress.flags |= BCM_L3_TGID;
  RETURN_IF_BCM_ERROR(
      ModifyL3EgressIntfHelper(unit, egress_intf_id, &l3_egress));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3DropIntf(int unit, int egress_intf_id) {
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  // We dont care about nexthop_mac, port, vlan, and router_intf_id in this
  // case. BCM_L3_DST_DISCARD flag discards all the packets.
  Uint64ToBcmMac(0ULL, &l3_egress.mac_addr);
  l3_egress.port = 0;
  l3_egress.module = 0;
  l3_egress.flags |= BCM_L3_DST_DISCARD;  // Drop the packets.
  RETURN_IF_BCM_ERROR(
      ModifyL3EgressIntfHelper(unit, egress_intf_id, &l3_egress));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3EgressIntf(int unit, int egress_intf_id) {
  RETURN_IF_BCM_ERROR(bcm_l3_egress_destroy(unit, egress_intf_id));

  VLOG(1) << "Egress intf with ID " << egress_intf_id << " deleted on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindRouterIntfFromEgressIntf(
    int unit, int egress_intf_id) {
  bcm_l3_egress_t l3_egress;
  bcm_l3_egress_t_init(&l3_egress);
  RETURN_IF_BCM_ERROR(bcm_l3_egress_get(unit, egress_intf_id, &l3_egress));

  return l3_egress.intf;
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateEcmpEgressIntf(
    int unit, const std::vector<int>& member_ids) {
  int members_array[kMaxEcmpGroupSize];
  for (size_t i = 0; i < member_ids.size(); ++i) {
    members_array[i] = member_ids[i];
  }
  int members_count = static_cast<int>(member_ids.size());
  bcm_l3_egress_ecmp_t l3_egress_ecmp;
  bcm_l3_egress_ecmp_t_init(&l3_egress_ecmp);
  l3_egress_ecmp.max_paths = members_count;
  RETURN_IF_BCM_ERROR(FindOrCreateEcmpEgressIntfHelper(
      unit, &l3_egress_ecmp, members_count, members_array));
  CHECK_RETURN_IF_FALSE(l3_egress_ecmp.ecmp_intf > 0);

  return l3_egress_ecmp.ecmp_intf;
}

::util::Status BcmSdkWrapper::ModifyEcmpEgressIntf(
    int unit, int egress_intf_id, const std::vector<int>& member_ids) {
  int members_array[kMaxEcmpGroupSize];
  for (size_t i = 0; i < member_ids.size(); ++i) {
    members_array[i] = member_ids[i];
  }
  int members_count = static_cast<int>(member_ids.size());
  bcm_l3_egress_ecmp_t l3_egress_ecmp;
  bcm_l3_egress_ecmp_t_init(&l3_egress_ecmp);
  l3_egress_ecmp.max_paths = members_count;
  l3_egress_ecmp.ecmp_intf = egress_intf_id;

  RETURN_IF_BCM_ERROR(ModifyEcmpEgressIntfHelper(unit, &l3_egress_ecmp,
                                                 members_count, members_array));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteEcmpEgressIntf(int unit,
                                                   int egress_intf_id) {
  bcm_l3_egress_ecmp_t l3_egress_ecmp;
  bcm_l3_egress_ecmp_t_init(&l3_egress_ecmp);
  l3_egress_ecmp.ecmp_intf = egress_intf_id;
  RETURN_IF_BCM_ERROR(bcm_l3_egress_ecmp_destroy(unit, &l3_egress_ecmp));

  VLOG(1) << "ECMP group with ID " << egress_intf_id << " deleted on unit "
          << unit << ".";

  return ::util::OkStatus();
}

namespace {

void PopulateL3RouteKeyIpv4(int vrf, uint32 subnet, uint32 mask,
                            bcm_l3_route_t* route) {
  route->l3a_vrf = ControllerVrfToBcmVrf(vrf);
  route->l3a_subnet = subnet;
  route->l3a_ip_mask = !subnet ? 0 : (mask ? mask : 0xffffffff);
}

void PopulateL3RouteKeyIpv6(int vrf, const std::string& subnet,
                            const std::string& mask, bcm_l3_route_t* route) {
  route->l3a_flags |= BCM_L3_IP6;  // IPv6
  route->l3a_vrf = ControllerVrfToBcmVrf(vrf);
  if (subnet.empty()) {
    memset(route->l3a_ip6_net, 0, BCM_IP6_ADDRLEN);
  } else {
    memcpy(route->l3a_ip6_net, subnet.data(), BCM_IP6_ADDRLEN);
  }
  if (subnet.empty()) {
    memset(route->l3a_ip6_mask, 0, BCM_IP6_ADDRLEN);
  } else if (mask.empty()) {
    memset(route->l3a_ip6_mask, 0xff, BCM_IP6_ADDRLEN);
  } else {
    memcpy(route->l3a_ip6_mask, mask.data(), BCM_IP6_ADDRLEN);
  }
}

void PopulateL3HostKeyIpv4(int vrf, uint32 ipv4, bcm_l3_host_t* host) {
  host->l3a_vrf = ControllerVrfToBcmVrf(vrf);
  host->l3a_ip_addr = ipv4;
}

void PopulateL3HostKeyIpv6(int vrf, const std::string& ipv6,
                           bcm_l3_host_t* host) {
  host->l3a_flags |= BCM_L3_IP6;  // IPv6
  host->l3a_vrf = ControllerVrfToBcmVrf(vrf);
  if (ipv6.empty()) {
    memset(host->l3a_ip6_addr, 0, BCM_IP6_ADDRLEN);
  } else {
    memcpy(host->l3a_ip6_addr, ipv6.data(), BCM_IP6_ADDRLEN);
  }
}

void PopulateL3RouteAction(int class_id, int egress_intf_id,
                           bool is_intf_multipath, bcm_l3_route_t* route) {
  if (is_intf_multipath) route->l3a_flags |= BCM_L3_MULTIPATH;
  if (class_id > 0) route->l3a_lookup_class = class_id;
  route->l3a_intf = egress_intf_id;
}

void PopulateL3HostAction(int class_id, int egress_intf_id,
                          bcm_l3_host_t* host) {
  if (class_id > 0) host->l3a_lookup_class = class_id;
  host->l3a_intf = egress_intf_id;
}

}  // namespace

::util::Status BcmSdkWrapper::AddL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                             uint32 mask, int class_id,
                                             int egress_intf_id,
                                             bool is_intf_multipath) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv4(vrf, subnet, mask, &route);
  PopulateL3RouteAction(class_id, egress_intf_id, is_intf_multipath, &route);
  // Since route.l3a_flags & BCM_L3_REPLACE = 0, we expect an error if the route
  // already exists.
  RETURN_IF_BCM_ERROR(bcm_l3_route_add(unit, &route));

  VLOG(1) << "Added IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3RouteIpv6(int unit, int vrf,
                                             const std::string& subnet,
                                             const std::string& mask,
                                             int class_id, int egress_intf_id,
                                             bool is_intf_multipath) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv6(vrf, subnet, mask, &route);
  PopulateL3RouteAction(class_id, egress_intf_id, is_intf_multipath, &route);
  // Since route.l3a_flags & BCM_L3_REPLACE = 0, we expect an error if the route
  // already exists.
  RETURN_IF_BCM_ERROR(bcm_l3_route_add(unit, &route));

  VLOG(1) << "Added IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                            int class_id, int egress_intf_id) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv4(vrf, ipv4, &host);
  PopulateL3HostAction(class_id, egress_intf_id, &host);
  // Since host.l3a_flags & BCM_L3_REPLACE = 0, we expect an error if the host
  // already exists.
  RETURN_IF_BCM_ERROR(bcm_l3_host_add(unit, &host));

  VLOG(1) << "Added IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv6(int unit, int vrf,
                                            const std::string& ipv6,
                                            int class_id, int egress_intf_id) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv6(vrf, ipv6, &host);
  PopulateL3HostAction(class_id, egress_intf_id, &host);
  // Since host.l3a_flags & BCM_L3_REPLACE = 0, we expect an error if the host
  // already exists.
  RETURN_IF_BCM_ERROR(bcm_l3_host_add(unit, &host));

  VLOG(1) << "Added IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask,
                                                int class_id,
                                                int egress_intf_id,
                                                bool is_intf_multipath) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv4(vrf, subnet, mask, &route);
  route.l3a_flags |= BCM_L3_REPLACE;
  PopulateL3RouteAction(class_id, egress_intf_id, is_intf_multipath, &route);
  // Since route.l3a_flags & BCM_L3_REPLACE != 0, we expect an error if the
  // route does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_route_add(unit, &route));

  VLOG(1) << "Modify IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv6(
    int unit, int vrf, const std::string& subnet, const std::string& mask,
    int class_id, int egress_intf_id, bool is_intf_multipath) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv6(vrf, subnet, mask, &route);
  route.l3a_flags |= BCM_L3_REPLACE;
  PopulateL3RouteAction(class_id, egress_intf_id, is_intf_multipath, &route);
  // Since route.l3a_flags & BCM_L3_REPLACE != 0, we expect an error if the
  // route does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_route_add(unit, &route));

  VLOG(1) << "Modify IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                               int class_id,
                                               int egress_intf_id) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv4(vrf, ipv4, &host);
  host.l3a_flags |= BCM_L3_REPLACE;
  PopulateL3HostAction(class_id, egress_intf_id, &host);
  // Since host.l3a_flags & BCM_L3_REPLACE != 0, we expect an error if the
  // host does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_host_add(unit, &host));

  VLOG(1) << "Modify IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6,
                                               int class_id,
                                               int egress_intf_id) {
  CHECK_RETURN_IF_FALSE(egress_intf_id > 0);
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv6(vrf, ipv6, &host);
  RETURN_IF_BCM_ERROR(bcm_l3_host_find(unit, &host));
  host.l3a_flags |= BCM_L3_REPLACE;
  PopulateL3HostAction(class_id, egress_intf_id, &host);
  // Since host.l3a_flags & BCM_L3_REPLACE != 0, we expect an error if the
  // host does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_host_add(unit, &host));

  VLOG(1) << "Modify IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask) {
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv4(vrf, subnet, mask, &route);
  // Will return an error if the route does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_route_delete(unit, &route));

  VLOG(1) << "Deleted IPv4 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv6(int unit, int vrf,
                                                const std::string& subnet,
                                                const std::string& mask) {
  bcm_l3_route_t route;
  bcm_l3_route_t_init(&route);
  PopulateL3RouteKeyIpv6(vrf, subnet, mask, &route);
  // Will return an error if the route does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_route_delete(unit, &route));

  VLOG(1) << "Deleted IPv6 L3 LPM route " << PrintL3Route(route) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv4(int unit, int vrf, uint32 ipv4) {
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv4(vrf, ipv4, &host);
  // Will return an error if the host does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_host_delete(unit, &host));

  VLOG(1) << "Deleted IPv4 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6) {
  bcm_l3_host_t host;
  bcm_l3_host_t_init(&host);
  PopulateL3HostKeyIpv6(vrf, ipv6, &host);
  // Will return an error if the host does not exist.
  RETURN_IF_BCM_ERROR(bcm_l3_host_delete(unit, &host));

  VLOG(1) << "Deleted IPv6 L3 host route " << PrintL3Host(host) << " on unit "
          << unit << ".";

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::AddMyStationEntry(int unit, int priority,
                                                       int vlan, int vlan_mask,
                                                       uint64 dst_mac,
                                                       uint64 dst_mac_mask) {
  bcm_l2_station_t l2_station;
  bcm_l2_station_t_init(&l2_station);
  l2_station.flags = BCM_L2_STATION_IPV4 | BCM_L2_STATION_IPV6;
  l2_station.priority = priority;
  if (vlan > 0) {
    // A specific VLAN is specified.
    l2_station.vlan = vlan;
    l2_station.vlan_mask = vlan_mask;
  } else {
    // Any VLAN is OK.
    l2_station.vlan = 0;
    l2_station.vlan_mask = 0;
  }
  if (dst_mac > 0) {
    // A specific dst MAC is specified.
    Uint64ToBcmMac(dst_mac, &l2_station.dst_mac);
    Uint64ToBcmMac(dst_mac_mask, &l2_station.dst_mac_mask);
  } else {
    // Any dst_mac is OK.
    Uint64ToBcmMac(0ULL, &l2_station.dst_mac);
    Uint64ToBcmMac(1ULL, &l2_station.dst_mac_mask);
  }
  int station_id = -1;
  RETURN_IF_BCM_ERROR(bcm_l2_station_add(unit, &station_id, &l2_station));
  CHECK_RETURN_IF_FALSE(station_id > 0);

  VLOG(1) << "Added dst MAC " << BcmMacToStr(l2_station.dst_mac) << " & VLAN "
          << vlan << " to my station TCAM with priority " << priority
          << " on unit " << unit << ".";

  return station_id;
}

::util::Status BcmSdkWrapper::DeleteMyStationEntry(int unit, int station_id) {
  RETURN_IF_BCM_ERROR(bcm_l2_station_delete(unit, station_id));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2EntriesByVlan(int unit, int vlan) {
  RETURN_IF_BCM_ERROR(bcm_l2_addr_delete_by_vlan(unit, vlan, /*flags=*/0));
  VLOG(1) << "Removed all L2 entries for VLAN " << vlan << " on unit " << unit
          << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL2Entry(int unit, int vlan, uint64 dst_mac,
                                         int logical_port, int trunk_port,
                                         int l2_mcast_group_id, int class_id,
                                         bool copy_to_cpu, bool dst_drop) {
  // TODO(max): Apply all remaining parameters.
  bcm_l2_addr_t l2_addr;
  bcm_mac_t bcm_mac;
  Uint64ToBcmMac(dst_mac, &bcm_mac);
  bcm_l2_addr_t_init(&l2_addr, bcm_mac, vlan);
  l2_addr.port = logical_port;

  RETURN_IF_BCM_ERROR(bcm_l2_addr_add(unit, &l2_addr));

  VLOG(1) << "Added L2 unicast entry "
          << " to .. on unit " << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2Entry(int unit, int vlan,
                                            uint64 dst_mac) {
  bcm_mac_t bcm_mac;
  Uint64ToBcmMac(dst_mac, &bcm_mac);
  RETURN_IF_BCM_ERROR(bcm_l2_addr_delete(unit, bcm_mac, vlan));

  VLOG(1) << "Removed L2 unicast to "
          << " ... on unit " << unit << ".";
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL2MulticastEntry(
    int unit, int priority, int vlan, int vlan_mask, uint64 dst_mac,
    uint64 dst_mac_mask, bool copy_to_cpu, bool drop, uint8 l2_mcast_group_id) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "not implemented";
}

::util::Status BcmSdkWrapper::DeleteL2MulticastEntry(int unit, int vlan,
                                                     int vlan_mask,
                                                     uint64 dst_mac,
                                                     uint64 dst_mac_mask) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "not implemented";
}

::util::Status BcmSdkWrapper::DeleteVlanIfFound(int unit, int vlan) {
  // TODO(unknown): Will we need to remove the ports from VLAN first? Most
  // probably not, but make sure.
  RETURN_IF_BCM_ERROR(bcm_vlan_destroy(unit, vlan));
  VLOG(1) << "Removed VLAN " << vlan << " from unit " << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddVlanIfNotFound(int unit, int vlan) {
  int retval = bcm_vlan_create(unit, vlan);
  if (retval == BCM_E_EXISTS) {
    VLOG(1) << "VLAN " << vlan << " already exists on unit " << unit << ".";
    return ::util::OkStatus();
  }
  if (BCM_FAILURE(retval)) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Failed to create VLAN " << vlan << " on unit " << unit << ".";
  }

  bcm_port_config_t port_cfg;
  RETURN_IF_BCM_ERROR(bcm_port_config_get(unit, &port_cfg));
  RETURN_IF_BCM_ERROR(
      bcm_vlan_port_add(unit, vlan, port_cfg.all, port_cfg.all));

  VLOG(1) << "Added VLAN " << vlan << " on unit " << unit << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureVlanBlock(int unit, int vlan,
                                                 bool block_broadcast,
                                                 bool block_known_multicast,
                                                 bool block_unknown_multicast,
                                                 bool block_unknown_unicast) {
  bcm_pbmp_t vlan_ports;
  RETURN_IF_BCM_ERROR(bcm_vlan_port_get(unit, vlan, &vlan_ports, nullptr));
  bcm_vlan_block_t block;
  bcm_vlan_block_t_init(&block);
  if (block_broadcast) {
    BCM_PBMP_ASSIGN(block.broadcast, vlan_ports);
  }
  if (block_known_multicast) {
    BCM_PBMP_ASSIGN(block.known_multicast, vlan_ports);
  }
  if (block_unknown_multicast) {
    BCM_PBMP_ASSIGN(block.unknown_multicast, vlan_ports);
  }
  if (block_unknown_unicast) {
    BCM_PBMP_ASSIGN(block.unknown_unicast, vlan_ports);
  }
  RETURN_IF_BCM_ERROR(bcm_vlan_block_set(unit, vlan, &block));

  VLOG(1) << "Configured block on VLAN " << vlan << " on unit " << unit
          << ". block_broadcast: " << block_broadcast
          << ", block_known_multicast: " << block_known_multicast
          << ", block_unknown_multicast: " << block_unknown_multicast
          << ", block_unknown_unicast: " << block_unknown_unicast << ".";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureL2Learning(int unit, int vlan,
                                                  bool disable_l2_learning) {
  bcm_vlan_control_vlan_t vlan_control;
  bcm_vlan_control_vlan_t_init(&vlan_control);
  RETURN_IF_BCM_ERROR(bcm_vlan_control_vlan_get(unit, vlan, &vlan_control));
  if (disable_l2_learning) {
    vlan_control.flags |= BCM_VLAN_LEARN_DISABLE;
  } else {
    vlan_control.flags &= ~BCM_VLAN_LEARN_DISABLE;
  }
  RETURN_IF_BCM_ERROR(bcm_vlan_control_vlan_set(unit, vlan, vlan_control));

  VLOG(1) << "L2 learning for VLAN " << vlan << " on unit " << unit
          << (disable_l2_learning ? " disabled." : " enabled.");

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetL2AgeTimer(int unit, int l2_age_duration_sec) {
  RETURN_IF_BCM_ERROR(bcm_l2_age_timer_set(unit, l2_age_duration_sec));
  VLOG(1) << "L2 aging duration on unit " << unit << " set to "
          << l2_age_duration_sec << " secs.";

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigSerdesForPort(
    int unit, int port, uint64 speed_bps, int serdes_core, int serdes_lane,
    int serdes_num_lanes, const std::string& intf_type,
    const SerdesRegisterConfigs& serdes_register_configs,
    const SerdesAttrConfigs& serdes_attr_configs) {
  CHECK_RETURN_IF_FALSE(!intf_type.empty());
  ASSIGN_OR_RETURN(auto chip_type, GetChipType(unit));
  CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK ||
                        chip_type == BcmChip::TOMAHAWK_PLUS ||
                        chip_type == BcmChip::TRIDENT2)
      << "Un-supported BCM chip type: " << BcmChip::BcmChipType_Name(chip_type);

  // First disable linkscan and the port. But first save the state to be able
  // to recover at the end of the function.
  int linkscan_mode = 0, enable = 0;
  RETURN_IF_BCM_ERROR(bcm_linkscan_mode_get(unit, port, &linkscan_mode));
  RETURN_IF_BCM_ERROR(bcm_port_enable_get(unit, port, &enable));

  // From this point forward, we need to make sure we recover linkscan mode
  // and/or enable if there was an issue.
  ::util::Status status = ::util::OkStatus();
  APPEND_STATUS_IF_BCM_ERROR(
      status, bcm_linkscan_mode_set(unit, port, BCM_LINKSCAN_MODE_NONE));
  if (!status.ok()) return status;
  APPEND_STATUS_IF_BCM_ERROR(status, bcm_port_enable_set(unit, port, 0));
  if (!status.ok()) {
    APPEND_STATUS_IF_BCM_ERROR(
        status, bcm_linkscan_mode_set(unit, port, linkscan_mode));
    return status;
  }

  // Set interface and configure Phy based on the chip_type and intf_type.
  status = SetIntfAndConfigurePhyForPort(unit, port, chip_type, speed_bps,
                                         intf_type);

  // Apply the serdes register settings. In the input of this function
  // serdes_register_configs and serdes_attr_configs both have the
  // same values for all the lanes, in case we needed to set the config per
  // lane. However we do not need to do so for the TH and T2 based platfroms.
  // We just pick the first lane.
  // TODO(unknown): If we ever try to support T+, revisit this part.
  for (const auto& e : serdes_register_configs) {
    if (!status.ok()) break;
    status = SetSerdesRegisterForPort(unit, port, chip_type, serdes_lane,
                                      e.first, e.second);
  }
  for (const auto& e : serdes_attr_configs) {
    if (!status.ok()) break;
    status =
        SetSerdesAttributeForPort(unit, port, chip_type, e.first, e.second);
  }

  // Recover state before we exit, even if we had an error.
  APPEND_STATUS_IF_BCM_ERROR(status,
                             bcm_linkscan_mode_set(unit, port, linkscan_mode));
  APPEND_STATUS_IF_BCM_ERROR(status, bcm_port_enable_set(unit, port, enable));

  return status;
}

::util::Status BcmSdkWrapper::CreateKnetIntf(int unit, int vlan,
                                             std::string* netif_name,
                                             int* netif_id) {
  CHECK_RETURN_IF_FALSE(netif_name != nullptr && netif_id != nullptr)
      << "Null netif_name or netif_id pointers.";
  CHECK_RETURN_IF_FALSE(!netif_name->empty())
      << "Empty netif name for unit " << unit << ".";
  CHECK_RETURN_IF_FALSE(netif_name->length() <= static_cast<size_t>(IFNAMSIZ) &&
                        netif_name->length() <=
                            static_cast<size_t>(BCM_KNET_NETIF_NAME_MAX))
      << "Oversize netif name for unit " << unit << ": " << *netif_name << ".";
  bcm_knet_netif_t netif;
  bcm_knet_netif_t_init(&netif);
  strncpy(netif.name, netif_name->c_str(), BCM_KNET_NETIF_NAME_MAX);
  netif.type = BCM_KNET_NETIF_T_TX_META_DATA;
  netif.flags |= BCM_KNET_NETIF_F_RCPU_ENCAP;
  netif.vlan = vlan > 0 ? vlan : BCM_VLAN_DEFAULT;
  RETURN_IF_BCM_ERROR(bcm_knet_netif_create(unit, &netif));
  *netif_id = netif.id;
  *netif_name = netif.name;

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DestroyKnetIntf(int unit, int netif_id) {
  RETURN_IF_BCM_ERROR(bcm_knet_netif_destroy(unit, netif_id));

  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::CreateKnetFilter(int unit, int netif_id,
                                                      KnetFilterType type) {
  bcm_knet_filter_t filter;
  bcm_knet_filter_t_init(&filter);
  filter.type = BCM_KNET_FILTER_T_RX_PKT;
  filter.dest_type = BCM_KNET_DEST_T_NETIF;
  filter.dest_id = netif_id;
  switch (type) {
    case KnetFilterType::CATCH_NON_SFLOW_FP_MATCH:
      // Send all the non-sflow packets which match an FP rule to controller.
      filter.priority = 0;  // hardcoded. Highest priority.
      snprintf(filter.desc, sizeof(filter.desc), "CATCH_NON_SFLOW_FP_MATCH");
      filter.m_fp_rule = 1;  // This is a cookie we use for all the FP rules
                             // that send packets to CPU
      BCM_RX_REASON_SET(filter.m_reason, bcmRxReasonFilterMatch);
      filter.match_flags |= BCM_KNET_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_SFLOW_FROM_INGRESS_PORT:
      // Send all ingress-sampled sflow packets to sflow agent.
      filter.priority = 2;  // hardcoded. Cannot use 1. 1 is reserved.
      snprintf(filter.desc, sizeof(filter.desc),
               "CATCH_SFLOW_FROM_INGRESS_PORT");
      BCM_RX_REASON_SET(filter.m_reason, bcmRxReasonSampleSource);
      filter.match_flags |= BCM_KNET_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_SFLOW_FROM_EGRESS_PORT:
      // Send all egress-sampled sflow packets to sflow agent.
      filter.priority = 3;  // hardcoded. Cannot use 1. 1 is reserved.
      snprintf(filter.desc, sizeof(filter.desc),
               "CATCH_SFLOW_FROM_EGRESS_PORT");
      BCM_RX_REASON_SET(filter.m_reason, bcmRxReasonSampleDest);
      filter.match_flags |= BCM_KNET_FILTER_M_REASON;
      break;
    case KnetFilterType::CATCH_ALL:
      filter.priority = 10;  // hardcoded. Lowest priority.
      snprintf(filter.desc, sizeof(filter.desc), "CATCH_ALL");
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Un-supported KNET filter type.";
  }

  RETURN_IF_BCM_ERROR(bcm_knet_filter_create(unit, &filter));
  return filter.id;
}

::util::Status BcmSdkWrapper::DestroyKnetFilter(int unit, int filter_id) {
  RETURN_IF_BCM_ERROR(bcm_knet_filter_destroy(unit, filter_id));

  return ::util::OkStatus();
}

namespace {

int CanonicalRate(int rate) { return rate > 0 ? rate : BCM_RX_RATE_NOLIMIT; }

}  // namespace

::util::Status BcmSdkWrapper::StartRx(int unit, const RxConfig& rx_config) {
  // Sanity checking.
  CHECK_RETURN_IF_FALSE(rx_config.rx_pool_pkt_count > 0);
  CHECK_RETURN_IF_FALSE(rx_config.rx_pool_bytes_per_pkt > 0);
  CHECK_RETURN_IF_FALSE(rx_config.max_pkt_size_bytes > 0);
  CHECK_RETURN_IF_FALSE(rx_config.pkts_per_chain > 0);
  CHECK_RETURN_IF_FALSE(!rx_config.dma_channel_configs.empty());
  for (const auto& e : rx_config.dma_channel_configs) {
    CHECK_RETURN_IF_FALSE(e.first <= BCM_RX_CHANNELS);
    CHECK_RETURN_IF_FALSE(e.second.chains > 0);
    CHECK_RETURN_IF_FALSE(!e.second.cos_set.empty());
    for (int c : e.second.cos_set) {
      CHECK_RETURN_IF_FALSE(c <= 48);  // Maximum number of cos values
    }
  }

  // Init RX.
  RETURN_IF_BCM_ERROR(bcm_rx_init(unit));

  // Rx pool setup. Override the default done by bcm_rx_init.
  RETURN_IF_BCM_ERROR(bcm_rx_pool_cleanup());
  RETURN_IF_BCM_ERROR(bcm_rx_pool_setup(rx_config.rx_pool_pkt_count,
                                        rx_config.rx_pool_bytes_per_pkt));

  // Configure and start RX.
  bcm_rx_cfg_t rx_cfg;
  bcm_rx_cfg_t_init(&rx_cfg);
  rx_cfg.pkt_size = rx_config.max_pkt_size_bytes;
  rx_cfg.pkts_per_chain = rx_config.pkts_per_chain;
  rx_cfg.global_pps = CanonicalRate(rx_config.max_rate_pps);
  rx_cfg.max_burst = CanonicalRate(rx_config.max_burst_pkts);
  for (const auto& e : rx_config.dma_channel_configs) {
    rx_cfg.chan_cfg[e.first].chains = e.second.chains;
    rx_cfg.chan_cfg[e.first].cos_bmp = 0;
    for (int c : e.second.cos_set) {
      rx_cfg.chan_cfg[e.first].cos_bmp |= 1 << c;
    }
    rx_cfg.chan_cfg[e.first].flags = 0;
    if (e.second.strip_crc) {
      rx_cfg.chan_cfg[e.first].flags |= BCM_RX_F_CRC_STRIP;
    }
    if (e.second.strip_vlan) {
      rx_cfg.chan_cfg[e.first].flags |= BCM_RX_F_VTAG_STRIP;
    }
    if (e.second.oversized_packets_ok) {
      rx_cfg.chan_cfg[e.first].flags |= BCM_RX_F_OVERSIZED_OK;
    }
    if (e.second.no_pkt_parsing) {
      rx_cfg.chan_cfg[e.first].flags |= BCM_RX_F_PKT_UNPARSED;
    }
  }
  RETURN_IF_BCM_ERROR(bcm_rx_start(unit, &rx_cfg));

  // Apply the rest of DMA channel configs, not done in bcm_rx_start.
  for (const auto& e : rx_config.dma_channel_configs) {
    for (int c : e.second.cos_set) {
      RETURN_IF_BCM_ERROR(bcm_rx_queue_channel_set(unit, c, e.first));
    }
  }

  // Register the RX callback. In case of KNET, this callback is not used.
  uint32 rx_callback_flags = BCM_RCO_F_ALL_COS;
  if (rx_config.use_interrupt) {
    BCM_RX_F_INTERRUPT_SET(rx_callback_flags);
  }
  RETURN_IF_BCM_ERROR(bcm_rx_register(unit, "HAL packet I/O callback",
                                      packet_receive_callback, BCM_RX_PRIO_MAX,
                                      this, rx_callback_flags));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopRx(int unit) {
  // Unregister the RX callback.
  RETURN_IF_BCM_ERROR(
      bcm_rx_unregister(unit, packet_receive_callback, BCM_RX_PRIO_MAX));

  // Stop RX.
  RETURN_IF_BCM_ERROR(bcm_rx_stop(unit, nullptr));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetRateLimit(
    int unit, const RateLimitConfig& rate_limit_config) {
  // Sanity checking.
  for (const auto& e : rate_limit_config.per_cos_rate_limit_configs) {
    CHECK_RETURN_IF_FALSE(e.first <= 48);  // Maximum number of cos values
  }

  // Apply global and per cos rate limiting.
  RETURN_IF_BCM_ERROR(
      bcm_rx_rate_set(unit, CanonicalRate(rate_limit_config.max_rate_pps)));
  RETURN_IF_BCM_ERROR(
      bcm_rx_burst_set(unit, CanonicalRate(rate_limit_config.max_burst_pkts)));
  for (const auto& e : rate_limit_config.per_cos_rate_limit_configs) {
    RETURN_IF_BCM_ERROR(bcm_rx_cos_rate_set(
        unit, e.first, CanonicalRate(e.second.max_rate_pps)));
    RETURN_IF_BCM_ERROR(bcm_rx_cos_burst_set(
        unit, e.first, CanonicalRate(e.second.max_burst_pkts)));
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetKnetHeaderForDirectTx(int unit, int port,
                                                       int cos, uint64 smac,
                                                       size_t packet_len,
                                                       std::string* header) {
  CHECK_RETURN_IF_FALSE(header != nullptr);
  header->clear();

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

  // For RCPU header, smac is the given smac (read from the KNET netif). dmac
  // is set to 0.
  Uint64ToBcmMac(smac, &rcpu_header.ether_header.ether_shost);
  Uint64ToBcmMac(0, &rcpu_header.ether_header.ether_dhost);

  // RCPU header is always VLAN tagged. We use a fixed special VLAN ID for
  // RCPU headers.
  rcpu_header.ether_header.ether_type = htons(kRcpuVlanEthertype);
  rcpu_header.vlan_tag.vlan_id = htons(kRcpuVlanId);
  rcpu_header.vlan_tag.type = htons(kRcpuEthertype);

  // Now fill up the RCPU data.
  // TODO(unknown): use correct PCI device ID for signature.
  rcpu_header.rcpu_data.rcpu_signature = htons(0 & ~0xf);
  rcpu_header.rcpu_data.rcpu_opcode = kRcpuOpcodeFromCpuPkt;
  rcpu_header.rcpu_data.rcpu_flags |= kRcpuFlagModhdr;  // we add SOBMH later

  header->assign(reinterpret_cast<const char*>(&rcpu_header),
                 sizeof(rcpu_header));

  //------------------------------------------
  // SOB module header (SOBMH)
  //------------------------------------------
  // The rest of the code is chip-dependent. Need to see which chip we are
  // talking about
  ASSIGN_OR_RETURN(auto chip_type, GetChipType(unit));
  CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK ||
                        chip_type == BcmChip::TOMAHAWK_PLUS ||
                        chip_type == BcmChip::TRIDENT2)
      << "Un-supported BCM chip type: " << BcmChip::BcmChipType_Name(chip_type);

  cos = (cos >= 0 ? cos : BCM_COS_DEFAULT);
  int qbase = 0;
  RETURN_IF_BCM_ERROR(soc_esw_hw_qnum_get(unit, port, 0, &qbase));
  int qnum = qbase + cos;
  int module = -1;
  RETURN_IF_BCM_ERROR(bcm_stk_my_modid_get(unit, &module));

  char meta[kRcpuTxMetaSize];
  memset(meta, 0, sizeof(meta));
  bool ok = true;
  if (chip_type == BcmChip::TRIDENT2) {
    ok &= SobFieldSizeVerify<12>(qnum);
    ok &= SetSobField<0, 31, 30>(meta, 0x2);                // INTERNAL_HEADER
    ok &= SetSobField<0, 29, 24>(meta, 0x01);               // SOBMH_FROM_CPU
    ok &= SetSobSplitField<1, 31, 30, 11, 10>(meta, qnum);  // QUEUE_NUM_3
    ok &= SetSobField<1, 6, 0>(meta, port);                 // DST_PORT
    ok &= SetSobField<2, 31, 28>(meta, cos);                // INPUT_PRI
    ok &= SetSobField<2, 27, 24>(meta, cos);                // COS
    ok &= SetSobField<2, 18, 18>(meta, 1);                  // UNICAST: yes
    ok &= SetSobSplitField<2, 17, 8, 9, 0>(meta, qnum);     // QUEUE_NUM_1 & 2
    ok &= SetSobField<2, 7, 0>(meta, module);               // SRC_MODID
  } else if (chip_type == BcmChip::TOMAHAWK ||
             chip_type == BcmChip::TOMAHAWK_PLUS) {
    ok &= SobFieldSizeVerify<12>(qnum);
    ok &= SetSobField<0, 31, 30>(meta, 0x2);   // INTERNAL_HEADER
    ok &= SetSobField<0, 29, 24>(meta, 0x01);  // SOBMH_FROM_CPU
    ok &= SetSobField<1, 7, 0>(meta, port);    // DST_PORT
    ok &= SetSobField<2, 28, 25>(meta, cos);   // INPUT_PRI
    ok &= SetSobField<2, 13, 8>(meta, cos);    // COS
    ok &= SetSobField<2, 14, 14>(meta, 1);     // UNICAST: yes
    ok &= SetSobField<2, 7, 0>(meta, module);  // SRC_MODID
  }
  CHECK_RETURN_IF_FALSE(ok) << "Failed to set SOBMH fields.";
  header->append(meta, sizeof(meta));

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
  rcpu_header.ether_header.ether_type = htons(kRcpuVlanEthertype);
  rcpu_header.vlan_tag.vlan_id = htons(kRcpuVlanId);
  rcpu_header.vlan_tag.type = htons(kRcpuEthertype);

  // Now fill up the RCPU data.
  // TODO(unknown): use correct PCI device ID for signature.
  rcpu_header.rcpu_data.rcpu_signature = htons(0 & ~0xf);
  rcpu_header.rcpu_data.rcpu_opcode = kRcpuOpcodeFromCpuPkt;

  header->assign(reinterpret_cast<char*>(&rcpu_header), sizeof(rcpu_header));

  return ::util::OkStatus();
}

size_t BcmSdkWrapper::GetKnetHeaderSizeForRx(int unit) {
  return sizeof(RcpuHeader) + kRcpuRxMetaSize;
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
  CHECK_RETURN_IF_FALSE(chip_type == BcmChip::TOMAHAWK ||
                        chip_type == BcmChip::TOMAHAWK_PLUS ||
                        chip_type == BcmChip::TRIDENT2)
      << "Un-supported BCM chip type: " << BcmChip::BcmChipType_Name(chip_type);

  const char* meta = header.data() + sizeof(RcpuHeader);
  int src_module = -1, dst_module = -1, src_port = -1, dst_port = -1,
      op_code = -1;
  if (chip_type == BcmChip::TRIDENT2) {
    op_code = GetDcbField<uint8, 9, 10, 8>(meta);      // OPCODE
    src_module = GetDcbField<uint8, 7, 31, 24>(meta);  // SRC_MODID
    dst_module = GetDcbField<uint8, 6, 15, 8>(meta);   // DST_MODID
    src_port = GetDcbField<uint8, 7, 23, 16>(meta);    // SRC_PORT
    dst_port = GetDcbField<uint8, 6, 7, 0>(meta);      // DST_PORT
    *cos = GetDcbField<uint8, 4, 5, 0>(meta);          // COS
  } else if (chip_type == BcmChip::TOMAHAWK ||
             chip_type == BcmChip::TOMAHAWK_PLUS) {
    op_code = GetDcbField<uint8, 9, 10, 8>(meta);      // OPCODE
    src_module = GetDcbField<uint8, 7, 31, 24>(meta);  // SRC_MODID
    dst_module = GetDcbField<uint8, 6, 15, 8>(meta);   // DST_MODID
    src_port = GetDcbField<uint8, 7, 23, 16>(meta);    // SRC_PORT
    dst_port = GetDcbField<uint8, 6, 7, 0>(meta);      // DST_PORT
    *cos = GetDcbField<uint8, 4, 5, 0>(meta);          // COS
  }
  int module = -1;
  RETURN_IF_BCM_ERROR(bcm_stk_my_modid_get(unit, &module));
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
    case 1:  // BCM_PKT_OPCODE_UC
      CHECK_RETURN_IF_FALSE(dst_module == module)
          << "Invalid dst_module: (op_code=" << op_code
          << ", src_mod=" << src_module << ", dst_mod=" << dst_module
          << ", base_mod=" << module << ", src_port=" << src_port
          << ", dst_port=" << dst_port << ", cos=" << *cos << ").";
      *ingress_logical_port = src_port;
      *egress_logical_port = dst_port;
      break;
    case 0:  // BCM_PKT_OPCODE_CPU
    case 2:  // BCM_PKT_OPCODE_BC
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
  RETURN_IF_BCM_ERROR(bcm_field_init(unit));
  RETURN_IF_BCM_ERROR(bcm_policer_init(unit));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclControl(int unit,
                                            const AclControl& acl_control) {
  // All ACL stages are by default enabled for all ports
  // Check external port ACL enable flags
  if (acl_control.extern_port_flags.apply) {
    // TODO(unknown): get external port list and apply flags per port
  }
  // Check internal port ACL enable flags
  if (acl_control.intern_port_flags.apply) {
    // TODO(unknown): get internal port list and apply flags per port
  }
  LOG(WARNING) << "Currently not explicitly enabling/disabling ACL stages for "
               << "packets ingressing on internal and external ports.";
  // Check CPU port ACL enable flags
  if (acl_control.cpu_port_flags.apply) {
    RETURN_IF_BCM_ERROR(
        bcm_port_control_set(unit, 0 /*cmic port*/, bcmPortControlFilterLookup,
                             acl_control.cpu_port_flags.vfp_enable ? 1 : 0));
    RETURN_IF_BCM_ERROR(
        bcm_port_control_set(unit, 0 /*cmic port*/, bcmPortControlFilterIngress,
                             acl_control.cpu_port_flags.ifp_enable ? 1 : 0));
    RETURN_IF_BCM_ERROR(
        bcm_port_control_set(unit, 0 /*cmic port*/, bcmPortControlFilterEgress,
                             acl_control.cpu_port_flags.efp_enable ? 1 : 0));
  }
  // Apply intra-slice double wide enable flag
  if (acl_control.intra_double_wide_enable.apply) {
    RETURN_IF_BCM_ERROR(bcm_field_control_set(
        unit, bcmFieldControlIntraDoubleEnable,
        acl_control.intra_double_wide_enable.enable ? 1 : 0));
  }
  // Apply stats collection hardware read-through enable flag (slower)
  if (acl_control.stats_read_through_enable.apply) {
    RETURN_IF_BCM_ERROR(bcm_field_control_set(
        unit, bcmFieldControlStatSyncEnable,
        acl_control.stats_read_through_enable.enable ? 1 : 0));
  }
  return ::util::OkStatus();
}

namespace {

// Returns BCM enum for packet layer or else enum count.
bcm_field_data_offset_base_t HalPacketLayerToBcm(BcmUdfSet::PacketLayer layer) {
  static auto* bcm_pkt_layer_map =
      new absl::flat_hash_map<BcmUdfSet::PacketLayer,
                              bcm_field_data_offset_base_t,
                              EnumHash<BcmUdfSet::PacketLayer>>({
          {BcmUdfSet::PACKET_START, bcmFieldDataOffsetBasePacketStart},
          {BcmUdfSet::L2_HEADER, bcmFieldDataOffsetBaseL2Header},
          {BcmUdfSet::L3_HEADER, bcmFieldDataOffsetBaseOuterL3Header},
          {BcmUdfSet::INNER_L3_HEADER, bcmFieldDataOffsetBaseInnerL3Header},
          {BcmUdfSet::L4_HEADER, bcmFieldDataOffsetBaseInnerL4Header},
      });
  return gtl::FindWithDefault(*bcm_pkt_layer_map, layer,
                              bcmFieldDataOffsetBaseCount);
}

// Returns Stratum type for UDF packet layer or else UNKNOWN.
BcmUdfSet::PacketLayer BcmUdfBaseOffsetToHal(
    bcm_field_data_offset_base_t layer) {
  static auto* pkt_layer_map =
      new absl::flat_hash_map<bcm_field_data_offset_base_t,
                              BcmUdfSet::PacketLayer>({
          {bcmFieldDataOffsetBasePacketStart, BcmUdfSet::PACKET_START},
          {bcmFieldDataOffsetBaseL2Header, BcmUdfSet::L2_HEADER},
          {bcmFieldDataOffsetBaseOuterL3Header, BcmUdfSet::L3_HEADER},
          {bcmFieldDataOffsetBaseInnerL3Header, BcmUdfSet::INNER_L3_HEADER},
          {bcmFieldDataOffsetBaseInnerL4Header, BcmUdfSet::L4_HEADER},
      });
  return gtl::FindWithDefault(*pkt_layer_map, layer, BcmUdfSet::UNKNOWN);
}

// Retrieves the currently programmed set of UDF ids.
::util::Status GetAclUdfChunkIds(int unit, std::vector<int>* chunk_ids) {
  int num_chunks = 0;
  // First make the multi_get call to determine total number of chunks, then
  // make call again with an appropriately sized buffer to store the chunk ids.
  RETURN_IF_BCM_ERROR(
      bcm_field_data_qualifier_multi_get(unit, 0, nullptr, &num_chunks));
  if (num_chunks < 0) {
    return MAKE_ERROR(ERR_INTERNAL) << "Failed retrieving UDF chunks.";
  }
  if (num_chunks > 0) {
    chunk_ids->resize(num_chunks);
    chunk_ids->assign(num_chunks, 0);
    RETURN_IF_BCM_ERROR(bcm_field_data_qualifier_multi_get(
        unit, num_chunks, chunk_ids->data(), &num_chunks));
    if (num_chunks != chunk_ids->size()) {
      return MAKE_ERROR(ERR_INTERNAL)
             << "Retrieved wrong UDF chunk count from hardware. Got "
             << num_chunks << ", expected " << chunk_ids->size() << ".";
    }
  }
  return ::util::OkStatus();
}

// Supported packet encapsulations for ACL UDF matching.
const bcm_field_data_packet_format_t kUdfEncaps[] = {
    {0, BCM_FIELD_DATA_FORMAT_L2_LLC,  // LLC
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP_NONE,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP_NONE,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IP
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IPinIP
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IP6in4
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IP+GRE+IP
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IP+GRE+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_NO_TAG, BCM_FIELD_DATA_FORMAT_IP6,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP_NONE,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IP
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IPinIP
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IP6in4
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IP+GRE+IP
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IP+GRE+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+1VLAN+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_SINGLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP6,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP_NONE,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IP
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IPinIP
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IP6in4
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_IP_IN_IP},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IP+GRE+IP
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP4, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IP+GRE+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP4,
     BCM_FIELD_DATA_FORMAT_IP6, BCM_FIELD_DATA_FORMAT_TUNNEL_GRE},
    {0, BCM_FIELD_DATA_FORMAT_L2_ETH_II,  // EthV2+2VLAN+IPv6
     BCM_FIELD_DATA_FORMAT_VLAN_DOUBLE_TAGGED, BCM_FIELD_DATA_FORMAT_IP6,
     BCM_FIELD_DATA_FORMAT_IP_NONE, BCM_FIELD_DATA_FORMAT_TUNNEL_NONE},
};

}  // namespace

::util::Status BcmSdkWrapper::SetAclUdfChunks(int unit, const BcmUdfSet& udfs) {
  // Get the existing UDF qualifier chunks.
  std::vector<int> chunk_ids;
  RETURN_IF_ERROR(GetAclUdfChunkIds(unit, &chunk_ids));
  absl::flat_hash_set<int> hw_chunks(chunk_ids.begin(), chunk_ids.end());
  absl::flat_hash_set<int> specified_chunks;
  std::vector<bcm_field_data_qualifier_t> qualifiers;
  // For each chunk in the set, determine if it is new or a modification of an
  // existing chunk. Also check against existing chunks to determine which
  // chunks already in hardware need to be destroyed.
  for (const auto& udf_chunk : udfs.chunks()) {
    if (!udf_chunk.id()) {
      return MAKE_ERROR(ERR_INVALID_PARAM) << "Received invalid UDF chunk id 0 "
                                           << "for request to program ACL UDFs "
                                           << "on unit " << unit << ".";
    }
    // Check for duplicate chunk in BcmUdfSet.
    if (!specified_chunks.insert(udf_chunk.id()).second) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Specified UDF id " << udf_chunk.id() << " multiple times for "
             << "unit " << unit << " UDF set.";
    }
    bcm_field_data_qualifier_t qualifier;
    bcm_field_data_offset_base_t layer = HalPacketLayerToBcm(udf_chunk.layer());
    if (layer == bcmFieldDataOffsetBaseCount) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Received invalid UDF base offset for unit: " << unit
             << ", chunk: " << udf_chunk.id() << ".";
    }
    // Check if hardware already contains the chunk id.
    if (gtl::ContainsKey(hw_chunks, udf_chunk.id())) {
      // Remove it from the set of chunks to destroy in hardware.
      hw_chunks.erase(udf_chunk.id());
      RETURN_IF_BCM_ERROR(
          bcm_field_data_qualifier_get(unit, udf_chunk.id(), &qualifier));
      if (qualifier.offset == udf_chunk.offset() &&
          qualifier.length == kUdfChunkSize && qualifier.offset_base == layer) {
        continue;
      }
      // Mark chunk to be replaced.
      qualifier.flags =
          BCM_FIELD_DATA_QUALIFIER_WITH_ID | BCM_FIELD_DATA_QUALIFIER_REPLACE;
    } else {
      bcm_field_data_qualifier_t_init(&qualifier);
      qualifier.flags = BCM_FIELD_DATA_QUALIFIER_WITH_ID;
    }
    // Set UDF chunk properties and save.
    qualifier.qual_id = udf_chunk.id();
    qualifier.offset_base = layer;
    qualifier.offset = udf_chunk.offset();
    qualifier.length = kUdfChunkSize;
    qualifiers.push_back(qualifier);
  }
  // Destroy chunks which weren't identified as duplicates or to be replaced.
  for (const auto& rem_chunk : hw_chunks) {
    RETURN_IF_BCM_ERROR(bcm_field_data_qualifier_destroy(unit, rem_chunk));
  }
  // Create the new or modified chunks.
  for (auto& add_chunk : qualifiers) {
    RETURN_IF_BCM_ERROR(bcm_field_data_qualifier_create(unit, &add_chunk));
    // Add the set of supported encaps to the chunk.
    for (int i = 0; i < sizeof(kUdfEncaps) / sizeof(kUdfEncaps[0]); ++i) {
      bcm_field_data_packet_format_t udf_encap = kUdfEncaps[i];
      RETURN_IF_BCM_ERROR(bcm_field_data_qualifier_packet_format_add(
          unit, add_chunk.qual_id, &udf_encap));
    }
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclUdfChunks(int unit, BcmUdfSet* udfs) {
  // Get the programmed chunk ids.
  std::vector<int> chunk_ids;
  RETURN_IF_ERROR(GetAclUdfChunkIds(unit, &chunk_ids));
  // Obtain configuration for each chunk.
  for (const int chunk_id : chunk_ids) {
    bcm_field_data_qualifier_t qualifier;
    RETURN_IF_BCM_ERROR(
        bcm_field_data_qualifier_get(unit, chunk_id, &qualifier));
    auto* udf = udfs->add_chunks();
    udf->set_id(qualifier.qual_id);
    udf->set_layer(BcmUdfBaseOffsetToHal(qualifier.offset_base));
    udf->set_offset(qualifier.offset);
  }
  return ::util::OkStatus();
}

namespace {

// Returns BCM type for given stage or else enum count.
bcm_field_qualify_t HalAclStageToBcm(BcmAclStage stage) {
  static auto* bcm_stage_map =
      new absl::flat_hash_map<BcmAclStage, bcm_field_qualify_t,
                              EnumHash<BcmAclStage>>(
          {{BCM_ACL_STAGE_VFP, bcmFieldQualifyStageLookup},
           {BCM_ACL_STAGE_IFP, bcmFieldQualifyStageIngress},
           {BCM_ACL_STAGE_EFP, bcmFieldQualifyStageEgress}});
  return gtl::FindWithDefault(*bcm_stage_map, stage, bcmFieldQualifyCount);
}

// Returns the BCM type for given field or else enum count.
bcm_field_qualify_t HalAclFieldToBcm(BcmAclStage stage, BcmField::Type field) {
  // Default mappings for most ACL stages.
  static auto* default_field_map =
      new absl::flat_hash_map<BcmField::Type, bcm_field_qualify_t,
                              EnumHash<BcmField::Type>>({
          {BcmField::ETH_TYPE, bcmFieldQualifyEtherType},
          {BcmField::IP_TYPE, bcmFieldQualifyIpType},
          {BcmField::ETH_SRC, bcmFieldQualifySrcMac},
          {BcmField::ETH_DST, bcmFieldQualifyDstMac},
          {BcmField::VRF, bcmFieldQualifyVrf},
          {BcmField::IN_PORT, bcmFieldQualifyInPort},
          {BcmField::IN_PORT_BITMAP, bcmFieldQualifyInPorts},
          {BcmField::OUT_PORT, bcmFieldQualifyDstPort},
          {BcmField::VLAN_VID, bcmFieldQualifyOuterVlanId},
          {BcmField::VLAN_PCP, bcmFieldQualifyOuterVlanPri},
          {BcmField::IPV4_SRC, bcmFieldQualifySrcIp},
          {BcmField::IPV4_DST, bcmFieldQualifyDstIp},
          {BcmField::IPV6_SRC, bcmFieldQualifySrcIp6},
          {BcmField::IPV6_DST, bcmFieldQualifyDstIp6},
          {BcmField::IPV6_SRC_UPPER_64, bcmFieldQualifySrcIp6High},
          {BcmField::IPV6_DST_UPPER_64, bcmFieldQualifyDstIp6High},
          {BcmField::IP_PROTO_NEXT_HDR, bcmFieldQualifyIpProtocol},
          {BcmField::IP_DSCP_TRAF_CLASS, bcmFieldQualifyDSCP},
          {BcmField::IP_TTL_HOP_LIMIT, bcmFieldQualifyTtl},
          {BcmField::VFP_DST_CLASS_ID, bcmFieldQualifyDstClassField},
          {BcmField::L3_DST_CLASS_ID, bcmFieldQualifyDstClassL3},
          {BcmField::L4_SRC, bcmFieldQualifyL4SrcPort},
          {BcmField::L4_DST, bcmFieldQualifyL4DstPort},
          {BcmField::TCP_FLAGS, bcmFieldQualifyTcpControl},
          {BcmField::ICMP_TYPE_CODE, bcmFieldQualifyIcmpTypeCode},
      });
  // Stage specific field mappings.
  static auto* efp_field_map =
      new absl::flat_hash_map<BcmField::Type, bcm_field_qualify_t,
                              EnumHash<BcmField::Type>>({
          {BcmField::OUT_PORT, bcmFieldQualifyOutPort},
      });
  auto* stage_map = (stage == BCM_ACL_STAGE_EFP) ? efp_field_map : nullptr;
  auto default_qual =
      gtl::FindWithDefault(*default_field_map, field, bcmFieldQualifyCount);
  if (stage_map) return gtl::FindWithDefault(*stage_map, field, default_qual);
  return default_qual;
}

}  // namespace

::util::StatusOr<int> BcmSdkWrapper::CreateAclTable(int unit,
                                                    const BcmAclTable& table) {
  bcm_field_group_config_t group_config;
  bcm_field_group_config_t_init(&group_config);
  // Copy qualifier set to field group config.
  BCM_FIELD_QSET_INIT(group_config.qset);
  // Set pipeline stage for table.
  bcm_field_qualify_t bcm_stage = HalAclStageToBcm(table.stage());
  if (bcm_stage == bcmFieldQualifyCount) {
    RETURN_ERROR(ERR_INVALID_PARAM)
        << "Attempted to create ACL table with invalid pipeline stage: "
        << BcmAclStage_Name(table.stage()) << ".";
  }
  BCM_FIELD_QSET_ADD(group_config.qset, bcm_stage);
  // Add qualifier fields to group config.
  for (const auto& field : table.fields()) {
    // Handle UDF qualifier.
    if (field.udf_chunk_id()) {
      RETURN_IF_BCM_ERROR(bcm_field_qset_data_qualifier_add(
          unit, &group_config.qset, field.udf_chunk_id()));
      continue;
    }
    bcm_field_qualify_t bcm_field =
        HalAclFieldToBcm(table.stage(), field.type());
    if (bcm_field == bcmFieldQualifyCount) {
      RETURN_ERROR(ERR_INVALID_PARAM)
          << "Attempted to create ACL table with invalid predefined qualifier: "
          << field.ShortDebugString() << ".";
    }
    BCM_FIELD_QSET_ADD(group_config.qset, bcm_field);
  }
  // Allow SDK to find smallest possible table width for bank.
  group_config.mode = bcmFieldGroupModeAuto;
  // Allow arbitrary initial TCAM size.
  // Set table priority.
  group_config.priority = table.priority();
  // Either set table id or allow SDK to generate it.
  if (table.id()) {
    group_config.flags |= BCM_FIELD_GROUP_CREATE_WITH_ID;
    group_config.group = table.id();
  } else {
    group_config.flags &= ~BCM_FIELD_GROUP_CREATE_WITH_ID;
  }
  // Create the field group given the physical table configuration.
  RETURN_IF_BCM_ERROR(bcm_field_group_config_create(unit, &group_config));
  // Return SDK-generated table id.
  return group_config.group;
}

::util::Status BcmSdkWrapper::DestroyAclTable(int unit, int table_id) {
  RETURN_IF_BCM_ERROR(bcm_field_group_destroy(unit, table_id));
  return ::util::OkStatus();
}

namespace {

// For a qualifier type which fits within 32 bits, returns the corresponding
// mask value which denotes an exact match. If not found, returns ~0.
uint32 ExactMatchMask32(BcmField::Type field) {
  static auto* mask_map =
      new absl::flat_hash_map<BcmField::Type, uint32, EnumHash<BcmField::Type>>(
          {
              {BcmField::ETH_TYPE, 0xffff},
              {BcmField::VRF, 0xffffffff},
              {BcmField::IN_PORT, BCM_FIELD_EXACT_MATCH_MASK},
              {BcmField::OUT_PORT, BCM_FIELD_EXACT_MATCH_MASK},
              {BcmField::VLAN_VID, 0xfff},
              {BcmField::VLAN_PCP, 0x7},
              {BcmField::IPV4_SRC, 0xffffffff},
              {BcmField::IPV4_DST, 0xffffffff},
              {BcmField::IP_PROTO_NEXT_HDR, 0xff},
              {BcmField::IP_DSCP_TRAF_CLASS, 0xff},
              {BcmField::IP_TTL_HOP_LIMIT, 0xff},
              {BcmField::VFP_DST_CLASS_ID, 0xffffffff},
              {BcmField::L3_DST_CLASS_ID, 0xffffffff},
              {BcmField::L4_SRC, 0xffff},
              {BcmField::L4_DST, 0xffff},
              {BcmField::TCP_FLAGS, 0xff},
              {BcmField::ICMP_TYPE_CODE, 0xffff},
          });
  return gtl::FindWithDefault(*mask_map, field, ~0);
}

// For a qualifier type which fits within 64 bits, returns the corresponding
// mask value which denotes an exact match. If not found, returns ~0ULL.
uint64 ExactMatchMask64(BcmField::Type field) {
  static auto* mask_map =
      new absl::flat_hash_map<BcmField::Type, uint64, EnumHash<BcmField::Type>>(
          {
              {BcmField::ETH_DST, 0xffffffffffffULL},
              {BcmField::ETH_SRC, 0xffffffffffffULL},
          });
  return gtl::FindWithDefault(*mask_map, field, ~0ULL);
}

// For a qualifier type which is represented as a string of bytes, returns the
// corresponding mask string which denotes an exact match. If not found, returns
// an empty string.
const std::string& ExactMatchMaskBytes(BcmField::Type field) {
  static auto* mask_map = new absl::flat_hash_map<BcmField::Type, std::string,
                                                  EnumHash<BcmField::Type>>({
      {BcmField::IPV6_SRC,
       "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"},
      {BcmField::IPV6_DST,
       "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"},
      {BcmField::IPV6_SRC_UPPER_64,
       "\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00"},
      {BcmField::IPV6_DST_UPPER_64,
       "\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00"},
  });
  static const auto* default_string = new std::string();
  return gtl::FindWithDefault(*mask_map, field, *default_string);
}

// If the given BcmField denotes source or destination MAC, adds the
// corresponding qualifier to the given flow entry.
::util::Status AddAclMacQualifier(int unit, bcm_field_entry_t entry,
                                  const BcmField& field) {
  if ((field.type() != BcmField::ETH_DST) &&
      (field.type() != BcmField::ETH_SRC)) {
    RETURN_ERROR()
        << "Attempted to add MAC address qualifier with wrong field type: "
        << BcmField::Type_Name(field.type()) << ".";
  }

  // Copy over value and mask from field to BCM types.
  bcm_mac_t value, mask;
  uint64 tmp = htobe64(field.value().u64());
  auto* tmp_ptr = reinterpret_cast<uint8*>(&tmp) + sizeof(tmp) - sizeof(value);
  memcpy(&value, tmp_ptr, sizeof(value));
  if (field.has_mask()) {
    tmp = htobe64(field.mask().u64());
    memcpy(&mask, tmp_ptr, sizeof(mask));
  } else {
    uint64 exact_match_mask = ExactMatchMask64(field.type());
    memcpy(&mask, &exact_match_mask, sizeof(mask));
  }

  // Execute BCM call to add appropriate qualifier to flow.
  if (field.type() == BcmField::ETH_DST) {
    RETURN_IF_BCM_ERROR(bcm_field_qualify_DstMac(unit, entry, value, mask));
  } else {
    RETURN_IF_BCM_ERROR(bcm_field_qualify_SrcMac(unit, entry, value, mask));
  }
  return ::util::OkStatus();
}

// If the given BcmField denotes source or destination IPv6 address, adds the
// corresponding qualifier to the given flow entry.
::util::Status AddAclIpv6Qualifier(int unit, bcm_field_entry_t entry,
                                   const BcmField& field) {
  if (!((field.type() != BcmField::IPV6_SRC) ||
        (field.type() != BcmField::IPV6_DST) ||
        (field.type() != BcmField::IPV6_SRC_UPPER_64) ||
        (field.type() != BcmField::IPV6_DST_UPPER_64))) {
    RETURN_ERROR()
        << "Attempted to add IPv6 address qualifier with wrong field type: "
        << BcmField::Type_Name(field.type()) << ".";
  }

  // Copy over value and mask from field to BCM types.
  bcm_ip6_t value, mask;
  memcpy(&value, field.value().b().data(), sizeof(value));
  if (field.has_mask()) {
    memcpy(&mask, field.mask().b().data(), sizeof(mask));
  } else {
    const auto& exact_match_mask = ExactMatchMaskBytes(field.type());
    memcpy(&mask, exact_match_mask.data(),
           std::min(exact_match_mask.size(), sizeof(mask)));
  }

  // Execute BCM call to add appropriate qualifier to flow.
  switch (field.type()) {
    case BcmField::IPV6_SRC:
      RETURN_IF_BCM_ERROR(bcm_field_qualify_SrcIp6(unit, entry, value, mask));
      break;
    case BcmField::IPV6_DST:
      RETURN_IF_BCM_ERROR(bcm_field_qualify_DstIp6(unit, entry, value, mask));
      break;
    case BcmField::IPV6_SRC_UPPER_64:
      RETURN_IF_BCM_ERROR(
          bcm_field_qualify_SrcIp6High(unit, entry, value, mask));
      break;
    case BcmField::IPV6_DST_UPPER_64:
      RETURN_IF_BCM_ERROR(
          bcm_field_qualify_DstIp6High(unit, entry, value, mask));
      break;
    default:
      RETURN_ERROR() << "Control flow is broken.";
  }
  return ::util::OkStatus();
}

// Add InPorts (Ingress Port Bitmap) qualifier to specified flow.
::util::Status AddAclIPBMQualifier(int unit, bcm_field_entry_t entry,
                                   const BcmField& field) {
  if (field.type() != BcmField::IN_PORT_BITMAP) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempted to add IPBM qualifier with wrong field type: "
           << BcmField::Type_Name(field.type()) << ".";
  }
  if (field.has_mask()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "IPBM qualifier contained unexpected mask entry.";
  }
  if (field.value().u32_list().u32_size() > BCM_PBMP_PORT_MAX) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "IPBM qualifier contains " << field.value().u32_list().u32_size()
           << " ports, more than max count of " << BCM_PBMP_PORT_MAX << ".";
  }
  bcm_pbmp_t pbmp_value;
  BCM_PBMP_CLEAR(pbmp_value);
  // Set value bits for match ports.
  for (const auto& port : field.value().u32_list().u32()) {
    BCM_PBMP_PORT_ADD(pbmp_value, port);
  }
  // Set the mask to all ports ("Don't Care" has no real meaning here).
  bcm_port_config_t port_cfg;
  // TODO(unknown): !!!! Ensure that port bitmap is not being changed
  // under us (as in, only set on chassis config change).
  RETURN_IF_BCM_ERROR(bcm_port_config_get(unit, &port_cfg));
  RETURN_IF_BCM_ERROR(
      bcm_field_qualify_InPorts(unit, entry, pbmp_value, port_cfg.all));
  return ::util::OkStatus();
}

// Add IpType qualifier which is used to match on a subset of EtherType values.
// The qualifier actually supports matching on specific types of packets (e.g.
// IPv4 with specific options), but for now we are just using it as a compressed
// EtherType qualifier.
::util::Status AddAclIpTypeQualifier(int unit, bcm_field_entry_t entry,
                                     const BcmField& field) {
  if (field.type() != BcmField::IP_TYPE) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempted to add IpType metadata qualifier with wrong field "
              "type: "
           << BcmField::Type_Name(field.type()) << ".";
  }
  if (field.has_mask()) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "IpType metadata qualifier contained unexpected mask entry.";
  }
  bcm_field_IpType_t ip_type;
  switch (field.value().u32()) {
    // The case values are EtherType values specified in IEEE 802.3. Please
    // refer to https://en.wikipedia.org/wiki/EtherType.
    case 0x0800:  // IPv4
      ip_type = bcmFieldIpTypeIpv4Any;
      break;
    case 0x86dd:  // IPv6
      ip_type = bcmFieldIpTypeIpv6;
      break;
    case 0x0806:  // ARP
      ip_type = bcmFieldIpTypeArp;
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "IpType metadata qualifier contained unsupported EtherType "
                "value.";
  }
  RETURN_IF_BCM_ERROR(bcm_field_qualify_IpType(unit, entry, ip_type));
  return ::util::OkStatus();
}

// Call Broadcom SDK function to add a specific qualifier field to a flow entry
// given by flow_id. F denotes the type of the function. T denotes the SDK type
// for the match field which resolves to some integer type within 32 bits. F is
// of the form:
//   int (*bcm_field_qualify_<QualifierString>)(
//       int unit, bcm_field_entry_t flow_id, T value, T mask)
template <typename T, typename F>
inline int bcm_add_field_u32(F func, int unit, int flow_id,
                             const BcmField& field) {
  T t_value = static_cast<T>(field.value().u32());
  T t_mask = field.has_mask() ? static_cast<T>(field.mask().u32())
                              : static_cast<T>(ExactMatchMask32(field.type()));
  return func(unit, flow_id, t_value, t_mask);
}

// Adds the qualifier described by the given BcmField to the given flow entry.
// On failure, return error status.
::util::Status AddAclQualifier(int unit, bcm_field_entry_t entry,
                               const BcmAclStage& stage,
                               const BcmField& field) {
  switch (field.type()) {
    case BcmField::IN_PORT:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_port_t>(
          bcm_field_qualify_InPort, unit, entry, field));
      break;
    case BcmField::IN_PORT_BITMAP:
      return AddAclIPBMQualifier(unit, entry, field);
    case BcmField::OUT_PORT:
      if (stage == BCM_ACL_STAGE_EFP) {
        RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_port_t>(
            bcm_field_qualify_OutPort, unit, entry, field));
      } else {
        // This assumes that the caller has verified that unit manages the CPU
        // port.
        bcm_module_t module = -1;
        RETURN_IF_BCM_ERROR(bcm_stk_my_modid_get(unit, &module));
        RETURN_IF_BCM_ERROR(bcm_field_qualify_DstPort(
            unit, entry, module, BCM_FIELD_EXACT_MATCH_MASK,
            field.value().u32(), BCM_FIELD_EXACT_MATCH_MASK));
      }
      break;
    case BcmField::ETH_TYPE:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_ethertype_t>(
          bcm_field_qualify_EtherType, unit, entry, field));
      break;
    case BcmField::IP_TYPE:
      return AddAclIpTypeQualifier(unit, entry, field);
    case BcmField::ETH_SRC:
    case BcmField::ETH_DST:
      return AddAclMacQualifier(unit, entry, field);
    case BcmField::VRF:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_vrf_t>(bcm_field_qualify_Vrf,
                                                       unit, entry, field));
      break;
    case BcmField::VLAN_VID:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_vlan_t>(
          bcm_field_qualify_OuterVlanId, unit, entry, field));
      break;
    case BcmField::VLAN_PCP:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint8>(
          bcm_field_qualify_OuterVlanPri, unit, entry, field));
      break;
    case BcmField::IPV4_SRC:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_ip_t>(bcm_field_qualify_SrcIp,
                                                      unit, entry, field));
      break;
    case BcmField::IPV4_DST:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_ip_t>(bcm_field_qualify_DstIp,
                                                      unit, entry, field));
      break;
    case BcmField::IPV6_SRC:
    case BcmField::IPV6_DST:
    case BcmField::IPV6_SRC_UPPER_64:
    case BcmField::IPV6_DST_UPPER_64:
      return AddAclIpv6Qualifier(unit, entry, field);
    case BcmField::IP_PROTO_NEXT_HDR:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint8>(bcm_field_qualify_IpProtocol,
                                                   unit, entry, field));
      break;
    case BcmField::IP_DSCP_TRAF_CLASS:
      RETURN_IF_BCM_ERROR(
          bcm_add_field_u32<uint8>(bcm_field_qualify_DSCP, unit, entry, field));
      break;
    case BcmField::IP_TTL_HOP_LIMIT:
      RETURN_IF_BCM_ERROR(
          bcm_add_field_u32<uint8>(bcm_field_qualify_Ttl, unit, entry, field));
      break;
    case BcmField::VFP_DST_CLASS_ID:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint32>(
          bcm_field_qualify_DstClassField, unit, entry, field));
      break;
    case BcmField::L3_DST_CLASS_ID:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint32>(
          bcm_field_qualify_DstClassL3, unit, entry, field));
      break;
    case BcmField::L4_SRC:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_l4_port_t>(
          bcm_field_qualify_L4SrcPort, unit, entry, field));
      break;
    case BcmField::L4_DST:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<bcm_l4_port_t>(
          bcm_field_qualify_L4DstPort, unit, entry, field));
      break;
    case BcmField::TCP_FLAGS:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint8>(bcm_field_qualify_TcpControl,
                                                   unit, entry, field));
      break;
    case BcmField::ICMP_TYPE_CODE:
      RETURN_IF_BCM_ERROR(bcm_add_field_u32<uint16>(
          bcm_field_qualify_IcmpTypeCode, unit, entry, field));
      break;
    default:
      RETURN_ERROR() << "Attempted to translate unsupported BcmField::Type: "
                     << BcmField::Type_Name(field.type()) << ".";
  }
  return ::util::OkStatus();
}

#undef ADD_BCM_FIELD

// Fill the configurataion struct for an ACL policer based on BcmMeterConfig
// message.
void FillAclPolicerConfig(const BcmMeterConfig& meter,
                          bcm_policer_config_t* policer_config) {
  bcm_policer_config_t_init(policer_config);
  // Determine whether the meter is to be configured for a single rate (two
  // colors) or for trTCM mode.
  if ((meter.committed_rate() == meter.peak_rate()) &&
      (meter.committed_burst() == meter.peak_burst())) {
    policer_config->mode = bcmPolicerModeCommitted;
  } else {
    policer_config->mode = bcmPolicerModeTrTcm;
    // Need peak rates for trTCM.
    policer_config->pkbits_sec = meter.peak_rate();
    policer_config->pkbits_burst = meter.peak_burst();
  }
  policer_config->ckbits_sec = meter.committed_rate();
  policer_config->ckbits_burst = meter.committed_burst();
  policer_config->flags = BCM_POLICER_MODE_BYTES;
}

// Create and attach policer to the specified flow with the given rate and burst
// parameters.
::util::Status AddAclPolicer(int unit, bcm_field_entry_t entry,
                             const BcmMeterConfig& meter) {
  bcm_policer_config_t policer_config;
  // Initialize and fill the configuration struct.
  FillAclPolicerConfig(meter, &policer_config);
  bcm_policer_t policer_id;
  // Create policer with given configuration.
  RETURN_IF_BCM_ERROR(bcm_policer_create(unit, &policer_config, &policer_id));
  // Attach policer to flow.
  RETURN_IF_BCM_ERROR(
      bcm_field_entry_policer_attach(unit, entry, 0, policer_id));
  return ::util::OkStatus();
}

// Modify policer attached to a flow if it exists, otherwise create a new one
// with the given configuration.
::util::Status ModifyAclPolicer(int unit, bcm_field_entry_t entry,
                                const BcmMeterConfig& meter) {
  // Find if policer exists.
  bcm_policer_t policer_id;
  int retval = bcm_field_entry_policer_get(unit, entry, 0, &policer_id);
  // Create a new policer if it doesn't exist.
  if (retval == BCM_E_NOT_FOUND) {
    return AddAclPolicer(unit, entry, meter);
  } else if (BCM_FAILURE(retval)) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed while looking up policer for flow " << entry << ".";
  }
  // Detach the existing policer.
  RETURN_IF_BCM_ERROR(bcm_field_entry_policer_detach(unit, entry, 0));
  // Re-configure policer
  bcm_policer_config_t policer_config;
  FillAclPolicerConfig(meter, &policer_config);
  RETURN_IF_BCM_ERROR(bcm_policer_set(unit, policer_id, &policer_config));
  // Attach the policer again.
  RETURN_IF_BCM_ERROR(
      bcm_field_entry_policer_attach(unit, entry, 0, policer_id));
  return ::util::OkStatus();
}

// Detach and destroy policer if there is one attached to the specified flow.
::util::Status RemoveAclPolicer(int unit, bcm_field_entry_t entry) {
  // Find if policer exists.
  bcm_policer_t policer_id;
  int retval = bcm_field_entry_policer_get(unit, entry, 0, &policer_id);
  if (BCM_SUCCESS(retval)) {  // Found policer, detach and destroy it.
    RETURN_IF_BCM_ERROR(bcm_field_entry_policer_detach(unit, entry, 0));
    RETURN_IF_BCM_ERROR(bcm_policer_destroy(unit, policer_id));
  } else if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed while looking up policer for flow " << entry << ".";
  }
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

// Adds the action described by the given BcmAction to the given flow entry. On
// failure, returns error status.
::util::Status AddAclAction(int unit, bcm_field_entry_t entry,
                            const BcmAction& action) {
  // Sets of required and optional action parameters.
  absl::flat_hash_set<BcmAction::Param::Type, EnumHash<BcmAction::Param::Type>>
      required;
  absl::flat_hash_set<BcmAction::Param::Type, EnumHash<BcmAction::Param::Type>>
      optional;
  bcm_field_action_t bcm_action;
  uint32 param_0 = 0, param_1 = 0;
  switch (action.type()) {
    case BcmAction::DROP:
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (action.params().empty()) {  // No params, just drop
        bcm_action = bcmFieldActionDrop;
        break;
      }
      switch (action.params(0).value().u32()) {
        case BCM_FIELD_COLOR_GREEN:
          bcm_action = bcmFieldActionGpDrop;
          break;
        case BCM_FIELD_COLOR_YELLOW:
          bcm_action = bcmFieldActionYpDrop;
          break;
        case BCM_FIELD_COLOR_RED:
          bcm_action = bcmFieldActionRpDrop;
          break;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid color parameter for DROP action: "
                 << action.params(0).value().u32() << ".";
      }
      break;
    case BcmAction::OUTPUT_PORT: {
      required.insert(BcmAction::Param::LOGICAL_PORT);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionRedirect;
      bcm_gport_t out_gport;
      RETURN_IF_BCM_ERROR(
          bcm_port_gport_get(unit, action.params(0).value().u32(), &out_gport));
      param_1 = static_cast<uint32>(out_gport);
    } break;
    // TODO(unknown): It may be necessary to add an OUTPUT_PBMP action
    // to support L2 multicast output.
    case BcmAction::OUTPUT_TRUNK:
      required.insert(BcmAction::Param::TRUNK_PORT);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionRedirectTrunk;
      param_0 = action.params(0).value().u32();
      break;
    case BcmAction::OUTPUT_L3:
      required.insert(BcmAction::Param::EGRESS_INTF_ID);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionL3Switch;
      param_0 = action.params(0).value().u32();
      break;
    case BcmAction::COPY_TO_CPU:
      required.insert(BcmAction::Param::QUEUE);
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionCopyToCpu;
      for (const auto& param : action.params()) {
        switch (param.type()) {
          case BcmAction::Param::QUEUE:
            param_1 = param.value().u32();
            break;
          case BcmAction::Param::COLOR:
            switch (param.value().u32()) {
              case BCM_FIELD_COLOR_GREEN:
                bcm_action = bcmFieldActionGpCopyToCpu;
                break;
              case BCM_FIELD_COLOR_YELLOW:
                bcm_action = bcmFieldActionYpCopyToCpu;
                break;
              case BCM_FIELD_COLOR_RED:
                bcm_action = bcmFieldActionRpCopyToCpu;
                break;
              default:
                return MAKE_ERROR(ERR_INVALID_PARAM)
                       << "Invalid color parameter for COPY_TO_CPU action: "
                       << param.value().u32() << ".";
            }
            param_0 = 1;
            break;
          default:
            return MAKE_ERROR(ERR_INVALID_PARAM)
                   << "Invalid parameter type for COPY_TO_CPU action: "
                   << BcmAction::Param::Type_Name(param.type()) << ".";
        }
      }
      break;
    case BcmAction::CANCEL_COPY_TO_CPU:
      optional.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      if (action.params().empty()) {  // No params, just drop
        bcm_action = bcmFieldActionCopyToCpuCancel;
        break;
      }
      switch (action.params(0).value().u32()) {
        case BCM_FIELD_COLOR_GREEN:
          bcm_action = bcmFieldActionGpCopyToCpuCancel;
          break;
        case BCM_FIELD_COLOR_YELLOW:
          bcm_action = bcmFieldActionYpCopyToCpuCancel;
          break;
        case BCM_FIELD_COLOR_RED:
          bcm_action = bcmFieldActionRpCopyToCpuCancel;
          break;
        default:
          return MAKE_ERROR(ERR_INVALID_PARAM)
                 << "Invalid color parameter for CANCEL_COPY_TO_CPU "
                 << "action: " << action.params(0).value().u32() << ".";
      }
      break;
    case BcmAction::SET_COLOR:
      required.insert(BcmAction::Param::COLOR);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionDropPrecedence;
      param_0 = action.params(0).value().u32();
      if ((param_0 != BCM_FIELD_COLOR_GREEN) ||
          (param_0 != BCM_FIELD_COLOR_YELLOW) ||
          (param_0 != BCM_FIELD_COLOR_RED)) {
        return MAKE_ERROR(ERR_INVALID_PARAM)
               << "Invalid color parameter for SET_COLOR action: " << param_0
               << ".";
      }
      break;
    case BcmAction::SET_VRF:
      required.insert(BcmAction::Param::VRF);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionVrfSet;
      param_0 = action.params(0).value().u32();
      break;
    case BcmAction::SET_VFP_DST_CLASS_ID:
      required.insert(BcmAction::Param::VFP_DST_CLASS_ID);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionClassDestSet;
      param_0 = action.params(0).value().u32();
      break;
    case BcmAction::SET_IP_DSCP:
      required.insert(BcmAction::Param::IP_DSCP);
      RETURN_IF_ERROR(VerifyAclActionParams(action, required, optional));
      bcm_action = bcmFieldActionDscpNew;
      param_0 = action.params(0).value().u32();
      break;
    default:
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to translate unsupported BcmAction::Type: "
             << BcmAction::Type_Name(action.type()) << ".";
  }
  RETURN_IF_BCM_ERROR(
      bcm_field_action_add(unit, entry, bcm_action, param_0, param_1));
  return ::util::OkStatus();
}

}  // namespace

::util::StatusOr<int> BcmSdkWrapper::InsertAclFlow(int unit,
                                                   const BcmFlowEntry& flow,
                                                   bool add_stats,
                                                   bool color_aware) {
  // Generate flow id for new ACL rule.
  bcm_field_entry_t flow_id;
  RETURN_IF_BCM_ERROR(
      bcm_field_entry_create(unit, flow.bcm_acl_table_id(), &flow_id));
  // Translate qualifiers and add to new flow entry.
  for (const auto& field : flow.fields()) {
    // Handle pre-defined qualifier.
    if (!field.udf_chunk_id()) {
      RETURN_IF_ERROR(AddAclQualifier(unit, flow_id, flow.acl_stage(), field));
      continue;
    }
    if (field.value().b().size() != kUdfChunkSize ||
        (field.has_mask() && field.mask().b().size() != kUdfChunkSize)) {
      return MAKE_ERROR(ERR_INVALID_PARAM)
             << "Attempted to program flow with UDF chunk "
             << field.udf_chunk_id() << " with value or mask size not equal to "
             << "chunk size " << kUdfChunkSize << ".";
    }
    uint8 value[kUdfChunkSize], mask[kUdfChunkSize];
    memcpy(value, field.value().b().data(), kUdfChunkSize);
    if (field.has_mask()) {
      memcpy(mask, field.mask().b().data(), kUdfChunkSize);
    } else {
      memset(mask, 0xff, kUdfChunkSize);
    }
    RETURN_IF_BCM_ERROR(bcm_field_qualify_data(
        unit, flow_id, field.udf_chunk_id(), value, mask, kUdfChunkSize));
  }
  // Add policer if meter config is specified.
  if (flow.has_meter()) {
    RETURN_IF_ERROR(AddAclPolicer(unit, flow_id, flow.meter()));
  }
  // Translate actions and add to new flow entry.
  for (const auto& action : flow.actions()) {
    RETURN_IF_ERROR(AddAclAction(unit, flow_id, action));
  }
  RETURN_IF_BCM_ERROR(bcm_field_entry_prio_set(unit, flow_id, flow.priority()));
  // Setup and attach stats to the flow entry.
  if (add_stats) {
    RETURN_IF_ERROR(
        AddAclStats(unit, flow.bcm_acl_table_id(), flow_id, color_aware));
  }
  // Install flow entry.
  RETURN_IF_BCM_ERROR(bcm_field_entry_install(unit, flow_id));
  return flow_id;
}

::util::Status BcmSdkWrapper::ModifyAclFlow(int unit, int flow_id,
                                            const BcmFlowEntry& flow) {
  // Remove all actions.
  RETURN_IF_BCM_ERROR(bcm_field_action_remove_all(unit, flow_id));
  // Modify or remove policer if it exists.
  if (flow.has_meter()) {
    RETURN_IF_ERROR(ModifyAclPolicer(unit, flow_id, flow.meter()));
  } else {
    RETURN_IF_ERROR(RemoveAclPolicer(unit, flow_id));
  }
  // Translate actions and add to updated flow entry.
  for (const auto& action : flow.actions()) {
    RETURN_IF_ERROR(AddAclAction(unit, flow_id, action));
  }
  // Detach and re-attach statistics before reinstalling flow. This is a
  // necessary hack due to b/28863173. The re-attaching should be done after
  // modifications to the shadow state (changing actions, policer config) and
  // before the re-install of the flow, which commits the changes.
  int stat_id;
  int retval = bcm_field_entry_stat_get(unit, flow_id, &stat_id);
  // BCM_E_NOT_FOUND means the stat does not exist, which is not an error.
  if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed to lookup existing stats attached to flow " << flow_id
        << " on unit " << unit << " before reinstalling the flow.";
    RETURN_IF_BCM_ERROR(bcm_field_entry_stat_detach(unit, flow_id, stat_id))
        << "Failed to detach stat " << stat_id << " from flow " << flow_id
        << " on unit " << unit << " before reinstalling the flow.";
    RETURN_IF_BCM_ERROR(bcm_field_entry_stat_attach(unit, flow_id, stat_id))
        << "Failed to re-attach stat " << stat_id << " to flow " << flow_id
        << " on unit " << unit << " before reinstalling the flow.";
  }
  // Re-install the flow entry.
  RETURN_IF_BCM_ERROR(bcm_field_entry_reinstall(unit, flow_id));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RemoveAclFlow(int unit, int flow_id) {
  // Remove the flow entry.
  RETURN_IF_BCM_ERROR(bcm_field_entry_remove(unit, flow_id));
  // Detach and delete the stats attached to the flow if they exist.
  RETURN_IF_ERROR(RemoveAclStats(unit, flow_id));
  // Remove a policer if it exists.
  RETURN_IF_ERROR(RemoveAclPolicer(unit, flow_id));
  // Destroy the flow entry.
  RETURN_IF_BCM_ERROR(bcm_field_entry_destroy(unit, flow_id));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclPolicer(int unit, int flow_id,
                                            const BcmMeterConfig& meter) {
  RETURN_IF_ERROR(ModifyAclPolicer(unit, flow_id, meter));
  RETURN_IF_BCM_ERROR(bcm_field_entry_reinstall(unit, flow_id));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclTable(int unit, int table_id,
                                          BcmAclTable* table) {
  // Get qualifier set from table.
  bcm_field_qset_t qset;
  RETURN_IF_BCM_ERROR(bcm_field_group_get(unit, table_id, &qset));
  // Get table stage.
  for (int i = BCM_ACL_STAGE_UNKNOWN + 1; i <= BcmAclStage_MAX; ++i) {
    auto stage = static_cast<BcmAclStage>(i);
    if (BCM_FIELD_QSET_TEST(qset, HalAclStageToBcm(stage))) {
      table->set_stage(stage);
      break;
    }
  }
  // Get table pre-defined qualifiers.
  table->clear_fields();
  for (int i = BcmField::UNKNOWN + 1; i <= BcmField::Type_MAX; ++i) {
    auto field = static_cast<BcmField::Type>(i);
    if (BCM_FIELD_QSET_TEST(qset, HalAclFieldToBcm(table->stage(), field))) {
      table->add_fields()->set_type(field);
    }
  }
  // Find all configured UDF qualifiers and check if table uses any of them.
  int num_chunks = 0;
  RETURN_IF_BCM_ERROR(
      bcm_field_data_qualifier_multi_get(unit, 0, nullptr, &num_chunks));
  if (num_chunks) {
    std::vector<int> chunk_ids(num_chunks);
    // num_chunks changes from total UDF count to table UDF count.
    RETURN_IF_BCM_ERROR(bcm_field_qset_data_qualifier_get(
        unit, qset, num_chunks, chunk_ids.data(), &num_chunks));
    for (int i = 0; i < num_chunks; ++i) {
      table->add_fields()->set_udf_chunk_id(chunk_ids[i]);
    }
  }
  // Get table priority.
  int priority = 0;
  RETURN_IF_BCM_ERROR(bcm_field_group_priority_get(unit, table_id, &priority));
  table->set_priority(priority);
  // Populate table id.
  table->set_id(table_id);
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InsertPacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeletePacketReplicationEntry(
    const BcmPacketReplicationEntry& entry) {
  return ::util::OkStatus();
}

namespace {

// TODO(unknown): use google/util/endian/endian.h?
inline uint64 ntohll(uint64 n) { return ntohl(1) == 1 ? n : bswap_64(n); }

// Attempts to recover the source or destination MAC qualifier from the given
// flow entry into the given BcmField*. Returns true if found. On failure,
// returns error status.
::util::StatusOr<bool> GetAclMacQualifier(int unit, bcm_field_entry_t entry,
                                          BcmField* field) {
  // Execute BCM call to get appropriate qualifier from flow.
  bcm_mac_t value, mask;
  int retval;
  switch (field->type()) {
    case BcmField::ETH_DST:
      retval = bcm_field_qualify_DstMac_get(unit, entry, &value, &mask);
      break;
    case BcmField::ETH_SRC:
      retval = bcm_field_qualify_SrcMac_get(unit, entry, &value, &mask);
      break;
    default:
      RETURN_ERROR()
          << "Attempted to get MAC address qualifier with wrong field type: "
          << BcmField::Type_Name(field->type()) << ".";
  }

  // Check success and copy over value and mask
  if (BCM_SUCCESS(retval)) {
    uint64 tmp = 0;
    int offset = sizeof(tmp) - sizeof(mask);
    memcpy(reinterpret_cast<uint8*>(&tmp) + offset, &mask, sizeof(mask));
    // Check the mask value to handle a hardware bug where success may be
    // returned but the flow in fact doesn't use the qualifier.
    if (tmp) {
      field->mutable_mask()->set_u64(ntohll(tmp));
      memcpy(reinterpret_cast<uint8*>(&tmp) + offset, &value, sizeof(value));
      field->mutable_value()->set_u64(ntohll(tmp));
      return true;
    }
  } else if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed trying to obtain qualifier " << field->type()
        << " for unit: " << unit << ", entry: " << entry << ".";
  }
  return false;
}

// Attempts to recover the source or destination IPv6 qualifier from the given
// flow entry into the given BcmField*. Returns true if found. On failure,
// returns error status.
::util::StatusOr<bool> GetAclIpv6Qualifier(int unit, bcm_field_entry_t entry,
                                           BcmField* field) {
  // Execute BCM call to get approriate qualifier from flow.
  bcm_ip6_t value, mask;
  int retval;
  switch (field->type()) {
    case BcmField::IPV6_SRC:
      retval = bcm_field_qualify_SrcIp6_get(unit, entry, &value, &mask);
      break;
    case BcmField::IPV6_DST:
      retval = bcm_field_qualify_DstIp6_get(unit, entry, &value, &mask);
      break;
    case BcmField::IPV6_SRC_UPPER_64:
      retval = bcm_field_qualify_SrcIp6High_get(unit, entry, &value, &mask);
      break;
    case BcmField::IPV6_DST_UPPER_64:
      retval = bcm_field_qualify_DstIp6High_get(unit, entry, &value, &mask);
      break;
    default:
      RETURN_ERROR()
          << "Attempted to get IPv6 address qualifier with wrong field type: "
          << BcmField::Type_Name(field->type()) << ".";
  }

  // Check success and copy over value and mask.
  if (BCM_SUCCESS(retval)) {
    // Check the mask value to handle a hardware bug where success may be
    // returned but the flow in fact doesn't use the qualifier.
    if (reinterpret_cast<uint64*>(mask)[0] ||
        reinterpret_cast<uint64*>(mask)[1]) {
      field->mutable_mask()->set_b(reinterpret_cast<const char*>(mask),
                                   sizeof(mask));
      field->mutable_value()->set_b(reinterpret_cast<const char*>(value),
                                    sizeof(value));
      return true;
    }
  } else if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed trying to obtain qualifier " << field->type()
        << " for unit: " << unit << ", entry: " << entry << ".";
  }
  return false;
}

::util::StatusOr<bool> GetAclIPBMQualifier(int unit, bcm_field_entry_t entry,
                                           BcmField* field) {
  if (field->type() != BcmField::IN_PORT_BITMAP) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempted to get IPBM qualifier with wrong field type: "
           << BcmField::Type_Name(field->type()) << ".";
  }
  // Get qualifier value and mask from hardware.
  bcm_pbmp_t pbmp_value, pbmp_mask;
  int retval =
      bcm_field_qualify_InPorts_get(unit, entry, &pbmp_value, &pbmp_mask);
  // Check success and copy over value and mask.
  if (BCM_SUCCESS(retval)) {
    bcm_port_config_t port_cfg;
    // TODO(unknown): !!!! Ensure that port bitmap is not being changed
    // under us (as in, only set on chassis config change).
    RETURN_IF_BCM_ERROR(bcm_port_config_get(unit, &port_cfg));
    // For IPBM, default behavior is to just match on all ports (which is the
    // same as not matching on any specific port(s)). As such, the qualifier has
    // only been specified if the value is not equal to the bitmap of all ports.
    if (memcmp(&pbmp_value, &port_cfg.all, sizeof(bcm_pbmp_t))) {
      auto* value = field->mutable_value()->mutable_u32_list();
      uint32 port;
      BCM_PBMP_ITER(pbmp_value, port) { value->add_u32(port); }
      return true;
    }
  } else if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed trying to obtain qualifier " << field->type()
        << " for unit: " << unit << ", entry: " << entry << ".";
  }
  return false;
}

// Get IpType qualifier which is used to match on a subset of EtherType values.
::util::StatusOr<bool> GetAclIpTypeQualifier(int unit, bcm_field_entry_t entry,
                                             BcmField* field) {
  if (field->type() != BcmField::IP_TYPE) {
    return MAKE_ERROR(ERR_INVALID_PARAM)
           << "Attempted to get IpType metadata qualifier with wrong field "
           << "type: " << BcmField::Type_Name(field->type()) << ".";
  }
  // Get IpType value and check success.
  bcm_field_IpType_t ip_type;
  int retval = bcm_field_qualify_IpType_get(unit, entry, &ip_type);
  if (retval == BCM_E_NOT_FOUND) return false;
  RETURN_IF_BCM_ERROR(retval)
      << "Failed trying to obtain qualifier " << field->type()
      << " for unit: " << unit << ", entry: " << entry << ".";
  // Add appropriate EtherType to field if IpType is recognized.
  switch (ip_type) {
    // The values set here are EtherType values specified in IEEE 802.3. Please
    // refer to https://en.wikipedia.org/wiki/EtherType.
    case bcmFieldIpTypeIpv4Any:
      field->mutable_value()->set_u32(0x0800);  // IPv4
      return true;
    case bcmFieldIpTypeIpv6:
      field->mutable_value()->set_u32(0x86dd);  // IPv6
      return true;
    case bcmFieldIpTypeArp:
      field->mutable_value()->set_u32(0x0806);  // ARP
      return true;
    default:
      return false;
  }
}

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
  if (BCM_SUCCESS(retval)) {
    *value = static_cast<uint32>(t_value);
    *mask = static_cast<uint32>(t_mask);
  }
  return retval;
}

// Attempts to recover the qualifier of type given in the BcmField* from the
// given flow entry. If found, populates the BcmField* and returns true. On
// failure, returns error status.
::util::StatusOr<bool> GetAclQualifier(int unit, bcm_field_entry_t entry,
                                       const BcmAclStage& stage,
                                       BcmField* field) {
  uint32 value = 0, mask = 0;
  int retval;
  // Execute appropriate call to get qualifier from hardware flow based on type.
  switch (field->type()) {
    case BcmField::IN_PORT:
      retval = bcm_get_field_u32<bcm_port_t>(bcm_field_qualify_InPort_get, unit,
                                             entry, &value, &mask);
      // InPort_get gives false positives, check that port is in range and that
      // the match is non-trivial.
      if ((value >= BCM_PBMP_PORT_MAX) || !(value & mask)) return false;
      break;
    case BcmField::IN_PORT_BITMAP:
      return GetAclIPBMQualifier(unit, entry, field);
    case BcmField::OUT_PORT:
      if (stage == BCM_ACL_STAGE_EFP) {
        retval = bcm_get_field_u32<bcm_port_t>(bcm_field_qualify_OutPort_get,
                                               unit, entry, &value, &mask);
      } else {
        bcm_module_t module = 0, module_mask = 0;
        bcm_port_t port_value = 0, port_mask = 0;
        retval = bcm_field_qualify_DstPort_get(
            unit, entry, &module, &module_mask, &port_value, &port_mask);
        // DstPort_get gives false positives, check that port is in range and
        // that the match is non-trivial.
        if ((port_value >= BCM_PBMP_PORT_MAX) || !(port_value & port_mask)) {
          return false;
        }
        if (BCM_SUCCESS(retval)) {
          value = static_cast<uint32>(port_value);
          mask = static_cast<uint32>(port_mask);
        }
      }
      break;
    case BcmField::ETH_TYPE:
      retval = bcm_get_field_u32<bcm_ethertype_t>(
          bcm_field_qualify_EtherType_get, unit, entry, &value, &mask);
      break;
    case BcmField::IP_TYPE:
      return GetAclIpTypeQualifier(unit, entry, field);
    case BcmField::ETH_SRC:
    case BcmField::ETH_DST:
      return GetAclMacQualifier(unit, entry, field);
    case BcmField::VRF:
      retval = bcm_get_field_u32<uint32>(bcm_field_qualify_Vrf_get, unit, entry,
                                         &value, &mask);
      break;
    case BcmField::VLAN_VID:
      retval = bcm_get_field_u32<bcm_vlan_t>(bcm_field_qualify_OuterVlanId_get,
                                             unit, entry, &value, &mask);
      break;
    case BcmField::VLAN_PCP:
      retval = bcm_get_field_u32<uint8>(bcm_field_qualify_OuterVlanPri_get,
                                        unit, entry, &value, &mask);
      break;
    case BcmField::IPV4_SRC:
      retval = bcm_get_field_u32<bcm_ip_t>(bcm_field_qualify_SrcIp_get, unit,
                                           entry, &value, &mask);
      break;
    case BcmField::IPV4_DST:
      retval = bcm_get_field_u32<bcm_ip_t>(bcm_field_qualify_DstIp_get, unit,
                                           entry, &value, &mask);
      break;
    case BcmField::IPV6_SRC:
    case BcmField::IPV6_DST:
    case BcmField::IPV6_SRC_UPPER_64:
    case BcmField::IPV6_DST_UPPER_64:
      return GetAclIpv6Qualifier(unit, entry, field);
    case BcmField::IP_PROTO_NEXT_HDR:
      retval = bcm_get_field_u32<uint8>(bcm_field_qualify_IpProtocol_get, unit,
                                        entry, &value, &mask);
      break;
    case BcmField::IP_DSCP_TRAF_CLASS:
      retval = bcm_get_field_u32<uint8>(bcm_field_qualify_DSCP_get, unit, entry,
                                        &value, &mask);
      break;
    case BcmField::IP_TTL_HOP_LIMIT:
      retval = bcm_get_field_u32<uint8>(bcm_field_qualify_Ttl_get, unit, entry,
                                        &value, &mask);
      break;
    case BcmField::VFP_DST_CLASS_ID:
      retval = bcm_get_field_u32<uint32>(bcm_field_qualify_DstClassField_get,
                                         unit, entry, &value, &mask);
      break;
    case BcmField::L3_DST_CLASS_ID:
      retval = bcm_get_field_u32<uint32>(bcm_field_qualify_DstClassL3_get, unit,
                                         entry, &value, &mask);
      break;
    case BcmField::L4_SRC:
      retval = bcm_get_field_u32<bcm_l4_port_t>(bcm_field_qualify_L4SrcPort_get,
                                                unit, entry, &value, &mask);
      break;
    case BcmField::L4_DST:
      retval = bcm_get_field_u32<bcm_l4_port_t>(bcm_field_qualify_L4DstPort_get,
                                                unit, entry, &value, &mask);
      break;
    case BcmField::TCP_FLAGS:
      retval = bcm_get_field_u32<uint8>(bcm_field_qualify_TcpControl_get, unit,
                                        entry, &value, &mask);
      break;
    case BcmField::ICMP_TYPE_CODE:
      retval = bcm_get_field_u32<uint16>(bcm_field_qualify_IcmpTypeCode_get,
                                         unit, entry, &value, &mask);
      break;
    default:
      LOG(WARNING) << "Currently cannot retrieve BcmField::Type "
                   << BcmField::Type_Name(field->type()) << " from hardware.";
      return false;
  }

  // Check success and copy over value and mask.
  if (BCM_SUCCESS(retval)) {
    // Check the mask value to handle a hardware bug where success may be
    // returned but the flow in fact doesn't use the qualifier.
    if (mask) {
      field->mutable_value()->set_u32(value);
      field->mutable_mask()->set_u32(mask);
      return true;
    }
  } else if (retval != BCM_E_NOT_FOUND) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed trying to obtain qualifier " << field->type()
        << " for unit: " << unit << ", entry: " << entry << ".";
  }
  return false;
}

::util::StatusOr<bool> GetAclUdfQualifier(int unit, bcm_field_entry_t entry,
                                          BcmField* field) {
  uint8 value[BcmSdkWrapper::kUdfChunkSize], mask[BcmSdkWrapper::kUdfChunkSize];
  uint16 length = 0;  // ignored.
  int retval = bcm_field_qualify_data_get(unit, entry, field->udf_chunk_id(),
                                          BcmSdkWrapper::kUdfChunkSize, value,
                                          mask, &length);
  if (retval == BCM_E_NOT_FOUND) return false;
  RETURN_IF_BCM_ERROR(retval)
      << "Failed attempting to retrieve UDF chunk " << field->udf_chunk_id()
      << " from flow: " << entry << ", unit: " << unit << ".";
  // Check for false positive returning empty mask.
  bool mask_is_zero = true;
  for (int i = 0; i < BcmSdkWrapper::kUdfChunkSize; ++i) {
    if (mask[i]) {
      mask_is_zero = false;
      break;
    }
  }
  if (mask_is_zero) return false;
  // TODO(unknown): determine if SDK ever shortens UDF qualifiers, in
  // which case length will need to be considered.
  field->mutable_value()->set_b(reinterpret_cast<const char*>(value),
                                BcmSdkWrapper::kUdfChunkSize);
  field->mutable_mask()->set_b(reinterpret_cast<const char*>(mask),
                               BcmSdkWrapper::kUdfChunkSize);
  return true;
}

// Executes the BCM SDK call to retrieve a policer and its configuration for the
// specified flow entry. If found, returns true. On failure, returns error
// status.
::util::StatusOr<bool> CheckGetAclPolicer(int unit, bcm_field_entry_t entry,
                                          BcmMeterConfig* meter) {
  bcm_policer_t policer_id = -1;
  int retval = bcm_field_entry_policer_get(unit, entry, 0, &policer_id);
  if (retval == BCM_E_NOT_FOUND) return false;
  RETURN_IF_BCM_ERROR(retval) << "Failed to obtain policer for unit: " << unit
                              << ", entry: " << entry << ".";
  bcm_policer_config_t policer_config;
  // Retrieve policer configuration.
  RETURN_IF_BCM_ERROR(bcm_policer_get(unit, policer_id, &policer_config));
  meter->set_committed_rate(policer_config.ckbits_sec);
  meter->set_committed_burst(policer_config.ckbits_burst);
  // Determine if the policer is in two-color single-rate mode or trTCM mode.
  if (policer_config.mode == bcmPolicerModeTrTcm) {
    meter->set_peak_rate(policer_config.pkbits_sec);
    meter->set_peak_burst(policer_config.pkbits_burst);
  } else {
    // In single-rate mode, peak rate will be equal to committed rate.
    meter->set_peak_rate(policer_config.ckbits_sec);
    meter->set_peak_burst(policer_config.ckbits_burst);
  }
  return true;
}

// Executes the BCM SDK call to retrieve a given action type and its parameters
// for the given flow entry. If found, returns true. On failure, returns error
// status.
inline ::util::StatusOr<bool> CheckGetAclAction(int unit,
                                                bcm_field_entry_t entry,
                                                bcm_field_action_t bcm_action,
                                                uint32* param_0,
                                                uint32* param_1) {
  int retval = bcm_field_action_get(unit, entry, bcm_action, param_0, param_1);
  if (retval == BCM_E_NOT_FOUND) return false;
  RETURN_IF_BCM_ERROR(retval)
      << "Failed to obtain action " << bcm_action << " for unit: " << unit
      << ", entry: " << entry << ".";
  return true;
}

// Get ACL action for given flow from hardware in the common case of only one
// parameter. Expects the type of the action to be set in the BcmAction
// parameter.
inline ::util::StatusOr<bool> GetAclActionOneParam(
    int unit, bcm_field_entry_t entry, bcm_field_action_t bcm_action,
    bool save_param_0, BcmAction* action) {
  uint32 param_0 = 0, param_1 = 0;
  ASSIGN_OR_RETURN(bool success, CheckGetAclAction(unit, entry, bcm_action,
                                                   &param_0, &param_1));
  if (success) {
    action->mutable_params(0)->mutable_value()->set_u32(save_param_0 ? param_0
                                                                     : param_1);
    return true;
  }
  return false;
}

// Attempts to retrieve the action of type given in the BcmAction* from the
// given flow entry. If found, populates the BcmAction* and returns true. On
// failure, returns error status.
::util::StatusOr<bool> GetAclAction(int unit, bcm_field_entry_t entry,
                                    BcmAction* action) {
  BcmAction::Param* param;
  uint32 param_0, param_1;
  bool success;
  switch (action->type()) {
    case BcmAction::DROP: {
      // For the DROP action, the actual Broadcom action changes based on the
      // color (or none) specified. Therefore, we need to try to retrieve each
      // different color.
      // TODO(unknown): in case there are two drop actions of different
      // color, will end up retrieving only the first hit. This is WRONG.
      uint32 color = 0;
      ASSIGN_OR_RETURN(success,
                       CheckGetAclAction(unit, entry, bcmFieldActionGpDrop,
                                         &param_0, &param_1));
      if (success) color = BCM_FIELD_COLOR_GREEN;
      if (!success) {
        ASSIGN_OR_RETURN(success,
                         CheckGetAclAction(unit, entry, bcmFieldActionYpDrop,
                                           &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_YELLOW;
      }
      if (!success) {
        ASSIGN_OR_RETURN(success,
                         CheckGetAclAction(unit, entry, bcmFieldActionRpDrop,
                                           &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_RED;
      }
      if (success) {
        param = action->add_params();
        param->set_type(BcmAction::Param::COLOR);
        param->mutable_value()->set_u32(color);
        return true;
      } else {
        ASSIGN_OR_RETURN(success,
                         CheckGetAclAction(unit, entry, bcmFieldActionDrop,
                                           &param_0, &param_1));
        if (success) return true;
      }
    } break;
    case BcmAction::OUTPUT_PORT:
      action->add_params()->set_type(BcmAction::Param::LOGICAL_PORT);
      return GetAclActionOneParam(unit, entry, bcmFieldActionRedirect, false,
                                  action);
    case BcmAction::OUTPUT_TRUNK:
      action->add_params()->set_type(BcmAction::Param::TRUNK_PORT);
      return GetAclActionOneParam(unit, entry, bcmFieldActionRedirectTrunk,
                                  true, action);
    case BcmAction::OUTPUT_L3:
      action->add_params()->set_type(BcmAction::Param::EGRESS_INTF_ID);
      return GetAclActionOneParam(unit, entry, bcmFieldActionL3Switch, true,
                                  action);
    case BcmAction::COPY_TO_CPU: {
      // For the COPY_TO_CPU action, the actual Broadcom action changes based on
      // the color (or none) specified. Therefore, we need to try to retrieve
      // each different color.
      // TODO(unknown): in case there are two actions of different
      // color, will end up retrieving only the first hit. This is WRONG.
      uint32 color;
      ASSIGN_OR_RETURN(success,
                       CheckGetAclAction(unit, entry, bcmFieldActionGpCopyToCpu,
                                         &param_0, &param_1));
      if (success) color = BCM_FIELD_COLOR_GREEN;
      if (!success) {
        ASSIGN_OR_RETURN(
            success, CheckGetAclAction(unit, entry, bcmFieldActionYpCopyToCpu,
                                       &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_YELLOW;
      }
      if (!success) {
        ASSIGN_OR_RETURN(
            success, CheckGetAclAction(unit, entry, bcmFieldActionRpCopyToCpu,
                                       &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_RED;
      }
      if (success) {
        param = action->add_params();
        param->set_type(BcmAction::Param::COLOR);
        param->mutable_value()->set_u32(color);
      } else {
        ASSIGN_OR_RETURN(success,
                         CheckGetAclAction(unit, entry, bcmFieldActionCopyToCpu,
                                           &param_0, &param_1));
      }
      if (success) {
        param = action->add_params();
        param->set_type(BcmAction::Param::QUEUE);
        param->mutable_value()->set_u32(param_1);
        return true;
      }
    } break;
    case BcmAction::CANCEL_COPY_TO_CPU: {
      // For the CANCEL_COPY_TO_CPU action, the actual Broadcom action changes
      // based on the color (or none) specified. Therefore, we need to try to
      // retrieve each different color.
      // TODO(unknown): in case there are two actions of different
      // color, will end up retrieving only the first hit. This is WRONG.
      uint32 color = 0;
      ASSIGN_OR_RETURN(
          success,
          CheckGetAclAction(unit, entry, bcmFieldActionGpCopyToCpuCancel,
                            &param_0, &param_1));
      if (success) color = BCM_FIELD_COLOR_GREEN;
      if (!success) {
        ASSIGN_OR_RETURN(
            success,
            CheckGetAclAction(unit, entry, bcmFieldActionYpCopyToCpuCancel,
                              &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_YELLOW;
      }
      if (!success) {
        ASSIGN_OR_RETURN(
            success,
            CheckGetAclAction(unit, entry, bcmFieldActionRpCopyToCpuCancel,
                              &param_0, &param_1));
        if (success) color = BCM_FIELD_COLOR_RED;
      }
      if (success) {
        param = action->add_params();
        param->set_type(BcmAction::Param::COLOR);
        param->mutable_value()->set_u32(color);
        return true;
      } else {
        ASSIGN_OR_RETURN(
            success,
            CheckGetAclAction(unit, entry, bcmFieldActionCopyToCpuCancel,
                              &param_0, &param_1));
        if (success) return true;
      }
    } break;
    case BcmAction::SET_COLOR:
      action->add_params()->set_type(BcmAction::Param::COLOR);
      return GetAclActionOneParam(unit, entry, bcmFieldActionDropPrecedence,
                                  true, action);
    case BcmAction::SET_VRF:
      action->add_params()->set_type(BcmAction::Param::VRF);
      return GetAclActionOneParam(unit, entry, bcmFieldActionVrfSet, true,
                                  action);
    case BcmAction::SET_VFP_DST_CLASS_ID:
      action->add_params()->set_type(BcmAction::Param::VFP_DST_CLASS_ID);
      return GetAclActionOneParam(unit, entry, bcmFieldActionClassDestSet, true,
                                  action);
    case BcmAction::SET_IP_DSCP:
      action->add_params()->set_type(BcmAction::Param::IP_DSCP);
      return GetAclActionOneParam(unit, entry, bcmFieldActionDscpNew, true,
                                  action);
    default:
      LOG(WARNING) << "Currently cannot retrieve BcmAction::Type "
                   << BcmAction::Type_Name(action->type()) << " from hardware.";
  }
  return false;
}

}  // namespace

::util::Status BcmSdkWrapper::GetAclFlow(int unit, int flow_id,
                                         BcmFlowEntry* flow) {
  bool success;
  // For each possible match field, try to generate BcmField.
  for (int i = BcmField::UNKNOWN + 1; i <= BcmField::Type_MAX; ++i) {
    BcmField field;
    field.set_type(static_cast<BcmField::Type>(i));
    ASSIGN_OR_RETURN(success,
                     GetAclQualifier(unit, flow_id, flow->acl_stage(), &field));
    if (success) *flow->add_fields() = field;
  }
  // Retrieve any UDF qualifiers.
  std::vector<int> chunk_ids;
  RETURN_IF_ERROR(GetAclUdfChunkIds(unit, &chunk_ids));
  for (const int chunk_id : chunk_ids) {
    BcmField field;
    field.set_udf_chunk_id(chunk_id);
    ASSIGN_OR_RETURN(success, GetAclUdfQualifier(unit, flow_id, &field));
    if (success) *flow->add_fields() = field;
  }
  // Check for a policer configuration.
  BcmMeterConfig meter;
  ASSIGN_OR_RETURN(success, CheckGetAclPolicer(unit, flow_id, &meter));
  if (success) *flow->mutable_meter() = meter;
  // For each possible match action, try to generate BcmAction.
  for (int i = BcmAction::UNKNOWN + 1; i <= BcmAction::Type_MAX; ++i) {
    BcmAction action;
    action.set_type(static_cast<BcmAction::Type>(i));
    ASSIGN_OR_RETURN(success, GetAclAction(unit, flow_id, &action));
    if (success) *flow->add_actions() = action;
  }
  // Get the flow priority.
  int priority;
  RETURN_IF_BCM_ERROR(bcm_field_entry_prio_get(unit, flow_id, &priority));
  flow->set_priority(priority);
  flow->set_bcm_table_type(BcmFlowEntry::BCM_TABLE_ACL);
  return ::util::OkStatus();
}

::util::StatusOr<std::string> BcmSdkWrapper::MatchAclFlow(
    int unit, int flow_id, const BcmFlowEntry& flow) {
  BcmFlowEntry recovered_flow;
  // Get flow priority.
  int hw_priority;
  RETURN_IF_BCM_ERROR(bcm_field_entry_prio_get(unit, flow_id, &hw_priority));
  if (static_cast<uint32>(hw_priority) != flow.priority()) {
    return std::string(absl::Substitute(
        "Failed to match flow $0 in hardware. Expected priority $1, "
        "got priority $2.",
        flow_id, flow.priority(), hw_priority));
  }
  // Get qualifier fields for fields in the original flow.
  for (const auto& field : flow.fields()) {
    BcmField hw_field;
    // Handle UDF qualifier.
    if (field.udf_chunk_id()) {
      hw_field.set_udf_chunk_id(field.udf_chunk_id());
      ASSIGN_OR_RETURN(bool got_field,
                       GetAclUdfQualifier(unit, flow_id, &hw_field));
      if (!got_field) {
        return std::string(absl::Substitute(
            "Failed to match flow $0 in hardware. Did not find UDF qualifier "
            "with chunk id $1.",
            flow_id, field.udf_chunk_id()));
      }
      if (!field.has_mask()) {
        for (int i = 0; i < kUdfChunkSize; ++i) {
          if (hw_field.mask().b().data()[i] == 0xff) continue;
          return std::string(absl::Substitute(
              "Failed to match flow $0 in hardware. Expected exact match mask "
              "for field $1, got $2.",
              flow_id, field.ShortDebugString().c_str(),
              hw_field.ShortDebugString().c_str()));
        }
      }
      continue;
    }
    hw_field.set_type(field.type());
    ASSIGN_OR_RETURN(
        bool got_field,
        GetAclQualifier(unit, flow_id, flow.acl_stage(), &hw_field));
    if (!got_field) {
      return std::string(absl::Substitute(
          "Failed to match flow $0 in hardware. Did not find qualifier field "
          "of type $1.",
          flow_id, BcmField::Type_Name(field.type()).c_str()));
    }
    // Handle default match case which implies exact match mask. Remove
    // recovered field mask if it is the exact match mask to simplify proto
    // comparison, otherwise return false.
    if (!field.has_mask() && (field.type() != BcmField::IN_PORT_BITMAP) &&
        (field.type() != BcmField::IP_TYPE)) {
      bool exact_match = false;
      switch (hw_field.mask().data_case()) {
        case BcmTableEntryValue::kU32:
          exact_match = hw_field.mask().u32() == ExactMatchMask32(field.type());
          break;
        case BcmTableEntryValue::kU64:
          exact_match = hw_field.mask().u64() == ExactMatchMask64(field.type());
          break;
        case BcmTableEntryValue::kB:
          exact_match =
              hw_field.mask().b() == ExactMatchMaskBytes(field.type());
          break;
        default:
          RETURN_ERROR() << "Invalid mask type: " << hw_field.mask().data_case()
                         << " for retrieved qualifier of type "
                         << BcmField::Type_Name(hw_field.type()) << ".";
      }
      if (!exact_match) {
        return std::string(absl::Substitute(
            "Failed to match flow $0 in hardware. Expected exact match mask "
            "for field $1, got $2.",
            flow_id, field.ShortDebugString().c_str(),
            hw_field.ShortDebugString().c_str()));
      }
      hw_field.clear_mask();
    }
    if (!MessageDifferencer::Equals(field, hw_field)) {
      return std::string(absl::Substitute(
          "Failed to match flow $0 in hardware. Expected $1, got $2.", flow_id,
          field.ShortDebugString().c_str(),
          hw_field.ShortDebugString().c_str()));
    }
  }
  // Get actions and params for actions in the original flow.
  MessageDifferencer action_comp;
  // Don't care about the order of action parameters.
  action_comp.TreatAsSet(BcmAction::descriptor()->FindFieldByName("params"));
  for (const auto& action : flow.actions()) {
    BcmAction hw_action;
    hw_action.set_type(action.type());
    ASSIGN_OR_RETURN(bool got_action, GetAclAction(unit, flow_id, &hw_action));
    if (!got_action) {
      return std::string(absl::Substitute(
          "Failed to match flow $0 in hardware. Did not find action type $1.",
          flow_id, BcmAction::Type_Name(action.type()).c_str()));
    }
    if (!action_comp.Compare(action, hw_action)) {
      return std::string(absl::Substitute(
          "Failed to match flow $0 in hardware. Expected $1, got $2.", flow_id,
          action.ShortDebugString().c_str(),
          hw_action.ShortDebugString().c_str()));
    }
  }
  // Compare policer configuration.
  if (flow.has_meter()) {
    BcmMeterConfig meter;
    ASSIGN_OR_RETURN(bool success, CheckGetAclPolicer(unit, flow_id, &meter));
    if (!success) {
      return std::string(
          absl::Substitute("Flow $0 is expected to but does not "
                           "have a meter configured.",
                           flow_id));
    }
    if (!MessageDifferencer::Equals(flow.meter(), meter)) {
      return std::string(absl::Substitute(
          "Failed to match flow $0 in hardware. Expected meter config $1, "
          "got $2.",
          flow_id, flow.meter().ShortDebugString().c_str(),
          meter.ShortDebugString().c_str()));
    }
  }
  return std::string();
}

::util::Status BcmSdkWrapper::GetAclTableFlowIds(int unit, int table_id,
                                                 std::vector<int>* flow_ids) {
  int num_entries;
  // Get the number flows in the table.
  RETURN_IF_BCM_ERROR(
      bcm_field_entry_multi_get(unit, table_id, 0, nullptr, &num_entries));
  if (num_entries < 0) {
    RETURN_ERROR()
        << "bcm_field_entry_multi_get() returned negative flow count for table "
        << table_id << " on unit " << unit << ".";
  } else if (!num_entries) {
    return ::util::OkStatus();
  }

  // Get the previously returned number of entries.
  flow_ids->resize(num_entries);
  RETURN_IF_BCM_ERROR(bcm_field_entry_multi_get(
      unit, table_id, num_entries, flow_ids->data(), &num_entries));
  if (num_entries != static_cast<int>(flow_ids->size())) {
    RETURN_ERROR() << "Consecutive bcm_field_entry_multi_get() for table "
                   << table_id << " on unit " << unit
                   << " return different flow counts.";
  }
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddAclStats(int unit, int table_id, int flow_id,
                                          bool color_aware) {
  int stat_id;
  // Create stat object with counter types depending on whether or not color is
  // relevant to the flow.
  bcm_field_stat_t stat_entry[kMaxStatCount];
  if (color_aware) {
    memcpy(stat_entry, kColoredStatEntry, sizeof(kColoredStatEntry));
    RETURN_IF_BCM_ERROR(bcm_field_stat_create(unit, table_id, kColoredStatCount,
                                              stat_entry, &stat_id));
  } else {
    memcpy(stat_entry, kUncoloredStatEntry, sizeof(kUncoloredStatEntry));
    RETURN_IF_BCM_ERROR(bcm_field_stat_create(
        unit, table_id, kUncoloredStatCount, stat_entry, &stat_id));
  }
  if (stat_id < 0) {
    RETURN_ERROR(ERR_INTERNAL)
        << "Received invalid stat_id " << stat_id << " for new stats object for"
        << " flow " << flow_id << ".";
  }
  // Attach stat to flow.
  RETURN_IF_BCM_ERROR(bcm_field_entry_stat_attach(unit, flow_id, stat_id));
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RemoveAclStats(int unit, int flow_id) {
  int stat_id;
  // Try to find stat object.
  int retval = bcm_field_entry_stat_get(unit, flow_id, &stat_id);
  if (retval == BCM_E_NOT_FOUND) return ::util::OkStatus();
  if (BCM_FAILURE(retval)) {
    RETURN_IF_BCM_ERROR(retval)
        << "Failed to find stat object attached to flow " << flow_id
        << " on unit " << unit << ".";
  }
  // Detach stat from flow and destroy.
  RETURN_IF_BCM_ERROR(bcm_field_entry_stat_detach(unit, flow_id, stat_id));
  RETURN_IF_BCM_ERROR(bcm_field_stat_destroy(unit, stat_id));
  return ::util::OkStatus();
}

namespace {

// Executes the BCM SDK call to retrieve the values of the stat counters
// represented by the given stat_id on the given unit. Requires that size of
// counter_data is >= 8 * size.
template <int size>
inline util::Status GetAclStatCounters(int unit, int stat_id,
                                       const bcm_field_stat_t stat_entry[size],
                                       uint64* counter_data) {
  bcm_field_stat_t stat_entry_copy[size];  // NOLINT: runtime/arrays
  memcpy(stat_entry_copy, stat_entry, size * sizeof(bcm_field_stat_t));
  // Needed because of type mismatch between stratum::uint64 and bcm::uint64
  ::uint64 counter_data_ = 0;
  RETURN_IF_BCM_ERROR(bcm_field_stat_multi_get(
      unit, stat_id, size, stat_entry_copy, &counter_data_));
  *counter_data = counter_data_;
  return ::util::OkStatus();
}

}  // namespace

::util::Status BcmSdkWrapper::GetAclStats(int unit, int flow_id,
                                          BcmAclStats* stats) {
  int stat_id;
  // Try to find stat object.
  RETURN_IF_BCM_ERROR(bcm_field_entry_stat_get(unit, flow_id, &stat_id));
  // Get the number of stat counters.
  int num_stats;
  RETURN_IF_BCM_ERROR(bcm_field_stat_size(unit, stat_id, &num_stats));
  uint64 counter_data[kMaxStatCount];
  if (num_stats == kUncoloredStatCount) {  // Uncolored stats
    RETURN_IF_ERROR(GetAclStatCounters<kUncoloredStatCount>(
        unit, stat_id, kUncoloredStatEntry, counter_data));
    auto* total = stats->mutable_total();
    // Store total counter values.
    total->set_packets(counter_data[kTotalCounterIndex]);
    total->set_bytes(counter_data[kTotalCounterIndex + 1]);
  } else if (num_stats == kColoredStatCount) {  // Colored stats
    RETURN_IF_ERROR(GetAclStatCounters<kColoredStatCount>(
        unit, stat_id, kColoredStatEntry, counter_data));
    // Store red and green counter values.
    auto* red = stats->mutable_red();
    red->set_packets(counter_data[kRedCounterIndex]);
    red->set_bytes(counter_data[kRedCounterIndex + 1]);
    auto* green = stats->mutable_green();
    green->set_packets(counter_data[kGreenCounterIndex]);
    green->set_bytes(counter_data[kGreenCounterIndex + 1]);
  } else {
    RETURN_ERROR() << "Invalid stat count for stat id " << stat_id
                   << " on unit " << unit << ".";
  }
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
  absl::ReaderMutexLock l(&data_lock_);
  const BcmSocDevice* soc_device =
      gtl::FindPtrOrNull(unit_to_soc_device_, unit);
  CHECK_RETURN_IF_FALSE(soc_device != nullptr)
      << "Unit " << unit << " has not been assigned to any SOC device.";
  CHECK_RETURN_IF_FALSE(soc_device->sdk_checkpoint_fd != -1)
      << "SDK checkpoint file for unit " << unit << " is not open.";

  return soc_device->sdk_checkpoint_fd;
}

::util::StatusOr<ibde_t*> BcmSdkWrapper::GetBde() const {
  if (bde_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "BDE not initialized yet. Call InitializeSdk() first.";
  }

  return bde_;
}

pthread_t BcmSdkWrapper::GetDiagShellThreadId() const {
  if (bcm_diag_shell_ == nullptr) return 0;  // sim mode
  return bcm_diag_shell_->GetDiagShellThreadId();
}

void BcmSdkWrapper::OnLinkscanEvent(int unit, int port, bcm_port_info_t* info) {
  // Create LinkscanEvent message.
  PortState state;
  if (info->linkstatus == BCM_PORT_LINK_STATUS_FAILED ||
      info->linkstatus == BCM_PORT_LINK_STATUS_REMOTE_FAULT) {
    state = PORT_STATE_FAILED;
  } else if (info->linkstatus == BCM_PORT_LINK_STATUS_UP) {
    state = PORT_STATE_UP;
  } else if (info->linkstatus == BCM_PORT_LINK_STATUS_DOWN) {
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

::util::Status BcmSdkWrapper::CleanupKnet(int unit) {
  // Cleanup existing KNET filters and KNET intfs.
  RETURN_IF_BCM_ERROR(
      bcm_knet_filter_traverse(unit, knet_filter_remover, nullptr));
  RETURN_IF_BCM_ERROR(
      bcm_knet_netif_traverse(unit, knet_intf_remover, nullptr));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::OpenSdkCheckpointFile(int unit) {
  if (bde_ == nullptr) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "BDE not initialized yet. Call InitializeSdk() first.";
  }

  // Find the checkpoint file path for this unit.
  ASSIGN_OR_RETURN(const std::string& checkpoint_file_path,
                   FindSdkCheckpointFilePath(unit));

  {
    absl::WriterMutexLock l(&data_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_.count(unit))
        << "Unit " << unit << " has not been assigned to any SOC device.";
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_[unit]->sdk_checkpoint_fd == -1)
        << "SDK checkpoint FD for unit " << unit << " already set.";

    // First check to make sure file is non-empty for the case of warmboot.
    struct stat filestat;
    CHECK_RETURN_IF_FALSE(stat(checkpoint_file_path.c_str(), &filestat) == 0)
        << "stat() failed on SDK checkpoint file '" << checkpoint_file_path
        << "' for unit " << unit << ".";
    CHECK_RETURN_IF_FALSE(filestat.st_size > 0)
        << "SDK checkpoint file '" << checkpoint_file_path << "' for unit "
        << unit << " is empty.";

    // Open the file now.
    int fd = open(checkpoint_file_path.c_str(), O_RDWR);
    CHECK_RETURN_IF_FALSE(fd != -1)
        << "open() failed on SDK checkpoint file '" << checkpoint_file_path
        << "' for unit " << unit << ".";
    unit_to_soc_device_[unit]->sdk_checkpoint_fd = fd;
  }

  // Register the SDK checkpoint file.
  RETURN_IF_ERROR(RegisterSdkCheckpointFile(unit));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::CreateSdkCheckpointFile(int unit) {
  CHECK_RETURN_IF_FALSE(bde_)
      << "BDE not initialized yet. Call InitializeSdk() first.";

  // Find the checkpoint file path for this unit.
  ASSIGN_OR_RETURN(const std::string& checkpoint_file_path,
                   FindSdkCheckpointFilePath(unit));

  {
    absl::WriterMutexLock l(&data_lock_);
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_.count(unit))
        << "Unit " << unit << " has not been assigned to any SOC device.";
    CHECK_RETURN_IF_FALSE(unit_to_soc_device_[unit]->sdk_checkpoint_fd == -1)
        << "SDK checkpoint FD for unit " << unit << " already set.";

    // Open a new SDK checkpoint file.
    int fd = open(checkpoint_file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    CHECK_RETURN_IF_FALSE(fd != -1)
        << "open() failed on SDK checkpoint file '" << checkpoint_file_path
        << "' for unit " << unit << ".";
    unit_to_soc_device_[unit]->sdk_checkpoint_fd = fd;
  }

  // Register the SDK checkpoint file.
  RETURN_IF_ERROR(RegisterSdkCheckpointFile(unit));

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RegisterSdkCheckpointFile(int unit) {
  // Find the checkpoint file size for this unit.
  ASSIGN_OR_RETURN(const int checkpoint_file_size,
                   FindSdkCheckpointFileSize(unit));

  // Register the checkpoint file with the SDK.
  RETURN_IF_BCM_ERROR(soc_stable_set(unit, _SHR_SWITCH_STABLE_APPLICATION, 0));
  RETURN_IF_BCM_ERROR(soc_switch_stable_register(
      unit, &sdk_checkpoint_file_read, &sdk_checkpoint_file_write, 0, 0));
  RETURN_IF_BCM_ERROR(soc_stable_size_set(unit, checkpoint_file_size));

  return ::util::OkStatus();
}

::util::StatusOr<std::string> BcmSdkWrapper::FindSdkCheckpointFilePath(
    int unit) {
  return std::string(absl::Substitute("$0/bcm-sdk-checkpoint-unit$1.bin",
                                      FLAGS_bcm_sdk_checkpoint_dir, unit));
}

::util::StatusOr<int> BcmSdkWrapper::FindSdkCheckpointFileSize(int unit) {
  ASSIGN_OR_RETURN(auto chip_type, GetChipType(unit));
  switch (chip_type) {
    case BcmChip::TOMAHAWK:
    case BcmChip::TOMAHAWK_PLUS:
    case BcmChip::TRIDENT2:
      return kSdkCheckpointFileSize;
    default:
      return MAKE_ERROR(ERR_INTERNAL) << "Un-supported BCM chip type: "
                                      << BcmChip::BcmChipType_Name(chip_type);
  }
}

::util::StatusOr<BcmChip::BcmChipType> BcmSdkWrapper::GetChipType(int unit) {
  absl::ReaderMutexLock l(&data_lock_);
  auto it = unit_to_chip_type_.find(unit);
  CHECK_RETURN_IF_FALSE(it != unit_to_chip_type_.end())
      << "Unit " << unit << "  is not found in unit_to_chip_type_. Have you "
      << "called FindUnit for this unit before?";
  return it->second;
}

::util::Status BcmSdkWrapper::SetIntfAndConfigurePhyForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, uint64 speed_bps,
    const std::string& intf_type) {
  // Parse bcm_port_if_t, autoneg and FEC from the intf_type string.
  // intf_type can encode just mode (e.g. sr) or mode and other parameters
  // (e.g. cr4_anoff, cr_anon_fec). Split into components:
  // <physical-interface-mode>_<autoneg>_<fec>
  std::vector<std::string> tokens = absl::StrSplit(intf_type, '_');
  std::string intf_str = "", autoneg_str = "", fec_str = "";
  if (tokens.size() > 3U) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid intf_type for (unit, port) = (" << unit << ", " << port
           << "): " << intf_type;
  }
  if (!tokens.empty()) {
    intf_str = tokens[0];
  }
  if (tokens.size() >= 2U) {
    autoneg_str = tokens[1];
  }
  if (tokens.size() == 3U) {
    fec_str = tokens[2];
  }

  bcm_port_if_t bcm_port_intf = BCM_PORT_IF_NULL;
  bool default_autoneg = false, default_fec = false;
  bool autoneg = false, fec = false;
  if (intf_str == "sfi") {
    bcm_port_intf = BCM_PORT_IF_SFI;
    default_autoneg = false;
  } else if (intf_str == "sr") {
    bcm_port_intf = BCM_PORT_IF_SR;
    default_autoneg = false;
  } else if (intf_str == "kr") {
    bcm_port_intf = BCM_PORT_IF_KR;
    default_autoneg = false;
  } else if (intf_str == "kr2") {
    bcm_port_intf = BCM_PORT_IF_KR2;
    default_autoneg = true;
  } else if (intf_str == "kr4") {
    bcm_port_intf = BCM_PORT_IF_KR4;
    default_autoneg = true;
  } else if (intf_str == "cr") {
    bcm_port_intf = BCM_PORT_IF_CR;
    default_autoneg = true;
  } else if (intf_str == "cr2") {
    bcm_port_intf = BCM_PORT_IF_CR2;
    default_autoneg = true;
  } else if (intf_str == "cr4") {
    bcm_port_intf = BCM_PORT_IF_CR4;
    default_autoneg = true;
  } else {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Invalid intf_type for (unit, port) = (" << unit << ", " << port
           << "): " << intf_type;
  }
  autoneg = autoneg_str.empty() ? default_autoneg : (autoneg_str == "anon");
  fec = fec_str.empty() ? default_fec : (fec_str == "fecon");

  // Set interface for the port.
  // TODO(unknown): For some reason previously SDK required port speed to
  // be applied before and after interface mode updating. This may not be
  // needed any more. Remove if not needed.
  RETURN_IF_BCM_ERROR(
      bcm_port_speed_set(unit, port, speed_bps / kBitsPerMegabit));
  RETURN_IF_BCM_ERROR(bcm_port_interface_set(unit, port, bcm_port_intf));
  RETURN_IF_BCM_ERROR(
      bcm_port_speed_set(unit, port, speed_bps / kBitsPerMegabit));

  // Apply autoneg settings for the port.
  if (autoneg) {
    bcm_port_ability_t port_ability_mask;
    bcm_port_ability_t_init(&port_ability_mask);
    switch (speed_bps) {
      case kHundredGigBps:
        port_ability_mask.speed_full_duplex = BCM_PORT_ABILITY_100GB;
        break;
      case kFiftyGigBps:
        port_ability_mask.speed_full_duplex = BCM_PORT_ABILITY_50GB;
        break;
      case kTwentyFiveGigBps:
        port_ability_mask.speed_full_duplex = BCM_PORT_ABILITY_25GB;
        break;
      default:
        return MAKE_ERROR(ERR_INTERNAL)
               << "Invalid speed for (unit, port) = (" << unit << ", " << port
               << ") when autoneg is ON: " << speed_bps;
    }
    port_ability_mask.interface = bcm_port_intf;
    RETURN_IF_BCM_ERROR(
        bcm_port_ability_advert_set(unit, port, &port_ability_mask));
  }
  RETURN_IF_BCM_ERROR(bcm_port_autoneg_set(unit, port, autoneg));

  // Apply FEC settings for the port.
  if (fec && !autoneg) {
    // Use bcm_port_phy_control for FEC control when autoneg is disabled.
    // e.g. for 100G optical links.
    // SDK enables FEC by default when autoneg is enabled (per IEEE std.)
    bcm_port_phy_control_t fec_control =
        (speed_bps == kHundredGigBps
             ? BCM_PORT_PHY_CONTROL_FORWARD_ERROR_CORRECTION_CL91
             : BCM_PORT_PHY_CONTROL_FORWARD_ERROR_CORRECTION);
    // Reset FEC before re-enabling to ensure correct settings if port speed
    // is changed, as in the case of flex ports.
    RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
        unit, port, BCM_PORT_PHY_CONTROL_FORWARD_ERROR_CORRECTION,
        BCM_PORT_PHY_CONTROL_FEC_OFF));
    RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
        unit, port, BCM_PORT_PHY_CONTROL_FORWARD_ERROR_CORRECTION_CL91,
        BCM_PORT_PHY_CONTROL_FEC_OFF));
    RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(unit, port, fec_control,
                                                 BCM_PORT_PHY_CONTROL_FEC_ON));
  } else if (!fec && autoneg) {
    // To disable FEC when autoneg is enabled, use a custom API.
    // This is non-standard behavior.
    // RETURN_IF_BCM_ERROR(goog_100g_fec_control_set(unit, port, 0));
    return MAKE_ERROR(ERR_FEATURE_UNAVAILABLE)
           << "goog_100g_fec_control_set() is not available!";
  } else if (fec && autoneg) {
    return MAKE_ERROR(ERR_INTERNAL)
           << "Cannot have both FEC and autogen ON for "
           << "(unit, port) = (" << unit << ", " << port << ").";
  }

  // Apply Phy control for port. Unfortunately this part is a bit
  // chip-dependant.
  if (chip_type == BcmChip::TOMAHAWK || chip_type == BcmChip::TOMAHAWK_PLUS) {
    RETURN_IF_BCM_ERROR(bcm_port_pause_set(unit, port, 0, 0));
    if (!autoneg) {
      RETURN_IF_BCM_ERROR(
          bcm_port_duplex_set(unit, port, BCM_PORT_DUPLEX_FULL));
    }
    // Unreliable LOS is equivalent to SwRxLOS. Broadcom initially recommended
    // setting this only for 10G/40G optics, but later extended it for all
    // optics. See g/bcmsdk-support/ydPxoUf4iRk/1P-3_QkXCQAJ for discussion.
    if (bcm_port_intf == BCM_PORT_IF_SR) {
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_UNRELIABLE_LOS, 1));
    }
  } else if (chip_type == BcmChip::TRIDENT2) {
    RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
        unit, port, BCM_PORT_PHY_CONTROL_SOFTWARE_RX_LOS, 1));
    // TODO(unknown): This may not be necessary anymore. Remove if not needed.
    // We needed this a long time ago.
    if (bcm_port_intf == BCM_PORT_IF_SR) {
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_RX_PEAK_FILTER_TEMP_COMP, 1));
    }
    // Sets the serdes firmware mode for Trident2 chips on the fly, based on
    // the interface type.
    _shr_port_phy_control_firmware_mode_t serdes_firmware_mode =
        BCM_PORT_PHY_FIRMWARE_DEFAULT;
    if (bcm_port_intf == BCM_PORT_IF_SR && speed_bps == kFortyGigBps) {
      serdes_firmware_mode = BCM_PORT_PHY_FIRMWARE_SFP_OPT_SR4;
    } else if (bcm_port_intf == BCM_PORT_IF_KR && speed_bps == kTwentyGigBps) {
      serdes_firmware_mode = BCM_PORT_PHY_FIRMWARE_DEFAULT;
    } else if (bcm_port_intf == BCM_PORT_IF_SR && speed_bps == kTenGigBps) {
      serdes_firmware_mode = BCM_PORT_PHY_FIRMWARE_DEFAULT;
    } else if (bcm_port_intf == BCM_PORT_IF_SFI && speed_bps == kTenGigBps) {
      serdes_firmware_mode = BCM_PORT_PHY_FIRMWARE_SFP_DAC;
    } else if (bcm_port_intf == BCM_PORT_IF_CR4 && speed_bps == kFortyGigBps) {
      serdes_firmware_mode = BCM_PORT_PHY_FIRMWARE_SFP_DAC;
    } else {
      MAKE_ERROR(ERR_INTERNAL)
          << "Unsupported bcm_port_intf and speed pair for (unit, port) = ("
          << unit << ", " << port << "): (" << bcm_port_intf << ", "
          << speed_bps << ").";
    }
    RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
        unit, port, BCM_PORT_PHY_CONTROL_FIRMWARE_MODE, serdes_firmware_mode));
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetSerdesRegisterForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, int serdes_lane,
    uint32 reg, uint32 value) {
  // Only T2 needs this.
  if (chip_type != BcmChip::TRIDENT2) return ::util::OkStatus();

  // TODO(unknown): NOT GOOD. We had to define the registers manually here.
  // Arent these defined in the SDK headers?
  const uint32 TRIDENT2_TX_ANALOG_CONTROL_REG = 0xc017;
  const uint32 TRIDENT2_TX_FIR_CONTROL_REG = 0xc252;
  switch (reg) {
    case TRIDENT2_TX_ANALOG_CONTROL_REG: {
      // POST2_COEFF is bits 14:12, IDRIVER is 11:8, IPREDRIVER is 7:4,
      // IFIR is 3:1.
      uint32 post2_coeff = (value & 0x7000) >> 12;
      uint32 idriver = (value & 0x0f00) >> 8;
      uint32 ipredriver = (value & 0x00f0) >> 4;
      uint32 ifir = (value & 0x000e) >> 1;
      CHECK_RETURN_IF_FALSE(ifir == 0)
          << "Detected non-zero IFIR field for (unit, port, reg, value) = ("
          << unit << ", " << port << ", " << reg << ", " << value << ").";
      // Set the pre, current, and post current drivers for all serdes lanes
      // associated with port.
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_PRE_DRIVER_CURRENT, ipredriver));
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_DRIVER_CURRENT, idriver));
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_DRIVER_POST2_CURRENT, post2_coeff));
      break;
    }
    case TRIDENT2_TX_FIR_CONTROL_REG:
      RETURN_IF_BCM_ERROR(bcm_port_phy_control_set(
          unit, port, BCM_PORT_PHY_CONTROL_PREEMPHASIS, value));
      break;
    default:
      return MAKE_ERROR(ERR_INTERNAL)
             << "Invalid SerDes register for (unit, port, reg, value) = ("
             << unit << ", " << port << ", " << reg << ", " << value << ").";
  }

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetSerdesAttributeForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, const std::string& attr,
    uint32 value) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
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

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
