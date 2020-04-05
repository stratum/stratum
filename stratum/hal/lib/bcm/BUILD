# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "bcm_proto",
    srcs = ["bcm.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

cc_proto_library(
    name = "bcm_cc_proto",
    deps = [":bcm_proto"],
)

stratum_cc_library(
    name = "bcm_global_vars",
    srcs = ["bcm_global_vars.cc"],
    hdrs = ["bcm_global_vars.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_acl_manager",
    srcs = ["bcm_acl_manager.cc"],
    hdrs = ["bcm_acl_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_cc_proto",
        ":bcm_chassis_ro_interface",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        ":pipeline_processor",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "bcm_acl_manager_test",
    srcs = ["bcm_acl_manager_test.cc"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_chassis_manager_mock",
        ":bcm_chassis_ro_mock",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib/test_utils:p4_proto_builders",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "bcm_acl_manager_mock",
    testonly = 1,
    hdrs = ["bcm_acl_manager_mock.h"],
    deps = [
        ":bcm_acl_manager",
        "@com_google_googletest//:gtest",
    ],
)

# TODO restore this when the file is building
# stratum_cc_library(
#     name = "bcm_acl_pipeline",
#     srcs = ["bcm_acl_pipeline.cc"],
#     hdrs = ["bcm_acl_pipeline.h"],
#     deps = [
#         "@com_github_p4lang_p4runtime//:p4info_cc_proto",
#         "@com_google_absl//absl/container:flat_hash_map",
#         "@com_google_absl//absl/memory",
#         "@com_google_absl//absl/strings",
#         "//stratum/glue:integral_types",
#         "//stratum/glue/gtl:map_util",
#         "//stratum/glue/status:status_macros",
#         "//stratum/glue/status:statusor",
#         "//stratum/glue/status",
#         "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
#         "//stratum/hal/lib/p4:p4_control_cc_proto",
#         "//stratum/hal/lib/p4:p4_table_map_cc_proto",
#         "//stratum/public/proto:p4_table_defs_cc_proto",
#     ],
# )

stratum_cc_library(
    name = "bcm_chassis_ro_interface",
    hdrs = ["bcm_chassis_ro_interface.h"],
    deps = [
        ":bcm_cc_proto",
        ":utils",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_library(
    name = "bcm_chassis_ro_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_ro_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_ro_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_chassis_manager",
    srcs = ["bcm_chassis_manager.cc"],
    hdrs = ["bcm_chassis_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_node",
        ":bcm_sdk_interface",
        ":bcm_serdes_db_manager",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
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
        "//stratum/lib/channel",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_chassis_manager_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_chassis_manager_test",
    srcs = ["bcm_chassis_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_node_mock",
        ":bcm_sdk_mock",
        ":bcm_serdes_db_manager_mock",
        ":test_main",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "bcm_diag_shell_interface",
    srcs = ["bcm_diag_shell.h"],
    defines = ["NDEBUG"],
    deps = [
        ":sdk_build_undef",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_flow_table",
    hdrs = ["bcm_flow_table.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:node_hash_set",
    ],
)

stratum_cc_test(
    name = "bcm_flow_table_test",
    srcs = ["bcm_flow_table_test.cc"],
    deps = [
        ":bcm_flow_table",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_l2_manager",
    srcs = ["bcm_l2_manager.cc"],
    hdrs = ["bcm_l2_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_chassis_ro_interface",
        ":bcm_sdk_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_l2_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l2_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l2_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_l2_manager_test",
    srcs = ["bcm_l2_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_l2_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_l3_manager",
    srcs = ["bcm_l3_manager.cc"],
    hdrs = ["bcm_l3_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_l3_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l3_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l3_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_l3_manager_test",
    srcs = ["bcm_l3_manager_test.cc"],
    deps = [
        ":bcm_l3_manager",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//stratum/glue/gtl:source_location",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_tunnel_manager",
    srcs = ["bcm_tunnel_manager.cc"],
    hdrs = ["bcm_tunnel_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",  #FIXME actually p4runtime_cc_proto
    ],
)

stratum_cc_library(
    name = "bcm_tunnel_manager_mock",
    testonly = 1,
    hdrs = ["bcm_tunnel_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_tunnel_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_tunnel_manager_test",
    srcs = ["bcm_tunnel_manager_test.cc"],
    deps = [
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":bcm_tunnel_manager",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_packetio_manager",
    srcs = ["bcm_packetio_manager.cc"],
    hdrs = ["bcm_packetio_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_sdk_interface",
        ":constants",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_packetio_manager_mock",
    testonly = 1,
    hdrs = ["bcm_packetio_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_packetio_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_packetio_manager_test",
    srcs = ["bcm_packetio_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_packetio_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/libcproxy:libcwrapper",
        "//stratum/lib/libcproxy:passthrough_proxy",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

''' FIXME google only
stratum_cc_library(
    name = "bcm_sdk_proxy",
    srcs = ["bcm_sdk_proxy.cc"],
    hdrs = ["bcm_sdk_proxy.h"],
    deps = [
        ":bcm_sdk_interface",
        ":constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/sandcastle:bcm_sdk_service_grpc_proto",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ],
)

stratum_cc_test(
    name = "bcm_sdk_proxy_test",
    srcs = ["bcm_sdk_proxy_test.cc"],
    data = ["//tools/test:requires_ipv4_loopback"],
    deps = [
        ":bcm_sdk_proxy",
        ":test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:hardware_status_service_test_helper",
        "//stratum/lib:utils",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/public/proto:error_proto",
    ],
)
'''

stratum_cc_library(
    name = "bcm_sdk_interface",
    hdrs = ["bcm_sdk_interface.h"],
    deps = [
        ":bcm_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_mock",
    testonly = 1,
    hdrs = ["bcm_sdk_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_sdk_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_sim",
    srcs = ["bcm_sdk_sim.cc"],
    hdrs = ["bcm_sdk_sim.h"],
    deps = [
        ":sdk_build_undef",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/status",
        "//stratum/hal/lib/bcm/sdk:bcm_sdk_wrapper",
        "//stratum/hal/lib/bcm/sdk:macros",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_serdes_db_manager",
    srcs = ["bcm_serdes_db_manager.cc"],
    hdrs = ["bcm_serdes_db_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_serdes_db_manager_mock",
    testonly = 1,
    hdrs = ["bcm_serdes_db_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_serdes_db_manager",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_serdes_db_manager_test",
    srcs = ["bcm_serdes_db_manager_test.cc"],
    deps = [
        ":bcm_serdes_db_manager",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_node",
    srcs = ["bcm_node.cc"],
    hdrs = ["bcm_node.h"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_global_vars",
        ":bcm_l2_manager",
        ":bcm_l3_manager",
        ":bcm_packetio_manager",
        ":bcm_table_manager",
        ":bcm_tunnel_manager",
        ":constants",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "bcm_node_mock",
    testonly = 1,
    hdrs = ["bcm_node_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_node",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_node_test",
    srcs = ["bcm_node_test.cc"],
    deps = [
        ":bcm_acl_manager_mock",
        ":bcm_l2_manager_mock",
        ":bcm_l3_manager_mock",
        ":bcm_node",
        ":bcm_packetio_manager_mock",
        ":bcm_table_manager_mock",
        ":bcm_tunnel_manager_mock",
        ":test_main",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_switch",
    srcs = ["bcm_switch.cc"],
    hdrs = ["bcm_switch.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_global_vars",
        ":bcm_node",
        ":constants",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "bcm_switch_test",
    srcs = ["bcm_switch_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_node_mock",
        ":bcm_packetio_manager_mock",
        ":bcm_switch",
        ":test_main",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager",
    srcs = ["bcm_table_manager.cc"],
    hdrs = ["bcm_table_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_cc_proto",
        ":bcm_chassis_ro_interface",
        ":bcm_flow_table",
        ":constants",
        ":utils",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager_mock",
    testonly = 1,
    hdrs = ["bcm_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_table_manager",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "bcm_table_manager_test",
    srcs = ["bcm_table_manager_test.cc"],
    deps = [
        ":bcm_chassis_ro_mock",
        ":bcm_table_manager",
        ":constants",
        ":test_main",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:source_location",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_test(
    name = "macros_sdklt_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/bcm/sdklt:macros",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_test(
    name = "macros_sdk_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/bcm/sdk:macros",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "sdk_build_undef",
    hdrs = ["sdk_build_undef.h"],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bcm_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:constants",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//stratum/lib:constants",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "acl_table",
    srcs = ["acl_table.cc"],
    hdrs = ["acl_table.h"],
    deps = [
        ":bcm_cc_proto",
        ":bcm_flow_table",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

stratum_cc_test(
    name = "acl_table_test",
    srcs = ["acl_table_test.cc"],
    deps = [
        ":acl_table",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "pipeline_processor",
    srcs = ["pipeline_processor.cc"],
    hdrs = ["pipeline_processor.h"],
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "pipeline_processor_test",
    srcs = ["pipeline_processor_test.cc"],
    deps = [
        ":pipeline_processor",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib/test_utils:p4_proto_builders",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bcm_udf_manager",
    srcs = ["bcm_udf_manager.cc"],
    hdrs = ["bcm_udf_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)

stratum_cc_test(
    name = "bcm_udf_manager_test",
    srcs = ["bcm_udf_manager_test.cc"],
    deps = [
        ":bcm_sdk_mock",
        ":bcm_udf_manager",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        #FIXME(boc) "//util/endian",
    ],
)
