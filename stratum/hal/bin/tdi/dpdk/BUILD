# stratum/hal/bin/tdi/dpdk/BUILD

# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_dpdk_common_deps = [
    "//stratum/glue:init_google",
    "//stratum/glue:logging",
    "//stratum/hal/lib/phal:phal_sim",
    "//stratum/hal/lib/phal",
    "//stratum/lib/security:auth_policy_checker",
    "//stratum/lib/security:credentials_manager",
    "@com_github_google_glog//:glog",
]

stratum_cc_binary(
    name = "stratum_dpdk",
    srcs = [
        "dpdk_main.cc",
        "dpdk_main.h",
        "main.cc",
    ],
    arches = HOST_ARCHES,
    data = [
        # NOTE: Replace with dpdk conf files
        "//stratum/hal/bin/barefoot:tofino_skip_p4.conf",
        "//stratum/hal/bin/barefoot:tofino_skip_p4_no_bsp.conf",
    ],
    defines = ["DPDK_TARGET"],
    deps = [
        "//stratum/hal/lib/tdi/dpdk:dpdk_chassis_manager",
        "//stratum/hal/lib/tdi/dpdk:dpdk_hal",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_target",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_utils",
        "//stratum/hal/lib/tdi/dpdk:dpdk_switch",
        "//stratum/hal/lib/tdi:tdi_action_profile_manager",
        "//stratum/hal/lib/tdi:tdi_counter_manager",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi:tdi_pre_manager",
        "//stratum/hal/lib/tdi:tdi_sde_wrapper",
        "//stratum/hal/lib/tdi:tdi_table_manager",
    ] + stratum_dpdk_common_deps,
)
