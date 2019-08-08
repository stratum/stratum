// Copyright 2018 Google LLC
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

#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"

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
#include <utility>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"

#include "gflags/gflags.h"
#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/substitute.h"
#include "stratum/glue/logging.h"
#include "stratum/hal/lib/bcm/constants.h"
#include "stratum/hal/lib/bcm/macros.h"
#include "stratum/hal/lib/common/constants.h"
#include "stratum/lib/constants.h"
#include "stratum/lib/macros.h"
#include "stratum/lib/utils.h"
// #include "util/endian/endian.h"
#include "stratum/glue/gtl/map_util.h"
#include "stratum/glue/gtl/stl_util.h"

DEFINE_int64(linkscan_interval_in_usec, 200000, "Linkscan interval in usecs.");
DEFINE_int32(max_num_linkscan_writers, 10,
             "Max number of linkscan event Writers supported.");
DECLARE_string(bcm_sdk_checkpoint_dir);

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
constexpr int BcmSdkWrapper::kRedCounterIndex;
constexpr int BcmSdkWrapper::kGreenCounterIndex;

// All the C style functions and vars used to work with BCM sdk need to be
// put into the following unnamed namespace.
namespace {

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

// Converts MAC address given as bcm_mac_t to uint64 in host order. In this
// byte array the MSB is at the byte with the lowest index.
uint64 BcmMacToUint64(const uint8 bcm_mac[6]) {
  uint64 mac = 0;
  for (int i = 0; i < 6; ++i) {
    mac = (mac << 8) | (bcm_mac[i] & 0xff);
  }
  return mac;
}

// Converts MAC address as uint64 in host order to bcm_mac_t byte array. In
// this byte array the MSB is at the byte with the lowest index.
void Uint64ToBcmMac(uint64 mac, uint8 (*bcm_mac)[6]) {
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

}  // namespace

BcmSdkWrapper* BcmSdkWrapper::singleton_ = nullptr;
ABSL_CONST_INIT absl::Mutex BcmSdkWrapper::init_lock_(absl::kConstInit);

BcmSdkWrapper::BcmSdkWrapper(BcmDiagShell* bcm_diag_shell)
    : unit_to_chip_type_(),
      unit_to_soc_device_(),
      bcm_diag_shell_(bcm_diag_shell),
      linkscan_event_writers_() {
  // TODO(unknown): Add implementation.
}

BcmSdkWrapper::~BcmSdkWrapper() { ShutdownAllUnits().IgnoreError(); }

::util::Status BcmSdkWrapper::InitializeSdk(
    const std::string& config_file_path,
    const std::string& config_flush_file_path,
    const std::string& bcm_shell_log_file_path) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::FindUnit(int unit, int pci_bus, int pci_slot,
                                       BcmChip::BcmChipType chip_type) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitializeUnit(int unit, bool warm_boot) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ShutdownUnit(int unit) {
  // TODO(unknown): Implement this function.
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
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitializePort(int unit, int port) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetPortOptions(int unit, int port,
                                             const BcmPortOptions& options) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetPortOptions(int unit, int port,
                                             BcmPortOptions* options) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartDiagShellServer() {
  if (bcm_diag_shell_ == nullptr) return ::util::OkStatus();  // sim mode
  RETURN_IF_ERROR(bcm_diag_shell_->StartServer());

  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartLinkscan(int unit) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopLinkscan(int unit) {
  // TODO(unknown): Implement this function.
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
  // TODO(craigs): needs porting to SDKLT
  // int linkscan_mode = BCM_LINKSCAN_MODE_NONE;
  // RETURN_IF_BCM_ERROR(bcm_port_linkscan_get(unit, port, &linkscan_mode));
  int linkscan_mode = 0;
  // Convert the BCM returned int value to the enum value defined in bcm.proto
  // Note that BCM_LINKSCAN_MODE_COUNT = 3 will never be returned by
  // bcm_port_linkscan_get
  // if (static_cast<bcm_linkscan_mode_e>(linkscan_mode) ==
  //     BCM_LINKSCAN_MODE_NONE) {
  //   return BcmPortOptions::LINKSCAN_MODE_NONE;
  // }
  return static_cast<BcmPortOptions::LinkscanMode>(linkscan_mode);
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
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateL3RouterIntf is not implemented.";
}

::util::Status BcmSdkWrapper::DeleteL3RouterIntf(int unit, int router_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3CpuEgressIntf(int unit) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateL3CpuEgressIntf is not implemented.";
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3PortEgressIntf(
    int unit, uint64 nexthop_mac, int port, int vlan, int router_intf_id) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateL3PortEgressIntf is not implemented.";
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3TrunkEgressIntf(
    int unit, uint64 nexthop_mac, int trunk, int vlan, int router_intf_id) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateL3TrunkEgressIntf is not implemented.";
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateL3DropIntf(int unit) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateL3DropIntf is not implemented.";
}

::util::Status BcmSdkWrapper::ModifyL3CpuEgressIntf(int unit,
                                                    int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3PortEgressIntf(int unit,
                                                     int egress_intf_id,
                                                     uint64 nexthop_mac,
                                                     int port, int vlan,
                                                     int router_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3TrunkEgressIntf(int unit,
                                                      int egress_intf_id,
                                                      uint64 nexthop_mac,
                                                      int trunk, int vlan,
                                                      int router_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3DropIntf(int unit, int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3EgressIntf(int unit, int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::FindRouterIntfFromEgressIntf(
    int unit, int egress_intf_id) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindRouterIntfFromEgressIntf is not implemented.";
}

::util::StatusOr<int> BcmSdkWrapper::FindOrCreateEcmpEgressIntf(
    int unit, const std::vector<int>& member_ids) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED)
      << "FindOrCreateEcmpEgressIntf is not implemented.";
}

::util::Status BcmSdkWrapper::ModifyEcmpEgressIntf(
    int unit, int egress_intf_id, const std::vector<int>& member_ids) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteEcmpEgressIntf(int unit,
                                                   int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3RouteIpv4(int unit, int vrf, uint32 subnet,
                                             uint32 mask, int class_id,
                                             int egress_intf_id,
                                             bool is_intf_multipath) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3RouteIpv6(int unit, int vrf,
                                             const std::string& subnet,
                                             const std::string& mask,
                                             int class_id, int egress_intf_id,
                                             bool is_intf_multipath) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                            int class_id, int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddL3HostIpv6(int unit, int vrf,
                                            const std::string& ipv6,
                                            int class_id, int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask,
                                                int class_id,
                                                int egress_intf_id,
                                                bool is_intf_multipath) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3RouteIpv6(
    int unit, int vrf, const std::string& subnet, const std::string& mask,
    int class_id, int egress_intf_id, bool is_intf_multipath) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv4(int unit, int vrf, uint32 ipv4,
                                               int class_id,
                                               int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ModifyL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6,
                                               int class_id,
                                               int egress_intf_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv4(int unit, int vrf,
                                                uint32 subnet, uint32 mask) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3RouteIpv6(int unit, int vrf,
                                                const std::string& subnet,
                                                const std::string& mask) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv4(int unit, int vrf, uint32 ipv4) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL3HostIpv6(int unit, int vrf,
                                               const std::string& ipv6) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::AddMyStationEntry(int unit, int priority,
                                                       int vlan, int vlan_mask,
                                                       uint64 dst_mac,
                                                       uint64 dst_mac_mask) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED) << "AddMyStationEntry is not implemented.";
}

::util::Status BcmSdkWrapper::DeleteMyStationEntry(int unit, int station_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteL2EntriesByVlan(int unit, int vlan) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddVlanIfNotFound(int unit, int vlan) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DeleteVlanIfFound(int unit, int vlan) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureVlanBlock(int unit, int vlan,
                                                 bool block_broadcast,
                                                 bool block_known_multicast,
                                                 bool block_unknown_multicast,
                                                 bool block_unknown_unicast) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigureL2Learning(int unit, int vlan,
                                                  bool disable_l2_learning) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetL2AgeTimer(int unit, int l2_age_duration_sec) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::ConfigSerdesForPort(
    int unit, int port, uint64 speed_bps, int serdes_core, int serdes_lane,
    int serdes_num_lanes, const std::string& intf_type,
    const SerdesRegisterConfigs& serdes_register_configs,
    const SerdesAttrConfigs& serdes_attr_configs) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::CreateKnetIntf(int unit, int vlan,
                                             std::string* netif_name,
                                             int* netif_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::DestroyKnetIntf(int unit, int netif_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::CreateKnetFilter(int unit, int netif_id,
                                                      KnetFilterType type) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED) << "CreateKnetFilter is not implemented.";
}

::util::Status BcmSdkWrapper::DestroyKnetFilter(int unit, int filter_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StartRx(int unit, const RxConfig& rx_config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::StopRx(int unit) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetRateLimit(
    int unit, const RateLimitConfig& rate_limit_config) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetKnetHeaderForDirectTx(int unit, int port,
                                                       int cos, uint64 smac,
                                                       std::string* header) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetKnetHeaderForIngressPipelineTx(
    int unit, uint64 smac, std::string* header) {
  // TODO(unknown): Implement this function.
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
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::InitAclHardware(int unit) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclControl(int unit,
                                            const AclControl& acl_control) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclUdfChunks(int unit, const BcmUdfSet& udfs) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclUdfChunks(int unit, BcmUdfSet* udfs) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::CreateAclTable(int unit,
                                                    const BcmAclTable& table) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED) << "CreateAclTable is not implemented.";
}

::util::Status BcmSdkWrapper::DestroyAclTable(int unit, int table_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<int> BcmSdkWrapper::InsertAclFlow(int unit,
                                                   const BcmFlowEntry& flow,
                                                   bool add_stats,
                                                   bool color_aware) {
  // TODO(unknown): Implement this function.
  RETURN_ERROR(ERR_UNIMPLEMENTED) << "InsertAclFlow is not implemented.";
}

::util::Status BcmSdkWrapper::ModifyAclFlow(int unit, int flow_id,
                                            const BcmFlowEntry& flow) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RemoveAclFlow(int unit, int flow_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetAclPolicer(int unit, int flow_id,
                                            const BcmMeterConfig& meter) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclTable(int unit, int table_id,
                                          BcmAclTable* table) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

namespace {

// TODO(unknown): use util/endian/endian.h?
inline uint64 ntohll(uint64 n) { return ntohl(1) == 1 ? n : bswap_64(n); }

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
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::StatusOr<std::string> BcmSdkWrapper::MatchAclFlow(
    int unit, int flow_id, const BcmFlowEntry& flow) {
  // TODO(unknown): Implement this function.
  return std::string();
}

::util::Status BcmSdkWrapper::GetAclTableFlowIds(int unit, int table_id,
                                                 std::vector<int>* flow_ids) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::AddAclStats(int unit, int table_id, int flow_id,
                                          bool color_aware) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::RemoveAclStats(int unit, int flow_id) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::GetAclStats(int unit, int flow_id,
                                          BcmAclStats* stats) {
  // TODO(unknown): Implement this function.
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

pthread_t BcmSdkWrapper::GetDiagShellThreadId() const {
  if (bcm_diag_shell_ == nullptr) return 0;  // sim mode
  return bcm_diag_shell_->GetDiagShellThreadId();
}

::util::Status BcmSdkWrapper::CleanupKnet(int unit) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::OpenSdkCheckpointFile(int unit) {
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
  // TODO(unknown): Implement this function.
  // Find the checkpoint file size for this unit.
  ASSIGN_OR_RETURN(const int checkpoint_file_size,
                   FindSdkCheckpointFileSize(unit));
  CHECK_RETURN_IF_FALSE(checkpoint_file_size >= 0)
      << "Invalid SDK checkpoint file size for unit " << unit << ".";
  return ::util::OkStatus();
}

::util::StatusOr<std::string> BcmSdkWrapper::FindSdkCheckpointFilePath(
    int unit) {
  return std::string(absl::Substitute("$0/bcm-sdk-checkpoint-unit$1.bin",
                                      FLAGS_bcm_sdk_checkpoint_dir, unit));
}

::util::StatusOr<int> BcmSdkWrapper::FindSdkCheckpointFileSize(int unit) {
  // TODO(unknown): Implement this function.
  return kSdkCheckpointFileSize;
}

::util::StatusOr<BcmChip::BcmChipType> BcmSdkWrapper::GetChipType(int unit) {
  // TODO(unknown): Implement this function.
  return BcmChip::UNKNOWN;
}

::util::Status BcmSdkWrapper::SetIntfAndConfigurePhyForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, uint64 speed_bps,
    const std::string& intf_type) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetSerdesRegisterForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, int serdes_lane,
    uint32 reg, uint32 value) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

::util::Status BcmSdkWrapper::SetSerdesAttributeForPort(
    int unit, int port, BcmChip::BcmChipType chip_type, const std::string& attr,
    uint32 value) {
  // TODO(unknown): Implement this function.
  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum
