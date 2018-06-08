#
# Copyright 2018 Google LLC
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

exports_files(["LICENSE"])

load(
    "//build_util:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_proto_library(
    name = "bcm_proto",
    hdrs = ["bcm.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_library(
    name = "bcm_acl_manager",
    srcs = ["bcm_acl_manager.cc"],
    hdrs = ["bcm_acl_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_acl_pipeline",
        ":bcm_chassis_manager",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_control_proto",
        "//stratum/hal/lib/p4:p4_pipeline_config_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:utils",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "bcm_acl_manager_test",
    srcs = ["bcm_acl_manager_test.cc"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_chassis_manager_mock",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/strings",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:flat_map",
        "//util/gtl:flat_set",
        "//util/gtl:map_util",
    ],
)

cc_library(
    name = "bcm_acl_manager_mock",
    testonly = 1,
    hdrs = ["bcm_acl_manager_mock.h"],
    deps = [
        ":bcm_acl_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_library(
    name = "bcm_acl_pipeline",
    srcs = ["bcm_acl_pipeline.cc"],
    hdrs = ["bcm_acl_pipeline.h"],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//sandblaze/p4lang:p4info_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/hal/lib/p4:p4_control_proto",
        "//stratum/hal/lib/p4:p4_table_map_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "bcm_acl_pipeline_test",
    srcs = ["bcm_acl_pipeline_test.cc"],
    deps = [
        ":bcm_acl_pipeline",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:p4_control_proto",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "bcm_chassis_manager",
    srcs = ["bcm_chassis_manager.cc"],
    hdrs = ["bcm_chassis_manager.h"],
    deps = [
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":bcm_serdes_db_manager",
        ":utils",
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//protobuf",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
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
        "//stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_chassis_manager_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_chassis_manager_test",
    srcs = ["bcm_chassis_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_sdk_mock",
        ":bcm_serdes_db_manager_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
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
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
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
#        "//sandblaze/p4lang:p4runtime_grpc_proto",
        "@com_github_p4lang_PI//p4lang:p4runtime_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
#        "//util/gtl:node_hash_set",
    ],
)

stratum_cc_test(
    name = "bcm_flow_table_test",
    srcs = ["bcm_flow_table_test.cc"],
    deps = [
        ":bcm_flow_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
    ],
)

stratum_cc_library(
    name = "bcm_l2_manager",
    srcs = ["bcm_l2_manager.cc"],
    hdrs = ["bcm_l2_manager.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_proto",
        ":bcm_sdk_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/public/proto:hal_grpc_proto",
        "//stratum/public/proto:p4_table_defs_proto",
    ],
)

stratum_cc_library(
    name = "bcm_l2_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l2_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l2_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_l2_manager_test",
    srcs = ["bcm_l2_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_l2_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "bcm_l3_manager",
    srcs = ["bcm_l3_manager.cc"],
    hdrs = ["bcm_l3_manager.h"],
    deps = [
        ":bcm_proto",
        ":bcm_sdk_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:hal_grpc_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "bcm_l3_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l3_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l3_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_l3_manager_test",
    srcs = ["bcm_l3_manager_test.cc"],
    deps = [
        ":bcm_l3_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "bcm_packetio_manager",
    srcs = ["bcm_packetio_manager.cc"],
    hdrs = ["bcm_packetio_manager.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":constants",
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/p4lang:p4runtime_grpc_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_packetio_manager_mock",
    testonly = 1,
    hdrs = ["bcm_packetio_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_packetio_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_packetio_manager_test",
    srcs = ["bcm_packetio_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_packetio_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "//util/libcproxy:libcwrapper",
        "//util/libcproxy:passthrough_proxy",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_interface",
    hdrs = ["bcm_sdk_interface.h"],
    deps = [
        ":bcm_proto",
        "@com_google_absl//absl/base:core_headers",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib/channel",
        "//stratum/public/proto:hal_grpc_proto",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_mock",
    testonly = 1,
    hdrs = ["bcm_sdk_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_sdk_interface",
        "//testing/base/public:gunit_no_google3",
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
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
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
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//protobuf",
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_serdes_db_manager",
    srcs = ["bcm_serdes_db_manager.cc"],
    hdrs = ["bcm_serdes_db_manager.h"],
    deps = [
        ":bcm_proto",
        ":utils",
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:hal_grpc_proto",
    ],
)

stratum_cc_library(
    name = "bcm_serdes_db_manager_mock",
    testonly = 1,
    hdrs = ["bcm_serdes_db_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_serdes_db_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_serdes_db_manager_test",
    srcs = ["bcm_serdes_db_manager_test.cc"],
    deps = [
        ":bcm_serdes_db_manager",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "bcm_node",
    srcs = ["bcm_node.cc"],
    hdrs = ["bcm_node.h"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_l2_manager",
        ":bcm_l3_manager",
        ":bcm_packetio_manager",
        ":bcm_table_manager",
        ":constants",
        "//base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//protobuf",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
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
        "//testing/base/public:gunit_no_google3",
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
        ":test_main",
        "//testing/base/public:gunit_no_google3",
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
        ":bcm_node",
        ":constants",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//protobuf",
        "//stratum:status_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
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
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/channel:channel_mock",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager",
    srcs = ["bcm_table_manager.cc"],
    hdrs = ["bcm_table_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_acl_pipeline",
        ":bcm_chassis_manager",
        ":bcm_flow_table",
        ":bcm_proto",
        ":constants",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//protobuf",
        "//sandblaze/p4lang:p4info_proto",
        "//sandblaze/p4lang:p4runtime_grpc_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:hal_grpc_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

stratum_cc_library(
    name = "bcm_table_manager_mock",
    testonly = 1,
    hdrs = ["bcm_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_table_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

stratum_cc_test(
    name = "bcm_table_manager_test",
    srcs = ["bcm_table_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_table_manager",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "//protobuf",
        "//sandblaze/p4lang:p4info_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/hal/lib/p4:p4_info_manager_mock",
        "//stratum/hal/lib/p4:p4_table_mapper_mock",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
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
        "//sandblaze/prebuilt/broadcom:bcm-sdk",
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
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
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
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

stratum_cc_library(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bcm_proto",
        "@com_google_absl//absl/base:core_headers",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:constants",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//stratum/lib:constants",
    ],
)

cc_library(
    name = "bcm_sim_test_fixture",
    testonly = 1,
    srcs = ["bcm_sim_test_fixture.cc"],
    hdrs = [
        "bcm_sim_test_fixture.h",
    ],
    data = [
        "//stratum/hal/bin/bcm/sim:bcm_pcid_sim",
    ],
    deps = [
        ":bcm_acl_manager",
        ":bcm_chassis_manager",
        ":bcm_sdk_sim",
        ":bcm_serdes_db_manager",
        ":bcm_table_manager",
        "//base",
        "//devtools/build/runtime:get_runfiles_dir",
        "//testing/base/public:gunit_no_google3",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/p4:p4_runtime_real",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/lib:utils",
    ],
)

cc_test(
    name = "bcm_acl_profile_test",
    timeout = "long",
    srcs = ["bcm_acl_profile_test.cc"],
    local = 1,
    deps = [
        ":bcm_sim_test_fixture",
        ":test_main",
        "//sandblaze/p4lang:p4runtime_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_test_util",
    ],
)

stratum_cc_library(
    name = "acl_table",
    srcs = ["acl_table.cc"],
    hdrs = ["acl_table.h"],
    deps = [
        ":bcm_flow_table",
        ":bcm_proto",
        "//stratum/hal/lib/p4:common_flow_entry_proto",
        "//stratum/public/proto:p4_annotation_proto",
        "//stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "acl_table_test",
    srcs = ["acl_table_test.cc"],
    deps = [
        ":acl_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//sandblaze/p4lang:p4runtime_grpc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)
