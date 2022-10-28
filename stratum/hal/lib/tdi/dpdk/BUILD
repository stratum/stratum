# stratum/hal/lib/tdi/dpdk/BUILD

# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

target_defines = [
    "DPDK_TARGET",      # shouldn't be necessary
]

stratum_cc_library(
    name = "dpdk_chassis_manager",
    srcs = ["dpdk_chassis_manager.cc"],
    hdrs = ["dpdk_chassis_manager.h"],
    defines = target_defines,
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/tdi:tdi_sde_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "dpdk_sde_utils",
    srcs = ["dpdk_sde_utils.cc"],
    hdrs = [
        "//stratum/hal/lib/tdi:tdi_sde_utils.h",
    ],
    deps = [
        "@local_dpdk_bin//:dpdk_sde",
    ],
)

stratum_cc_library(
    name = "dpdk_sde_target",
    srcs = ["dpdk_sde_target.cc"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/tdi:tdi_sde_headers",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@local_dpdk_bin//:dpdk_sde",
    ],
)

stratum_cc_library(
    name = "dpdk_switch",
    srcs = ["dpdk_switch.cc"],
    hdrs = ["dpdk_switch.h"],
    defines = target_defines,
    deps = [
        ":dpdk_chassis_manager",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/hal/lib/tdi:tdi_cc_proto",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi:tdi_pipeline_utils",
        "//stratum/hal/lib/tdi:tdi_sde_interface",
        "//stratum/hal/lib/tdi:utils",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "dpdk_chassis_manager_test",
    srcs = ["dpdk_chassis_manager_test.cc"],
    defines = target_defines,
    deps = [
        ":dpdk_chassis_manager",
        ":test_main",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/glue:integral_types",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/tdi:tdi_sde_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "dpdk_chassis_manager_mock",
    testonly = 1,
    hdrs = ["dpdk_chassis_manager_mock.h"],
    defines = target_defines,
    arches = HOST_ARCHES,
    deps = [
        ":dpdk_chassis_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    defines = target_defines,
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

# Not yet integrated or tested.
stratum_cc_library(
    name = "dpdk_hal",
    srcs = ["dpdk_hal.cc"],
    hdrs = ["dpdk_hal.h"],
    defines = target_defines,
    deps = [
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:config_monitoring_service",
        "//stratum/hal/lib/common:error_buffer",
        "//stratum/hal/lib/common:p4_service",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

stratum_cc_test(
    name = "dpdk_hal_test",
    srcs = [
        "dpdk_hal_test.cc",
    ],
    defines = target_defines,
    deps = [
        ":dpdk_hal",
        ":test_main",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:switch_mock",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "dpdk_switch_test",
    srcs = [
        "dpdk_switch_test.cc",
    ],
    defines = target_defines,
    deps = [
        ":dpdk_chassis_manager_mock",
        ":dpdk_switch",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/hal/lib/tdi:tdi_node_mock",
        "//stratum/hal/lib/tdi:tdi_sde_mock",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)
