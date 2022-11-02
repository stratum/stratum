# stratum/hal/bin/tdi/dpdk/BUILD

# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

stratum_cc_binary(
    name = "stratum_dpdk",
    srcs = ["main.cc"],
    arches = HOST_ARCHES,
    data = [
        "dpdk_skip_p4.conf",
    ],
    defines = ["DPDK_TARGET"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/phal",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/hal/lib/tdi:tdi_action_profile_manager",
        "//stratum/hal/lib/tdi:tdi_counter_manager",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi:tdi_pre_manager",
        "//stratum/hal/lib/tdi:tdi_sde_wrapper",
        "//stratum/hal/lib/tdi:tdi_table_manager",
        "//stratum/hal/lib/tdi/dpdk:dpdk_chassis_manager",
        "//stratum/hal/lib/tdi/dpdk:dpdk_hal",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_target",
        "//stratum/hal/lib/tdi/dpdk:dpdk_sde_utils",
        "//stratum/hal/lib/tdi/dpdk:dpdk_switch",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)
