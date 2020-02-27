#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "HOST_ARCHES",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# Settings to choose which Broadcom SDK to use.
config_setting(
    name = "bcm_sdk6",
    define_values = {
        "bcm_sdk": "6",
    },
)

config_setting(
    name = "bcm_sdklt",
    define_values = {
        "bcm_sdk": "lt",
    },
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
    deps = [":bcm_proto"]
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
        ":bcm_chassis_ro_interface",
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        ":pipeline_processor",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/glue/gtl:map_util",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib/test_utils:p4_proto_builders",
        "//stratum/public/proto:p4_annotation_cc_proto",
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

stratum_cc_library(
    name = "bcm_chassis_ro_interface",
    hdrs = ["bcm_chassis_ro_interface.h"],
    deps = [
        ":bcm_cc_proto",
        ":utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:utils",
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
    srcs = [
        "bcm_chassis_manager.cc",
    ] + select({
        ":bcm_sdk6": ["bcm_chassis_manager_config_generator_sdk6.cc"],
        ":bcm_sdklt": ["bcm_chassis_manager_config_generator_sdklt.cc"],
        "//conditions:default": ["bcm_chassis_manager_config_generator_sdklt.cc"],
    }),
    hdrs = ["bcm_chassis_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_node",
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        ":bcm_serdes_db_manager",
        ":utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
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
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ] + select({
        ":bcm_sdk6": [],
        ":bcm_sdklt": ["@com_github_jbeder_yaml_cpp//:yaml_cpp"],
        "//conditions:default": ["@com_github_jbeder_yaml_cpp//:yaml_cpp"],
    }),
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
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
    ],
)

stratum_cc_library(
    name = "bcm_diag_shell",
    srcs = ["bcm_diag_shell.cc"],
    hdrs = ["bcm_diag_shell.h"],
    defines = ["NDEBUG"],
    deps = [
        ":sdk_build_undef",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        #"//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "bcm_flow_table",
    hdrs = ["bcm_flow_table.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "bcm_flow_table_test",
    srcs = ["bcm_flow_table_test.cc"],
    deps = [
        ":bcm_flow_table",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "bcm_l2_manager",
    srcs = ["bcm_l2_manager.cc"],
    hdrs = ["bcm_l2_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_cc_proto",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
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
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:source_location",
        "//stratum/glue/status:status_test_util",
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
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc", #FIXME actually p4runtime_cc_proto
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
        ":bcm_tunnel_manager",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "//stratum/lib/test_utils:matchers",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "bcm_packetio_manager",
    srcs = ["bcm_packetio_manager.cc"],
    hdrs = ["bcm_packetio_manager.h"],
    deps = [
        ":bcm_chassis_ro_interface",
        ":bcm_global_vars",
        ":bcm_cc_proto",
        ":bcm_sdk_interface",
        ":constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/libcproxy:passthrough_proxy",
        "//stratum/lib/libcproxy:libcwrapper",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
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
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib/channel",
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
        ":bcm_sdk_wrapper",
        ":macros",
        ":sdk_build_undef",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        #"//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_wrapper",
    srcs = ["bcm_sdk_wrapper.cc"],
    hdrs = ["bcm_sdk_wrapper.h"],
    defines = ["NDEBUG"],
    deps = [
        ":bcm_diag_shell",
        ":bcm_sdk_interface",
        ":constants",
        ":macros",
        ":sdk_build_undef",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_opennetworkinglab_sdklt//:bcm_headers",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/net_util:ipaddress",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        # FIXME(boc)
        # "//util/endian",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/gtl:cleanup",
    ],
)

stratum_cc_library(
    name = "bcm_serdes_db_manager",
    srcs = ["bcm_serdes_db_manager.cc"],
    hdrs = ["bcm_serdes_db_manager.h"],
    deps = [
        ":bcm_cc_proto",
        ":utils",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
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
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager",
    srcs = ["bcm_table_manager.cc"],
    hdrs = ["bcm_table_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_chassis_ro_interface",
        ":bcm_flow_table",
        ":bcm_cc_proto",
        ":constants",
        ":utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/glue:logging",
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
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager_mock",
    testonly = 1,
    hdrs = ["bcm_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_table_manager",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_map",
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
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:source_location",
    ],
)

stratum_cc_library(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    defines = ["NDEBUG"],
    deps = [
        #"//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_github_opennetworkinglab_sdklt//:bcm_headers",
        "@com_github_opennetworkinglab_sdklt//:bcm_sdklt",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bcm_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "//stratum/hal/lib/common:utils",
        "//stratum/glue:integral_types",
        "//stratum/lib:constants",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "@com_google_googletest//:gtest",
        "//stratum/lib:constants",
    ],
)

stratum_cc_library(
    name = "acl_table",
    srcs = ["acl_table.cc"],
    hdrs = ["acl_table.h"],
    deps = [
        ":bcm_flow_table",
        ":bcm_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "acl_table_test",
    srcs = ["acl_table_test.cc"],
    deps = [
        ":acl_table",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "pipeline_processor",
    srcs = ["pipeline_processor.cc"],
    hdrs = ["pipeline_processor.h"],
    deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "//stratum/hal/lib/p4:common_flow_entry_cc_proto",
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/hal/lib/p4:p4_table_map_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_test(
    name = "pipeline_processor_test",
    srcs = ["pipeline_processor_test.cc"],
    deps = [
        ":pipeline_processor",
        ":test_main",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/lib/test_utils:p4_proto_builders",
        "//stratum/public/lib:error",
        "//stratum/public/proto:p4_annotation_cc_proto",
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
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "//stratum/glue/gtl:map_util",
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
