# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
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

# These defines depend on the SDE version and can be used to implement
# version-dependent features or fixes. There is no default condition to ensure
# a that version upgrade re-visits these.
SDE_DEFINES = select(
    {
        "@local_barefoot_bin//:sde_version_9.7.0": ["SDE_9_7_0"],
        "@local_barefoot_bin//:sde_version_9.7.1": ["SDE_9_7_1"],
        "@local_barefoot_bin//:sde_version_9.7.2": ["SDE_9_7_2"],
        "@local_barefoot_bin//:sde_version_9.8.0": ["SDE_9_8_0"],
        "@local_barefoot_bin//:sde_version_9.9.0": ["SDE_9_9_0"],
        "@local_barefoot_bin//:sde_version_9.10.0": ["SDE_9_10_0"],
    },
    no_match_error = "Unsupported SDE version",
)

stratum_cc_library(
    name = "bfrt_switch",
    srcs = ["bfrt_switch.cc"],
    hdrs = ["bfrt_switch.h"],
    deps = [
        ":bf_chassis_manager",
        ":bf_global_vars",
        ":bf_sde_interface",
        ":bfrt_node",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

stratum_cc_test(
    name = "bfrt_switch_test",
    srcs = ["bfrt_switch_test.cc"],
    deps = [
        ":bf_chassis_manager_mock",
        ":bf_sde_mock",
        ":bfrt_node_mock",
        ":bfrt_switch",
        ":test_main",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:utils",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_global_vars",
    srcs = ["bf_global_vars.cc"],
    hdrs = ["bf_global_vars.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bf_chassis_manager",
    srcs = ["bf_chassis_manager.cc"],
    hdrs = ["bf_chassis_manager.h"],
    copts = ["-Wno-thread-safety-analysis"],  # TODO(antonin)
    deps = [
        ":bf_global_vars",
        ":bf_sde_interface",
        ":bfrt_constants",
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
    name = "bf_chassis_manager_test",
    srcs = ["bf_chassis_manager_test.cc"],
    copts = ["-Wno-thread-safety-analysis"],  # TODO(antonin)
    deps = [
        ":bf_chassis_manager",
        ":bf_sde_mock",
        ":test_main",
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
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_chassis_manager_mock",
    testonly = 1,
    hdrs = ["bf_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bf_chassis_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_sde_interface",
    hdrs = ["bf_sde_interface.h"],
    deps = [
        ":bf_cc_proto",
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
    name = "bf_sde_mock",
    testonly = 1,
    hdrs = ["bf_sde_mock.h"],
    deps = [
        ":bf_sde_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_sde_wrapper",
    srcs = ["bf_sde_wrapper.cc"],
    hdrs = ["bf_sde_wrapper.h"],
    defines = SDE_DEFINES,
    deps = [
        ":bf_sde_interface",
        ":bfrt_constants",
        ":bfrt_id_mapper",
        ":macros",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
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
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "bf_pipeline_utils",
    srcs = ["bf_pipeline_utils.cc"],
    hdrs = ["bf_pipeline_utils.h"],
    deps = [
        ":bf_cc_proto",
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
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "bf_pipeline_utils_test",
    srcs = ["bf_pipeline_utils_test.cc"],
    deps = [
        ":bf_pipeline_utils",
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
    name = "bf_proto",
    srcs = ["bf.proto"],
    deps = [
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)

cc_proto_library(
    name = "bf_cc_proto",
    deps = [":bf_proto"],
)

stratum_cc_library(
    name = "bfrt_node",
    srcs = ["bfrt_node.cc"],
    hdrs = ["bfrt_node.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_global_vars",
        ":bf_pipeline_utils",
        ":bfrt_counter_manager",
        ":bfrt_p4runtime_translator",
        ":bfrt_packetio_manager",
        ":bfrt_pre_manager",
        ":bfrt_table_manager",
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

stratum_cc_test(
    name = "bfrt_node_test",
    srcs = ["bfrt_node_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_counter_manager_mock",
        ":bfrt_node",
        ":bfrt_p4runtime_translator_mock",
        ":bfrt_packetio_manager_mock",
        ":bfrt_pre_manager_mock",
        ":bfrt_table_manager_mock",
        ":test_main",
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
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_node_mock",
    testonly = 1,
    hdrs = ["bfrt_node_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_node",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_table_manager",
    srcs = ["bfrt_table_manager.cc"],
    hdrs = ["bfrt_table_manager.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_global_vars",
        ":bf_sde_interface",
        ":bfrt_p4runtime_translator",
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
    name = "bfrt_table_manager_test",
    srcs = ["bfrt_table_manager_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_p4runtime_translator_mock",
        ":bfrt_table_manager",
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
    name = "bfrt_table_manager_mock",
    testonly = 1,
    hdrs = ["bfrt_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_table_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_id_mapper",
    srcs = ["bfrt_id_mapper.cc"],
    hdrs = ["bfrt_id_mapper.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_sde_interface",
        ":bfrt_constants",
        ":macros",
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
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    deps = [
        "//stratum/lib:macros",
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "bfrt_constants",
    hdrs = ["bfrt_constants.h"],
    deps = [
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "bfrt_packetio_manager",
    srcs = ["bfrt_packetio_manager.cc"],
    hdrs = ["bfrt_packetio_manager.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_global_vars",
        ":bf_sde_interface",
        ":bfrt_p4runtime_translator",
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
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

stratum_cc_test(
    name = "bfrt_packetio_manager_test",
    srcs = ["bfrt_packetio_manager_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_p4runtime_translator_mock",
        ":bfrt_packetio_manager",
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
    name = "bfrt_packetio_manager_mock",
    testonly = 1,
    hdrs = ["bfrt_packetio_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_packetio_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_pre_manager",
    srcs = ["bfrt_pre_manager.cc"],
    hdrs = ["bfrt_pre_manager.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_sde_interface",
        ":bfrt_constants",
        ":bfrt_p4runtime_translator",
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
    name = "bfrt_pre_manager_test",
    srcs = ["bfrt_pre_manager_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_p4runtime_translator_mock",
        ":bfrt_pre_manager",
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
    name = "bfrt_pre_manager_mock",
    testonly = 1,
    hdrs = ["bfrt_pre_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_pre_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_counter_manager",
    srcs = ["bfrt_counter_manager.cc"],
    hdrs = ["bfrt_counter_manager.h"],
    deps = [
        ":bf_sde_interface",
        ":bfrt_constants",
        ":bfrt_p4runtime_translator",
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
    name = "bfrt_counter_manager_test",
    srcs = ["bfrt_counter_manager_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_counter_manager",
        ":bfrt_p4runtime_translator_mock",
        ":bfrt_table_manager",  # FIXME: for bfrt_table_sync_timeout_ms storage
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
    name = "bfrt_counter_manager_mock",
    testonly = 1,
    hdrs = ["bfrt_counter_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_counter_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bfrt_p4runtime_translator",
    srcs = ["bfrt_p4runtime_translator.cc"],
    hdrs = ["bfrt_p4runtime_translator.h"],
    deps = [
        ":bf_sde_interface",
        ":bfrt_constants",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "bfrt_p4runtime_translator_test",
    srcs = ["bfrt_p4runtime_translator_test.cc"],
    deps = [
        ":bf_sde_mock",
        ":bfrt_constants",
        ":bfrt_p4runtime_translator",
        ":bfrt_p4runtime_translator_mock",
        ":utils",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "bfrt_p4runtime_translator_mock",
    testonly = 1,
    hdrs = ["bfrt_p4runtime_translator_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bfrt_p4runtime_translator",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bf_cc_proto",
        ":bfrt_constants",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:utils",
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
