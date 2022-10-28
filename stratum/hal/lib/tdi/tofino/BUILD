# stratum/hal/lib/tdi/tofino/BUILD

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

stratum_cc_library(
    name = "tofino_hal",
    srcs = ["tofino_hal.cc"],
    hdrs = ["tofino_hal.h"],
    deps = [
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:config_monitoring_service",
        "//stratum/hal/lib/common:error_buffer",
        "//stratum/hal/lib/common:p4_service",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "tofino_sde_utils",
    srcs = ["tofino_sde_utils.cc"],
    hdrs = [
        "//stratum/hal/lib/tdi:tdi_sde_utils.h",
    ],
    deps = [
        "@local_tofino_bin//:tofino_sde",
    ],
)

stratum_cc_library(
    name = "tofino_sde_target",
    srcs = [
        "tofino_sde_target.cc",
    ],
    defines = ["TOFINO_TARGET"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/tdi:tdi_sde_headers",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@local_tofino_bin//:tofino_sde",
    ],
)

stratum_cc_library(
    name = "tofino_switch",
    srcs = ["tofino_switch.cc"],
    hdrs = ["tofino_switch.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/tdi:tdi_node",
        "//stratum/hal/lib/tdi/tofino:tofino_chassis_manager",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_library(
    name = "tofino_chassis_manager",
    srcs = ["tofino_chassis_manager.cc"],
    hdrs = ["tofino_chassis_manager.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
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

stratum_cc_test(
    name = "tofino_chassis_manager_test",
    srcs = ["tofino_chassis_manager_test.cc"],
    deps = [
        ":test_main",
        ":tofino_chassis_manager",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_mock",
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
    name = "tofino_chassis_manager_mock",
    testonly = 1,
    hdrs = ["tofino_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":tofino_chassis_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "tofino_hal_test",
    srcs = [
        "tofino_hal_test.cc",
    ],
    deps = [
        ":test_main",
        ":tofino_hal",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:switch_mock",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "tofino_switch_test",
    srcs = [
        "tofino_switch_test.cc",
    ],
    deps = [
        ":test_main",
        ":tofino_switch",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/hal/lib/tdi:tdi_node_mock",
        "//stratum/hal/lib/tdi:tdi_sde_mock",
        "//stratum/hal/lib/tdi/tofino:tofino_chassis_manager_mock",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)
