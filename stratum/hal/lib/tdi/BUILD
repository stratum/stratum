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

config_setting(
    name = "dpdk_target",
    define_values = {
        "target": "dpdk",
    },
)

config_setting(
    name = "tofino_target",
    define_values = {
        "target": "tofino",
    },
)

target_defines = select({
    ":dpdk_target": ["DPDK_TARGET"],
    ":tofino_target": ["TOFINO_TARGET"],
})

target_sdk = select({
    ":dpdk_target": ["@local_dpdk_bin//:dpdk_sde"],
    ":tofino_target": ["@local_tofino_bin//:tofino_sde"],
})

stratum_cc_library(
    name = "tdi_sde_interface",
    hdrs = ["tdi_sde_interface.h"],
    deps = [
        ":tdi_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "tdi_sde_mock",
    testonly = 1,
    hdrs = ["tdi_sde_mock.h"],
    deps = [
        ":tdi_sde_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "tdi_sde_wrapper",
    srcs = [
        "tdi_sde_action_profile.cc",
        "tdi_sde_clone_session.cc",
        "tdi_sde_counter.cc",
        "tdi_sde_helpers.cc",
        "tdi_sde_meter.cc",
        "tdi_sde_multicast.cc",
        "tdi_sde_register.cc",
        "tdi_sde_table_data.cc",
        "tdi_sde_table_entry.cc",
        "tdi_sde_table_key.cc",
        "tdi_sde_wrapper.cc",
    ],
    defines = target_defines,
    deps = [
        ":tdi_sde_interface",
        ":tdi_sde_headers",
        ":tdi_constants",
        ":tdi_id_mapper",
        ":macros",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "@com_google_absl//absl/cleanup",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
    ] + target_sdk,
)

stratum_cc_library(
    name = "tdi_sde_headers",
    hdrs = [
        "tdi_id_mapper.h",
        "tdi_sde_common.h",
        "tdi_sde_helpers.h",
        "tdi_sde_wrapper.h",
    ],
    deps = [
        ":macros",
        ":tdi_sde_interface",
    ],
)

stratum_cc_library(
    name = "tdi_pipeline_utils",
    srcs = ["tdi_pipeline_utils.cc"],
    hdrs = ["tdi_pipeline_utils.h"],
    deps = [
        ":tdi_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_nlohmann_json//:json",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "tdi_pipeline_utils_test",
    srcs = ["tdi_pipeline_utils_test.cc"],
    deps = [
        ":tdi_pipeline_utils",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/strings",
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

proto_library(
    name = "tdi_proto",
    srcs = ["tdi.proto"],
    deps = [
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)

cc_proto_library(
    name = "tdi_cc_proto",
    deps = [":tdi_proto"],
)

stratum_cc_library(
    name = "tdi_node",
    srcs = ["tdi_node.cc"],
    hdrs = ["tdi_node.h"],
    deps = [
        ":tdi_action_profile_manager",
        ":tdi_cc_proto",
        ":tdi_counter_manager",
        ":tdi_packetio_manager",
        ":tdi_pipeline_utils",
        ":tdi_pre_manager",
        ":tdi_table_manager",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:proto_oneof_writer_wrapper",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_library(
    name = "tdi_node_mock",
    testonly = 1,
    hdrs = ["tdi_node_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":tdi_node",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "tdi_table_manager",
    srcs = ["tdi_table_manager.cc"],
    hdrs = ["tdi_table_manager.h"],
    deps = [
        ":tdi_cc_proto",
        ":tdi_sde_interface",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "tdi_table_manager_test",
    srcs = ["tdi_table_manager_test.cc"],
    deps = [
        ":tdi_sde_mock",
        ":tdi_table_manager",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "tdi_id_mapper",
    srcs = ["tdi_id_mapper.cc"],
    hdrs = [
        "tdi_id_mapper.h",
        "tdi_sde_utils.h",
    ],
    defines = target_defines,
    deps = [
        ":macros",
        ":tdi_cc_proto",
        ":tdi_constants",
        ":tdi_sde_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_nlohmann_json//:json",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    # macros.h depends on bf_types
    defines = target_defines,
    deps = [
        "//stratum/lib:macros",
    ] + target_sdk,
)

stratum_cc_library(
    name = "tdi_constants",
    hdrs = ["tdi_constants.h"],
    deps = [
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "tdi_action_profile_manager",
    srcs = ["tdi_action_profile_manager.cc"],
    hdrs = ["tdi_action_profile_manager.h"],
    deps = [
        ":tdi_cc_proto",
        ":tdi_constants",
        ":tdi_sde_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_library(
    name = "tdi_packetio_manager",
    srcs = ["tdi_packetio_manager.cc"],
    hdrs = ["tdi_packetio_manager.h"],
    deps = [
        ":tdi_cc_proto",
        ":tdi_sde_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:utils",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

stratum_cc_test(
    name = "tdi_packetio_manager_test",
    srcs = ["tdi_packetio_manager_test.cc"],
    deps = [
        ":tdi_packetio_manager",
        ":tdi_sde_mock",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "tdi_pre_manager",
    srcs = ["tdi_pre_manager.cc"],
    hdrs = ["tdi_pre_manager.h"],
    deps = [
        ":tdi_cc_proto",
        ":tdi_constants",
        ":tdi_sde_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "tdi_pre_manager_test",
    srcs = ["tdi_pre_manager_test.cc"],
    deps = [
        ":tdi_pre_manager",
        ":tdi_sde_mock",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "tdi_counter_manager",
    srcs = ["tdi_counter_manager.cc"],
    hdrs = ["tdi_counter_manager.h"],
    deps = [
        ":tdi_constants",
        ":tdi_sde_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "tdi_counter_manager_test",
    srcs = ["tdi_counter_manager_test.cc"],
    deps = [
        ":tdi_counter_manager",
        ":tdi_sde_mock",
        ":tdi_table_manager",  # FIXME: for bfrt_table_sync_timeout_ms storage
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":tdi_cc_proto",
        ":tdi_constants",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = [
        "utils_test.cc",
    ],
    deps = [
        ":utils",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
