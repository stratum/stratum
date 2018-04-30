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
    "//third_party/stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
    "sc_proto_lib",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_proto_lib(
    name = "bcm_proto",
    hdrs = ["bcm.proto"],
    deps = [
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/public/proto:hal_proto",
    ],
)

sc_cc_lib(
    name = "bcm_acl_manager",
    srcs = ["bcm_acl_manager.cc"],
    hdrs = ["bcm_acl_manager.h"],
    deps = [
        ":acl_table",
        ":bcm_acl_pipeline",
        ":bcm_chassis_manager",
        ":bcm_sdk_interface",
        ":bcm_table_manager",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/p4:p4_control_proto",
        "//third_party/stratum/hal/lib/p4:p4_pipeline_config_proto",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_acl_manager_test",
    srcs = ["bcm_acl_manager_test.cc"],
    deps = [
        ":bcm_acl_manager",
        ":bcm_chassis_manager_mock",
        ":bcm_sdk_mock",
        ":bcm_table_manager_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/strings",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper_mock",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/test_utils:matchers",
        "//third_party/stratum/public/proto:p4_annotation_proto",
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

sc_cc_lib(
    name = "bcm_acl_pipeline",
    srcs = ["bcm_acl_pipeline.cc"],
    hdrs = ["bcm_acl_pipeline.h"],
    deps = [
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/p4:common_flow_entry_proto",
        "//third_party/stratum/hal/lib/p4:p4_control_proto",
        "//third_party/stratum/hal/lib/p4:p4_table_map_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_acl_pipeline_test",
    srcs = ["bcm_acl_pipeline_test.cc"],
    deps = [
        ":bcm_acl_pipeline",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/p4:p4_control_proto",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_manager",
    srcs = ["bcm_chassis_manager.cc"],
    hdrs = ["bcm_chassis_manager.h"],
    deps = [
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":bcm_serdes_db_manager",
        ":utils",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/hal/lib/common:switch_interface",
        "//third_party/stratum/hal/lib/common:utils",
        "//third_party/stratum/hal/lib/common:writer_interface",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/channel",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_chassis_manager_mock",
    testonly = 1,
    hdrs = ["bcm_chassis_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_chassis_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_chassis_manager_test",
    srcs = ["bcm_chassis_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_sdk_mock",
        ":bcm_serdes_db_manager_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:phal_mock",
        "//third_party/stratum/hal/lib/common:writer_mock",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/channel:channel_mock",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "bcm_diag_shell",
    srcs = ["bcm_diag_shell.cc"],
    hdrs = ["bcm_diag_shell.h"],
    defines = ["NDEBUG"],
    deps = [
        ":sdk_build_undef",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/broadcom:bcm-sdk",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "bcm_flow_table",
    hdrs = ["bcm_flow_table.h"],
    deps = [
        "//third_party/absl/base:core_headers",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
        "//util/gtl:node_hash_set",
    ],
)

sc_cc_test(
    name = "bcm_flow_table_test",
    srcs = ["bcm_flow_table_test.cc"],
    deps = [
        ":bcm_flow_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib/test_utils:matchers",
    ],
)

sc_cc_lib(
    name = "bcm_l2_manager",
    srcs = ["bcm_l2_manager.cc"],
    hdrs = ["bcm_l2_manager.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_proto",
        ":bcm_sdk_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:utils",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
    ],
)

sc_cc_lib(
    name = "bcm_l2_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l2_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l2_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_l2_manager_test",
    srcs = ["bcm_l2_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_l2_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "bcm_l3_manager",
    srcs = ["bcm_l3_manager.cc"],
    hdrs = ["bcm_l3_manager.h"],
    deps = [
        ":bcm_proto",
        ":bcm_sdk_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "bcm_l3_manager_mock",
    testonly = 1,
    hdrs = ["bcm_l3_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_l3_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_l3_manager_test",
    srcs = ["bcm_l3_manager_test.cc"],
    deps = [
        ":bcm_l3_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "bcm_packetio_manager",
    srcs = ["bcm_packetio_manager.cc"],
    hdrs = ["bcm_packetio_manager.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_proto",
        ":bcm_sdk_interface",
        ":constants",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_packetio_manager_mock",
    testonly = 1,
    hdrs = ["bcm_packetio_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_packetio_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_packetio_manager_test",
    srcs = ["bcm_packetio_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_packetio_manager",
        ":bcm_sdk_mock",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:writer_mock",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper_mock",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/lib:error",
        "//util/libcproxy:libcwrapper",
        "//util/libcproxy:passthrough_proxy",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_interface",
    hdrs = ["bcm_sdk_interface.h"],
    deps = [
        ":bcm_proto",
        "//third_party/absl/base:core_headers",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib/channel",
        "//third_party/stratum/public/proto:hal_grpc_proto",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_mock",
    testonly = 1,
    hdrs = ["bcm_sdk_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_sdk_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "bcm_sdk_sim",
    srcs = ["bcm_sdk_sim.cc"],
    hdrs = ["bcm_sdk_sim.h"],
    deps = [
        ":bcm_sdk_wrapper",
        ":macros",
        ":sdk_build_undef",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/broadcom:bcm-sdk",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/prebuilt/broadcom:bcm-sdk",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_serdes_db_manager",
    srcs = ["bcm_serdes_db_manager.cc"],
    hdrs = ["bcm_serdes_db_manager.h"],
    deps = [
        ":bcm_proto",
        ":utils",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_grpc_proto",
    ],
)

sc_cc_lib(
    name = "bcm_serdes_db_manager_mock",
    testonly = 1,
    hdrs = ["bcm_serdes_db_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_serdes_db_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_serdes_db_manager_test",
    srcs = ["bcm_serdes_db_manager_test.cc"],
    deps = [
        ":bcm_serdes_db_manager",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "bcm_node_mock",
    testonly = 1,
    hdrs = ["bcm_node_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_node",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
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
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:writer_mock",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper_mock",
        "//third_party/stratum/lib:utils",
    ],
)

sc_cc_lib(
    name = "bcm_switch",
    srcs = ["bcm_switch.cc"],
    hdrs = ["bcm_switch.h"],
    deps = [
        ":bcm_chassis_manager",
        ":bcm_node",
        ":constants",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/protobuf",
        "//third_party/stratum:status_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/hal/lib/common:phal_interface",
        "//third_party/stratum/hal/lib/common:switch_interface",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "bcm_switch_test",
    srcs = ["bcm_switch_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_node_mock",
        ":bcm_packetio_manager_mock",
        ":bcm_switch",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:phal_mock",
        "//third_party/stratum/hal/lib/common:switch_interface",
        "//third_party/stratum/hal/lib/common:writer_mock",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper_mock",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/channel:channel_mock",
    ],
)

sc_cc_lib(
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:writer_interface",
        "//third_party/stratum/hal/lib/p4:common_flow_entry_proto",
        "//third_party/stratum/hal/lib/p4:p4_info_manager",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
        "//util/gtl:stl_util",
    ],
)

sc_cc_lib(
    name = "bcm_table_manager_mock",
    testonly = 1,
    hdrs = ["bcm_table_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":bcm_table_manager",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_test(
    name = "bcm_table_manager_test",
    srcs = ["bcm_table_manager_test.cc"],
    deps = [
        ":bcm_chassis_manager_mock",
        ":bcm_table_manager",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/common:constants",
        "//third_party/stratum/hal/lib/common:writer_mock",
        "//third_party/stratum/hal/lib/p4:p4_info_manager_mock",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper_mock",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/test_utils:matchers",
        "//third_party/stratum/public/lib:error",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "//third_party/absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "macros",
    hdrs = ["macros.h"],
    defines = ["NDEBUG"],
    deps = [
        "//third_party/sandblaze/prebuilt/broadcom:bcm-sdk",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "macros_test",
    srcs = ["macros_test.cc"],
    deps = [
        ":macros",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:macros",
    ],
)

sc_cc_lib(
    name = "sdk_build_undef",
    hdrs = ["sdk_build_undef.h"],
)

sc_cc_lib(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":bcm_proto",
        "//third_party/absl/base:core_headers",
        "//third_party/stratum/hal/lib/common:utils",
        "//third_party/stratum/lib:constants",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/lib:constants",
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
        "//third_party/stratum/hal/bin/bcm/sim:bcm_pcid_sim",
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
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/hal/lib/p4:p4_runtime_real",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/hal/lib/phal:phal_sim",
        "//third_party/stratum/lib:utils",
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
        "//third_party/sandblaze/p4lang:p4runtime_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status:status_test_util",
    ],
)

sc_cc_lib(
    name = "acl_table",
    srcs = ["acl_table.cc"],
    hdrs = ["acl_table.h"],
    deps = [
        ":bcm_flow_table",
        ":bcm_proto",
        "//third_party/stratum/hal/lib/p4:common_flow_entry_proto",
        "//third_party/stratum/public/proto:p4_annotation_proto",
        "//third_party/stratum/public/proto:p4_table_defs_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:flat_hash_set",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "acl_table_test",
    srcs = ["acl_table_test.cc"],
    deps = [
        ":acl_table",
        ":test_main",
        "//testing/base/public:gunit_no_google3",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/test_utils:matchers",
    ],
)
