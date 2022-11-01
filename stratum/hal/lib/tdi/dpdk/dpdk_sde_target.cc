// Copyright 2019-present Barefoot Networks, Inc.
// Copyright 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

// DPDK-specific SDE wrapper methods.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/synchronization/mutex.h"
#include "stratum/glue/integral_types.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/status/status.h"
#include "stratum/glue/status/statusor.h"
#include "stratum/hal/lib/common/common.pb.h"
#include "stratum/hal/lib/tdi/macros.h"
#include "stratum/hal/lib/tdi/tdi_sde_common.h"
#include "stratum/hal/lib/tdi/tdi_sde_helpers.h"
#include "stratum/hal/lib/tdi/tdi_sde_wrapper.h"
#include "stratum/lib/channel/channel.h"
#include "stratum/lib/utils.h"

extern "C" {
#include "bf_pal/bf_pal_port_intf.h"
#include "bf_pal/dev_intf.h"
#include "bf_switchd/lib/bf_switchd_lib_init.h"
#include "tdi_rt/tdi_rt_defs.h"
}  // extern "C"

namespace stratum {
namespace hal {
namespace tdi {

using namespace stratum::hal::tdi::helpers;

::util::StatusOr<PortState> TdiSdeWrapper::GetPortState(int device, int port) {
  return PORT_STATE_UP;
}

::util::Status TdiSdeWrapper::GetPortCounters(int device, int port,
                                              PortCounters* counters) {
  uint64_t stats[BF_PORT_NUM_COUNTERS] = {0};
  RETURN_IF_TDI_ERROR(
      bf_pal_port_all_stats_get(static_cast<bf_dev_id_t>(device),
                                static_cast<bf_dev_port_t>(port), stats));
  counters->set_in_octets(stats[RX_BYTES]);
  counters->set_out_octets(stats[TX_BYTES]);
  counters->set_in_unicast_pkts(stats[RX_PACKETS]);
  counters->set_out_unicast_pkts(stats[TX_PACKETS]);
  counters->set_in_broadcast_pkts(stats[RX_BROADCAST]);
  counters->set_out_broadcast_pkts(stats[TX_BROADCAST]);
  counters->set_in_multicast_pkts(stats[RX_MULTICAST]);
  counters->set_out_multicast_pkts(stats[TX_MULTICAST]);
  counters->set_in_discards(stats[RX_DISCARDS]);
  counters->set_out_discards(stats[TX_DISCARDS]);
  counters->set_in_unknown_protos(0);  // stat not meaningful
  counters->set_in_errors(stats[RX_ERRORS]);
  counters->set_out_errors(stats[TX_ERRORS]);
  counters->set_in_fcs_errors(0);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::RegisterPortStatusEventWriter(
    std::unique_ptr<ChannelWriter<PortStatusEvent>> writer) {
  absl::WriterMutexLock l(&port_status_event_writer_lock_);
  port_status_event_writer_ = std::move(writer);
  return ::util::OkStatus();
}

namespace {
dpdk_port_type_t get_target_port_type(DpdkPortType type) {
  switch (type) {
    case PORT_TYPE_VHOST:
      return BF_DPDK_LINK;
    case PORT_TYPE_TAP:
      return BF_DPDK_TAP;
    case PORT_TYPE_LINK:
      return BF_DPDK_LINK;
    case PORT_TYPE_SOURCE:
      return BF_DPDK_SOURCE;
    case PORT_TYPE_SINK:
      return BF_DPDK_SINK;
    default:
      break;
  }
  return BF_DPDK_PORT_MAX;
}
}  // namespace

::util::Status TdiSdeWrapper::GetPortInfo(int device, int port,
                                          TargetDatapathId* target_dp_id) {
  struct port_info_t* port_info = NULL;
  RETURN_IF_TDI_ERROR(bf_pal_port_info_get(static_cast<bf_dev_id_t>(device),
                                           static_cast<bf_dev_port_t>(port),
                                           &port_info));
  target_dp_id->set_tdi_portin_id((port_info)->port_attrib.port_in_id);
  target_dp_id->set_tdi_portout_id((port_info)->port_attrib.port_out_id);

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::HotplugPort(int device, int port,
                                          HotplugConfigParams& hotplug_config) {
  auto hotplug_attrs = absl::make_unique<hotplug_attributes_t>();
  strncpy(hotplug_attrs->qemu_socket_ip, hotplug_config.qemu_socket_ip.c_str(),
          sizeof(hotplug_attrs->qemu_socket_ip));
  strncpy(hotplug_attrs->qemu_vm_netdev_id,
          hotplug_config.qemu_vm_netdev_id.c_str(),
          sizeof(hotplug_attrs->qemu_vm_netdev_id));
  strncpy(hotplug_attrs->qemu_vm_chardev_id,
          hotplug_config.qemu_vm_chardev_id.c_str(),
          sizeof(hotplug_attrs->qemu_vm_chardev_id));
  strncpy(hotplug_attrs->qemu_vm_device_id,
          hotplug_config.qemu_vm_device_id.c_str(),
          sizeof(hotplug_attrs->qemu_vm_device_id));
  strncpy(hotplug_attrs->native_socket_path,
          hotplug_config.native_socket_path.c_str(),
          sizeof(hotplug_attrs->native_socket_path));
  // Convert enum to Boolean (NONE == false, ADD or DEL == true)
  hotplug_attrs->qemu_hotplug = (hotplug_config.qemu_hotplug_mode != 0);
  hotplug_attrs->qemu_socket_port = hotplug_config.qemu_socket_port;
  uint64 mac_address = hotplug_config.qemu_vm_mac_address;

  std::string string_mac =
      (absl::StrFormat("%02x:%02x:%02x:%02x:%02x:%02x",
                       (mac_address >> 40) & 0xFF, (mac_address >> 32) & 0xFF,
                       (mac_address >> 24) & 0xFF, (mac_address >> 16) & 0xFF,
                       (mac_address >> 8) & 0xFF, mac_address & 0xFF));
  strcpy(hotplug_attrs->qemu_vm_mac_address, string_mac.c_str());

  LOG(INFO) << "Parameters for hotplug are:"
            << " qemu_socket_port=" << hotplug_attrs->qemu_socket_port
            << " qemu_vm_mac_address=" << hotplug_attrs->qemu_vm_mac_address
            << " qemu_socket_ip=" << hotplug_attrs->qemu_socket_ip
            << " qemu_vm_netdev_id=" << hotplug_attrs->qemu_vm_netdev_id
            << " qemu_vm_chardev_id=" << hotplug_attrs->qemu_vm_chardev_id
            << " qemu_vm_device_id=" << hotplug_attrs->qemu_vm_device_id
            << " native_socket_path=" << hotplug_attrs->native_socket_path
            << " qemu_hotplug=" << hotplug_attrs->qemu_hotplug;

  if (hotplug_config.qemu_hotplug_mode == HOTPLUG_MODE_ADD) {
    RETURN_IF_TDI_ERROR(bf_pal_hotplug_add(static_cast<bf_dev_id_t>(device),
                                           static_cast<bf_dev_port_t>(port),
                                           hotplug_attrs.get()));
  } else if (hotplug_config.qemu_hotplug_mode == HOTPLUG_MODE_DEL) {
    RETURN_IF_TDI_ERROR(bf_pal_hotplug_del(static_cast<bf_dev_id_t>(device),
                                           static_cast<bf_dev_port_t>(port),
                                           hotplug_attrs.get()));
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::AddPort(int device, int port, uint64 speed_bps,
                                      FecMode fec_mode) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "AddPort(device, port, speed, fec_mode) not supported";
}

::util::Status TdiSdeWrapper::AddPort(int device, int port, uint64 speed_bps,
                                      const PortConfigParams& config,
                                      FecMode fec_mode) {
  static int port_in;
  static int port_out;

  auto port_attrs = absl::make_unique<port_attributes_t>();
  strncpy(port_attrs->port_name, config.port_name.c_str(),
          sizeof(port_attrs->port_name));
  strncpy(port_attrs->pipe_in, config.pipeline_name.c_str(),
          sizeof(port_attrs->pipe_in));
  strncpy(port_attrs->pipe_out, config.pipeline_name.c_str(),
          sizeof(port_attrs->pipe_out));
  strncpy(port_attrs->mempool_name, config.mempool_name.c_str(),
          sizeof(port_attrs->mempool_name));
  port_attrs->port_type = get_target_port_type(config.port_type);
  port_attrs->port_dir = PM_PORT_DIR_DEFAULT;
  port_attrs->port_in_id = port_in++;
  port_attrs->port_out_id = port_out++;
  port_attrs->net_port = config.packet_dir;

  LOG(INFO) << "Parameters for backend are:"
            << " port_name=" << port_attrs->port_name
            << " port_type=" << port_attrs->port_type
            << " port_in_id=" << port_attrs->port_in_id
            << " port_out_id=" << port_attrs->port_out_id
            << " pipeline_in_name=" << port_attrs->pipe_in
            << " pipeline_out_name=" << port_attrs->pipe_out
            << " mempool_name=" << port_attrs->mempool_name
            << " net_port=" << port_attrs->net_port
            << " sdk_port_id = " << port;

  if (port_attrs->port_type == BF_DPDK_LINK) {
    // Update LINK parameters
    if (config.port_type == PORT_TYPE_VHOST) {
      port_attrs->link.dev_hotplug_enabled = 1;
      strncpy(port_attrs->link.pcie_domain_bdf, config.port_name.c_str(),
              sizeof(port_attrs->link.pcie_domain_bdf));
      snprintf(port_attrs->link.dev_args, DEV_ARGS_LEN, "iface=%s,queues=%d",
               config.socket_path.c_str(), config.queues);
    } else {
      strncpy(port_attrs->link.pcie_domain_bdf, config.pci_bdf.c_str(),
              sizeof(port_attrs->link.pcie_domain_bdf));
    }
    LOG(INFO) << "LINK Parameters of the port are "
              << " pcie_domain_bdf=" << port_attrs->link.pcie_domain_bdf
              << " dev_args=" << port_attrs->link.dev_args;
  } else if (port_attrs->port_type == BF_DPDK_TAP) {
    port_attrs->tap.mtu = config.mtu;
    LOG(INFO) << "TAP Parameters of the port are "
              << "mtu = " << port_attrs->tap.mtu;
  }

  auto bf_status =
      bf_pal_port_add(static_cast<bf_dev_id_t>(device),
                      static_cast<bf_dev_port_t>(port), port_attrs.get());
  if (bf_status != BF_SUCCESS) {
    // Revert the port_in and port_out values
    port_in--;
    port_out--;
    RETURN_IF_TDI_ERROR(bf_status);
  }

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::DeletePort(int device, int port) {
  RETURN_IF_TDI_ERROR(bf_pal_port_del(static_cast<bf_dev_id_t>(device),
                                      static_cast<bf_dev_port_t>(port)));
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::EnablePort(int device, int port) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "EnablePort not implemented";
}

::util::Status TdiSdeWrapper::DisablePort(int device, int port) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "DisablePort not implemented";
}

::util::Status TdiSdeWrapper::SetPortShapingRate(int device, int port,
                                                 bool is_in_pps,
                                                 uint32 burst_size,
                                                 uint64 rate_per_second) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "SetPortShapingRate not supported";
}

::util::Status TdiSdeWrapper::EnablePortShaping(int device, int port,
                                                TriState enable) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "EnablePortShaping not supported";
}

::util::Status TdiSdeWrapper::SetPortAutonegPolicy(int device, int port,
                                                   TriState autoneg) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "SetPortAutonegPolicy not supported";
}

::util::Status TdiSdeWrapper::SetPortMtu(int device, int port, int32 mtu) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED) << "SetPortMtu not implemented";
}

// Should this return ::util::StatusOr<bool>?
bool TdiSdeWrapper::IsValidPort(int device, int port) {
  // NOTE: Method returns bool. What is BF_SUCCESS (an enum) doing here?
  // Is the method supposed to succeed or fail? The name suggests
  // that it is supposed to succeed, but BF_SUCCESS == 0, which when
  // converted to a Boolean is FALSE, so it is actually failure.
  return BF_SUCCESS;
}

::util::Status TdiSdeWrapper::SetPortLoopbackMode(int device, int port,
                                                  LoopbackState loopback_mode) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED)
         << "SetPortLoopbackMode not supported";
}

::util::StatusOr<bool> TdiSdeWrapper::IsSoftwareModel(int device) {
  return true;
}

std::string TdiSdeWrapper::GetChipType(int device) const { return "DPDK"; }

std::string TdiSdeWrapper::GetSdeVersion() const {
  // TODO tdi version
  return "1.0.0";
}

::util::StatusOr<uint32> TdiSdeWrapper::GetPortIdFromPortKey(
    int device, const PortKey& port_key) {
  const int port = port_key.port;
  RET_CHECK(port >= 0) << "Port ID must be non-negative. Attempted to get port "
                       << port << " on dev " << device << ".";

  // PortKey uses three possible values for channel:
  //     > 0: port is channelized (first channel is 1)
  //     0: port is not channelized
  //     < 0: port channel is not important (e.g. for port groups)
  // BF SDK expects the first channel to be 0
  //     Convert base-1 channel to base-0 channel if port is channelized
  //     Otherwise, port is already 0 in the non-channelized case
  const int channel =
      (port_key.channel > 0) ? port_key.channel - 1 : port_key.channel;
  RET_CHECK(channel >= 0) << "Channel must be set for port " << port
                          << " on dev " << device << ".";

  char port_string[MAX_PORT_HDL_STRING_LEN];
  int r = snprintf(port_string, sizeof(port_string), "%d/%d", port, channel);
  RET_CHECK(r > 0 && r < sizeof(port_string))
      << "Failed to build port string for port " << port << " channel "
      << channel << " on dev " << device << ".";

  bf_dev_port_t dev_port;
  RETURN_IF_TDI_ERROR(bf_pal_port_str_to_dev_port_map(
      static_cast<bf_dev_id_t>(device), port_string, &dev_port));
  return static_cast<uint32>(dev_port);
}

::util::StatusOr<int> TdiSdeWrapper::GetPcieCpuPort(int device) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED) << "GetPcieCpuPort not supported";
}

::util::Status TdiSdeWrapper::SetTmCpuPort(int device, int port) {
  return MAKE_ERROR(ERR_OPER_NOT_SUPPORTED) << "SetTmCpuPort not supported";
}

::util::Status TdiSdeWrapper::SetDeflectOnDropDestination(int device, int port,
                                                          int queue) {
  return MAKE_ERROR(ERR_UNIMPLEMENTED)
         << "SetDeflectOnDropDestination not implemented";
}

::util::Status TdiSdeWrapper::InitializeSde(const std::string& sde_install_path,
                                            const std::string& sde_config_file,
                                            bool run_in_background) {
  RET_CHECK(sde_install_path != "") << "sde_install_path is required";
  RET_CHECK(sde_config_file != "") << "sde_config_file is required";

  // Parse bf_switchd arguments.
  auto switchd_main_ctx = absl::make_unique<bf_switchd_context_t>();
  switchd_main_ctx->install_dir = strdup(sde_install_path.c_str());
  switchd_main_ctx->conf_file = strdup(sde_config_file.c_str());
  switchd_main_ctx->skip_p4 = true;
  if (run_in_background) {
    switchd_main_ctx->running_in_background = true;
  } else {
    switchd_main_ctx->shell_set_ucli = true;
  }

  // Determine if kernel mode packet driver is loaded.
  std::string bf_sysfs_fname;
  {
    char buf[128] = {};
    RETURN_IF_TDI_ERROR(switch_pci_sysfs_str_get(buf, sizeof(buf)));
    bf_sysfs_fname = buf;
  }
  absl::StrAppend(&bf_sysfs_fname, "/dev_add");
  LOG(INFO) << "bf_sysfs_fname: " << bf_sysfs_fname;
  if (PathExists(bf_sysfs_fname)) {
    // Override previous parsing if bf_kpkt KLM was loaded.
    LOG(INFO)
        << "kernel mode packet driver present, forcing kernel_pkt option!";
  }

  RETURN_IF_TDI_ERROR(bf_switchd_lib_init(switchd_main_ctx.get()))
      << "Error when starting switchd.";
  LOG(INFO) << "switchd started successfully";

  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::AddDevice(int dev_id,
                                        const TdiDeviceConfig& device_config) {
  const ::tdi::Device* device = nullptr;
  absl::WriterMutexLock l(&data_lock_);

  RET_CHECK(device_config.programs_size() > 0);

  tdi_id_mapper_.reset();

  RETURN_IF_TDI_ERROR(bf_pal_device_warm_init_begin(dev_id,
                                                    BF_DEV_WARM_INIT_FAST_RECFG,
                                                    /* upgrade_agents */ true));
  bf_device_profile_t device_profile = {};

  // Commit new files to disk and build device profile for SDE to load.
  RETURN_IF_ERROR(RecursivelyCreateDir(FLAGS_tdi_sde_config_dir));
  // Need to extend the lifetime of the path strings until the SDE reads them.
  std::vector<std::unique_ptr<std::string>> path_strings;
  device_profile.num_p4_programs = device_config.programs_size();
  for (int i = 0; i < device_config.programs_size(); ++i) {
    const auto& program = device_config.programs(i);
    const std::string program_path =
        absl::StrCat(FLAGS_tdi_sde_config_dir, "/", program.name());
    auto tdi_path = absl::make_unique<std::string>(
        absl::StrCat(program_path, "/bfrt.json"));
    RETURN_IF_ERROR(RecursivelyCreateDir(program_path));
    RETURN_IF_ERROR(WriteStringToFile(program.bfrt(), *tdi_path));

    bf_p4_program_t* p4_program = &device_profile.p4_programs[i];
    ::snprintf(p4_program->prog_name, _PI_UPDATE_MAX_NAME_SIZE, "%s",
               program.name().c_str());
    p4_program->bfrt_json_file = &(*tdi_path)[0];
    p4_program->num_p4_pipelines = program.pipelines_size();
    path_strings.emplace_back(std::move(tdi_path));
    RET_CHECK(program.pipelines_size() > 0);
    for (int j = 0; j < program.pipelines_size(); ++j) {
      const auto& pipeline = program.pipelines(j);
      const std::string pipeline_path =
          absl::StrCat(program_path, "/", pipeline.name());
      auto context_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/context.json"));
      auto config_path = absl::make_unique<std::string>(
          absl::StrCat(pipeline_path, "/tofino.bin"));
      RETURN_IF_ERROR(RecursivelyCreateDir(pipeline_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.context(), *context_path));
      RETURN_IF_ERROR(WriteStringToFile(pipeline.config(), *config_path));

      bf_p4_pipeline_t* pipeline_profile = &p4_program->p4_pipelines[j];
      ::snprintf(pipeline_profile->p4_pipeline_name, _PI_UPDATE_MAX_NAME_SIZE,
                 "%s", pipeline.name().c_str());
      pipeline_profile->cfg_file = &(*config_path)[0];
      pipeline_profile->runtime_context_file = &(*context_path)[0];
      path_strings.emplace_back(std::move(config_path));
      path_strings.emplace_back(std::move(context_path));

      RET_CHECK(pipeline.scope_size() <= MAX_P4_PIPELINES);
      pipeline_profile->num_pipes_in_scope = pipeline.scope_size();
      for (int p = 0; p < pipeline.scope_size(); ++p) {
        const auto& scope = pipeline.scope(p);
        pipeline_profile->pipe_scope[p] = scope;
      }
    }
  }

  // This call re-initializes most SDE components.
  RETURN_IF_TDI_ERROR(bf_pal_device_add(dev_id, &device_profile));
  RETURN_IF_TDI_ERROR(bf_pal_device_warm_init_end(dev_id));

  // Set SDE log levels for modules of interest.
  // TODO(max): create story around SDE logs. How to get them into glog? What
  // levels to enable for which modules?
  RET_CHECK(
      bf_sys_log_level_set(BF_MOD_BFRT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  RET_CHECK(bf_sys_log_level_set(BF_MOD_PKT, BF_LOG_DEST_STDOUT, BF_LOG_WARN) ==
            0);
  RET_CHECK(
      bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT, BF_LOG_WARN) == 0);
  if (VLOG_IS_ON(2)) {
    RET_CHECK(bf_sys_log_level_set(BF_MOD_PIPE, BF_LOG_DEST_STDOUT,
                                   BF_LOG_WARN) == 0);
  }

  ::tdi::DevMgr::getInstance().deviceGet(dev_id, &device);
  RETURN_IF_TDI_ERROR(
      device->tdiInfoGet(device_config.programs(0).name(), &tdi_info_));

  // FIXME: if all we ever do is create and push, this could be one call.
  tdi_id_mapper_ = TdiIdMapper::CreateInstance();
  RETURN_IF_ERROR(
      tdi_id_mapper_->PushForwardingPipelineConfig(device_config, tdi_info_));

  return ::util::OkStatus();
}

//  Packetio

::util::Status TdiSdeWrapper::TxPacket(int device, const std::string& buffer) {
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::StartPacketIo(int device) {
  return ::util::OkStatus();
}

::util::Status TdiSdeWrapper::StopPacketIo(int device) {
  return ::util::OkStatus();
}

}  // namespace tdi
}  // namespace hal
}  // namespace stratum
