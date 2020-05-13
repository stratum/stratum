// Copyright 2020-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <iostream>
#include <list>
#include <memory>
#include <random>
#include <vector>

#include "gflags/gflags.h"
#include "stratum/glue/init_google.h"
#include "stratum/glue/logging.h"
#include "stratum/glue/net_util/ipaddress.h"
#include "stratum/hal/lib/bcm/bcm_acl_manager.h"
#include "stratum/hal/lib/bcm/bcm_chassis_manager.h"
#include "stratum/hal/lib/bcm/bcm_diag_shell.h"
#include "stratum/hal/lib/bcm/bcm_l2_manager.h"
#include "stratum/hal/lib/bcm/bcm_l3_manager.h"
#include "stratum/hal/lib/bcm/bcm_node.h"
#include "stratum/hal/lib/bcm/bcm_packetio_manager.h"
#include "stratum/hal/lib/bcm/bcm_sdk_wrapper.h"
#include "stratum/hal/lib/bcm/bcm_serdes_db_manager.h"
#include "stratum/hal/lib/bcm/bcm_switch.h"
#include "stratum/hal/lib/common/hal.h"
#include "stratum/hal/lib/p4/p4_table_mapper.h"
// #include "stratum/hal/lib/phal/legacy_phal.h"
// #include "stratum/hal/lib/phal/udev.h"
#include "absl/memory/memory.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "stratum/hal/lib/phal/phal.h"
#include "stratum/lib/security/auth_policy_checker.h"
#include "stratum/lib/security/credentials_manager.h"

DEFINE_int32(max_units, 1,
             "Maximum number of units supported on the switch platform.");

DECLARE_string(bcm_sdk_config_file);
DECLARE_string(bcm_sdk_config_flush_file);
DECLARE_string(bcm_sdk_shell_log_file);

namespace stratum {
namespace hal {
namespace bcm {

constexpr int kMaxL3RouteTableSize = 16000;
constexpr int kMaxL3HostTableSize = 14900;
constexpr int reporting_step = 100;

int unit = 0;
int vrf = 0;
int class_id = 0;
int port = 50;
int vlan = 1;

::util::Status L3RouteBenchmark(BcmSdkInterface* bcm_sdk_wrapper) {
  int subnet_base = 0x0a000000;
  int mask_base = 0xffffffff;

  ASSIGN_OR_RETURN(int router_intf, bcm_sdk_wrapper->FindOrCreateL3RouterIntf(
                                        unit, 0x000000bbbbbb, 1));
  ASSIGN_OR_RETURN(int egress_intf,
                   bcm_sdk_wrapper->FindOrCreateL3PortEgressIntf(
                       unit, 0x000000aaaaaa, port, vlan, router_intf));

  std::vector<std::pair<std::function<int(void)>, std::function<int(void)>>>
      dists;
  dists.push_back(
      std::make_pair([subnet_base, n = subnet_base]() mutable { return n++; },
                     []() { return 0xffffffff; }));
  // dists.push_back(std::make_pair([subnet_base, n = subnet_base]() mutable {
  // return n--; },
  //                                []() { return 0xffffffff; }));
  // dists.push_back(
  //     std::make_pair([rng = std::mt19937()]() mutable { return rng(); },
  //                    []() { return 0xffffffff; }));
  // dists.push_back(std::make_pair(
  //     [rng = std::mt19937(),
  //      dis = std::uniform_int_distribution<int>(0, 255 * 255 * 255)]()
  //      mutable {
  //       return dis(rng) << 8;
  //     },
  //     []() { return 0xffffff00; }));

  for (const auto& dist : dists) {
    LOG(INFO) << "## L3 Routing Table ##";
    // Generate subnets to insert
    std::vector<int> subnets(kMaxL3RouteTableSize);
    std::generate(subnets.begin(), subnets.end(), dist.first);
    std::vector<int> masks(kMaxL3RouteTableSize);
    std::generate(masks.begin(), masks.end(), dist.second);

    // Benchmark
    absl::Time t1 = absl::Now();
    absl::Time interval = absl::Now();
    std::vector<absl::Duration> intervals;
    for (size_t i = 0; i < subnets.size(); ++i) {
      const auto& subnet = subnets[i];
      const auto& mask = masks[i];
      if (i % reporting_step == 0 && i != 0) {
        absl::Time t = absl::Now();
        intervals.push_back(t - interval);
        interval = t;
      }
      RETURN_IF_ERROR(bcm_sdk_wrapper->AddL3RouteIpv4(
          unit, vrf, subnet, mask, class_id, egress_intf, false));
    }
    absl::Time t2 = absl::Now();

    // Reporting
    auto d = t2 - t1;
    auto flows_per_second = kMaxL3RouteTableSize / absl::ToDoubleSeconds(d);
    LOG(INFO) << "Inserting " << kMaxL3RouteTableSize
              << " L3 routing table entries (bcm_l3_route_add) took "
              << absl::ToDoubleSeconds(d) << " seconds, " << flows_per_second
              << " flows/s, "
              << absl::ToDoubleMicroseconds(d) / kMaxL3RouteTableSize
              << " us/flow.";
    for (size_t i = 0; i < intervals.size(); ++i) {
      const auto& d = intervals[i];
      LOG(INFO) << i * reporting_step + reporting_step << ", "
                << absl::ToDoubleMicroseconds(d) / reporting_step;
    }

    // Cleanup
    auto mask_it = masks.begin();
    absl::Time t3 = absl::Now();
    for (const auto& subnet : subnets) {
      RETURN_IF_ERROR(
          bcm_sdk_wrapper->DeleteL3RouteIpv4(unit, vrf, subnet, *mask_it++));
    }
    absl::Time t4 = absl::Now();
    auto d2 = t4 - t3;
    auto del_flows_per_second =
        kMaxL3RouteTableSize / absl::ToDoubleSeconds(d2);

    LOG(INFO) << "Deleting " << kMaxL3RouteTableSize
              << " L3 routing table entries (bcm_l3_route_delete) took "
              << absl::ToDoubleSeconds(d2) << " seconds, "
              << del_flows_per_second << " flows/s, "
              << absl::ToDoubleMicroseconds(d2) / kMaxL3RouteTableSize
              << " us/flow.";
  }

  return ::util::OkStatus();
}

::util::Status L3HostBenchmark(BcmSdkInterface* bcm_sdk_wrapper) {
  int ipv4_base = 0x1a000000;

  ASSIGN_OR_RETURN(int router_intf, bcm_sdk_wrapper->FindOrCreateL3RouterIntf(
                                        unit, 0x000000bbbbbb, 1));
  ASSIGN_OR_RETURN(int egress_intf,
                   bcm_sdk_wrapper->FindOrCreateL3PortEgressIntf(
                       unit, 0x000000aaaaaa, port, vlan, router_intf));

  std::random_device rd;
  std::vector<std::function<int(void)>> dists;
  dists.push_back([ipv4_base, n = ipv4_base]() mutable { return n++; });
  // dists.push_back([ipv4_base, n = ipv4_base]() mutable { return n--; });
  // dists.push_back([&rd, rng = std::mt19937(rd())]() mutable { return rng();
  // });

  for (const auto& dist : dists) {
    LOG(INFO) << "## L3 Host Table ##";
    // Generate ipv4s to insert
    std::vector<int> ipv4s(kMaxL3HostTableSize);
    std::generate(ipv4s.begin(), ipv4s.end(), dist);
    std::vector<absl::Duration> intervals;

    // Benchmark
    absl::Time t1 = absl::Now();
    absl::Time interval = absl::Now();
    for (size_t i = 0; i < ipv4s.size(); ++i) {
      // for (const auto& ipv4 : ipv4s) {
      const auto& ipv4 = ipv4s[i];
      if (i % reporting_step == 0 && i != 0) {
        absl::Time t = absl::Now();
        intervals.push_back(t - interval);
        interval = t;
      }
      RETURN_IF_ERROR_WITH_APPEND(bcm_sdk_wrapper->AddL3HostIpv4(
          unit, vrf, ipv4, class_id, egress_intf))
          << "Failed to add L3 host ip "
          << HostUInt32ToIPAddress(ipv4).ToString();
    }
    absl::Time t2 = absl::Now();

    // Reporting
    auto d = t2 - t1;
    auto flows_per_second = kMaxL3HostTableSize / absl::ToDoubleSeconds(d);
    LOG(INFO) << "Inserting " << kMaxL3HostTableSize
              << " L3 host table entries (bcm_l3_host_add) took "
              << absl::ToDoubleSeconds(d) << " seconds, " << flows_per_second
              << " flows/s, "
              << absl::ToDoubleMicroseconds(d) / kMaxL3HostTableSize
              << " us/flow.";

    for (size_t i = 0; i < intervals.size(); ++i) {
      const auto& d = intervals[i];
      LOG(INFO) << i * reporting_step + reporting_step << ", "
                << absl::ToDoubleMicroseconds(d) / reporting_step;
    }

    // Cleanup
    absl::Time t3 = absl::Now();
    for (const auto& ipv4 : ipv4s) {
      RETURN_IF_ERROR_WITH_APPEND(
          bcm_sdk_wrapper->DeleteL3HostIpv4(unit, vrf, ipv4))
          << "Failed to delete L3 host ip "
          << HostUInt32ToIPAddress(ipv4).ToString();
    }
    absl::Time t4 = absl::Now();
    auto d2 = t4 - t3;
    auto del_flows_per_second = kMaxL3HostTableSize / absl::ToDoubleSeconds(d2);

    LOG(INFO) << "Deleting " << kMaxL3HostTableSize
              << " L3 host table entries (bcm_l3_host_delete) took "
              << absl::ToDoubleSeconds(d2) << " seconds, "
              << del_flows_per_second << " flows/s, "
              << absl::ToDoubleMicroseconds(d2) / kMaxL3HostTableSize
              << " us/flow.";
  }

  return ::util::OkStatus();
}

::util::Status Main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  InitStratumLogging();

  LOG(INFO)
      << "Starting Stratum in STANDALONE mode for a Broadcom-based switch...";

  // Create chassis-wide and per-node class instances.
  auto* bcm_diag_shell = BcmDiagShell::CreateSingleton();
  auto* bcm_sdk_wrapper = BcmSdkWrapper::CreateSingleton(bcm_diag_shell);

  RETURN_IF_ERROR(bcm_sdk_wrapper->InitializeSdk(
      FLAGS_bcm_sdk_config_file, FLAGS_bcm_sdk_config_flush_file,
      FLAGS_bcm_sdk_shell_log_file));
  RETURN_IF_ERROR(bcm_sdk_wrapper->FindUnit(0, 1, 0, BcmChip::TOMAHAWK));
  RETURN_IF_ERROR(bcm_sdk_wrapper->InitializeUnit(0, false));
  RETURN_IF_ERROR(bcm_sdk_wrapper->SetModuleId(0, 0));
  RETURN_IF_ERROR(bcm_sdk_wrapper->StartDiagShellServer());
  RETURN_IF_ERROR(bcm_sdk_wrapper->SetMtu(0, kDefaultMtu));

  // Benchmarks
  {
    auto result = L3HostBenchmark(bcm_sdk_wrapper);
    if (!result.ok()) {
      LOG(ERROR) << result;
    }
  }
  {
    auto result = L3RouteBenchmark(bcm_sdk_wrapper);
    if (!result.ok()) {
      LOG(ERROR) << result;
    }
  }

  RETURN_IF_ERROR(bcm_sdk_wrapper->ShutdownAllUnits());

  LOG(INFO) << "See you later!";
  return ::util::OkStatus();
}

}  // namespace bcm
}  // namespace hal
}  // namespace stratum

int main(int argc, char** argv) {
  return stratum::hal::bcm::Main(argc, argv).error_code();
}
