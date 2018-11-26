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
    "//stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_lib",
    "sc_cc_test",
    "sc_proto_lib",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "admin_service",
    srcs = [
        "admin_service.cc",
    ],
    hdrs = [
        "admin_service.h",
    ],
    deps = [
        ":common_proto",
        ":error_buffer",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
        "//stratum/glue/gnoi:system_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
        "//util/task:status",
    ],
)

sc_cc_test(
    name = "admin_service_test",
    srcs = [
        "admin_service_test.cc",
    ],
    deps = [
        ":admin_service",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "//base",
        "//net/util:ports",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/gnoi:system_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "certificate_management_service",
    srcs = [
        "certificate_management_service.cc",
    ],
    hdrs = [
        "certificate_management_service.h",
    ],
    deps = [
        ":common_proto",
        ":error_buffer",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
        "//stratum/glue/gnoi:cert_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
        "//util/task:status",
    ],
)

sc_cc_test(
    name = "certificate_management_service_test",
    srcs = [
        "certificate_management_service_test.cc",
    ],
    deps = [
        ":certificate_management_service",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "//base",
        "//net/util:ports",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/gnoi:system_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "client_sync_reader_writer",
    hdrs = ["client_sync_reader_writer.h"],
    deps = [
        "//absl/base:core_headers",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
    ],
)

sc_proto_lib(
    name = "common_proto",
    hdrs = ["common.proto"],
    python_support = True,
)

sc_cc_lib(
    name = "config_monitoring_service",
    srcs = [
        "config_monitoring_service.cc",
        "gnmi_publisher.cc",
        "yang_parse_tree.cc",
        "yang_parse_tree_paths.cc",
    ],
    hdrs = [
        "config_monitoring_service.h",
        "gnmi_publisher.h",
        "yang_parse_tree.h",
        "yang_parse_tree_paths.h",
    ],
    deps = [
        ":channel_writer_wrapper",
        ":common_proto",
        ":error_buffer",
        ":openconfig_converter",
        ":switch_interface",
        ":writer_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/strings",
        "//absl/strings:str_format",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//sandblaze/prebuilt/grpc",
        "//stratum:gnmi_grpc_proto",
        "//stratum/glue:logging",
        "//stratum/glue/openconfig/proto:openconfig_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/time:clock",
    ],
)

sc_cc_test(
    name = "config_monitoring_service_test",
    srcs = [
        "config_monitoring_service_test.cc",
        "gnmi_publisher_test.cc",
        "yang_parse_tree_mock.h",
        "yang_parse_tree_test.cc",
    ],
    deps = [
        ":config_monitoring_service",
        ":error_buffer",
        ":gnmi_publisher_mock",
        ":subscribe_reader_writer_mock",
        ":switch_mock",
        ":test_main",
        ":writer_mock",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//protobuf",
        "//sandblaze/prebuilt/grpc",
        "//stratum:gnmi_grpc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

cc_library(
    name = "subscribe_reader_writer_mock",
    testonly = 1,
    hdrs = ["subscribe_reader_writer_mock.h"],
    deps = [
        "//testing/base/public:gmock_no_google3",
        "//sandblaze/prebuilt/grpc",
        "//stratum:gnmi_grpc_proto",
    ],
)

cc_library(
    name = "gnmi_publisher_mock",
    testonly = 1,
    hdrs = [
        "gnmi_events.h",
        "gnmi_publisher.h",
        "gnmi_publisher_mock.h",
        "yang_parse_tree.h",
    ],
    deps = [
        ":common_proto",
        ":switch_interface",
        ":switch_mock",
        ":writer_interface",
        "//testing/base/public:gmock_no_google3",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/synchronization",
        "//protobuf",
        "//stratum:gnmi_grpc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/public/lib:error",
        "//stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

sc_cc_lib(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "//absl/base:core_headers",
    ],
)

sc_cc_lib(
    name = "diag_service",
    srcs = [
        "diag_service.cc",
    ],
    hdrs = [
        "diag_service.h",
    ],
    deps = [
        ":common_proto",
        ":error_buffer",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
        "//stratum/glue/gnoi:diag_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "diag_service_test",
    srcs = [
        "diag_service_test.cc",
    ],
    deps = [
        ":diag_service",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "//base",
        "//net/util:ports",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/gnoi:system_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "error_buffer",
    srcs = ["error_buffer.cc"],
    hdrs = ["error_buffer.h"],
    deps = [
        "//base",
        "//file/base:path",
        "//absl/base:core_headers",
        "//absl/strings",
        "//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//util/gtl:source_location",
    ],
)

sc_cc_test(
    name = "error_buffer_test",
    srcs = [
        "error_buffer_test.cc",
    ],
    deps = [
        ":error_buffer",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "file_service",
    srcs = [
        "file_service.cc",
    ],
    hdrs = [
        "file_service.h",
    ],
    deps = [
        ":common_proto",
        ":error_buffer",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
        "//stratum/glue/gnoi:file_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:map_util",
        "//util/task:status",
    ],
)

sc_cc_test(
    name = "file_service_test",
    srcs = [
        "file_service_test.cc",
    ],
    deps = [
        ":error_buffer",
        ":file_service",
        ":switch_mock",
        ":test_main",
        "//base",
        "//net/util:ports",
        "//testing/base/public:gunit_no_google3",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue/gnoi:system_grpc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "hal",
    srcs = ["hal.cc"],
    hdrs = ["hal.h"],
    deps = [
        ":admin_service",
        ":certificate_management_service",
        ":common_proto",
        ":config_monitoring_service",
        ":diag_service",
        ":error_buffer",
        ":file_service",
        ":p4_service",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/memory",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum/glue:logging",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//util/gtl:flat_hash_map",
    ],
)

sc_cc_test(
    name = "hal_test",
    srcs = [
        "hal_test.cc",
    ],
    deps = [
        ":hal",
        ":switch_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/strings",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/security:credentials_manager_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "p4_service",
    srcs = ["p4_service.cc"],
    hdrs = ["p4_service.h"],
    deps = [
        ":channel_writer_wrapper",
        ":common_proto",
        ":error_buffer",
        ":server_writer_wrapper",
        ":switch_interface",
        "//base",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/numeric:int128",
        "//absl/strings",
        "//absl/synchronization",
        "//absl/time",
        "//protobuf",
        "//sandblaze/p4lang:p4info_v1_proto",
        "//sandblaze/p4lang:p4runtime_v1_grpc_proto",
        "//sandblaze/prebuilt/grpc",
        "//stratum:code_proto",
        "//stratum:status_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:forwarding_pipeline_configs_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:cleanup",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "p4_service_test",
    srcs = [
        "p4_service_test.cc",
    ],
    deps = [
        ":error_buffer",
        ":p4_service",
        ":switch_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//absl/base:core_headers",
        "//absl/memory",
        "//absl/numeric:int128",
        "//absl/strings",
        "//absl/synchronization",
        "//sandblaze/prebuilt/grpc",
        "//stratum:code_proto",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "phal_interface",
    hdrs = ["phal_interface.h"],
    deps = [
        ":common_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib/channel",
    ],
)

cc_library(
    name = "phal_mock",
    testonly = 1,
    hdrs = ["phal_mock.h"],
    deps = [
        ":phal_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

sc_cc_lib(
    name = "writer_interface",
    hdrs = ["writer_interface.h"],
)

cc_library(
    name = "writer_mock",
    testonly = 1,
    hdrs = ["writer_mock.h"],
    deps = [
        ":writer_interface",
        "//testing/base/public:gmock_no_google3",
    ],
)

sc_cc_lib(
    name = "server_writer_wrapper",
    hdrs = ["server_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "//sandblaze/prebuilt/grpc",
    ],
)

sc_cc_lib(
    name = "channel_writer_wrapper",
    hdrs = ["channel_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "//stratum/lib/channel",
    ],
)

sc_cc_lib(
    name = "switch_interface",
    hdrs = [
        "gnmi_events.h",
        "switch_interface.h",
    ],
    deps = [
        ":common_proto",
        ":writer_interface",
        "//absl/base:core_headers",
        "//absl/container:flat_hash_map",
        "//absl/synchronization",
        "//sandblaze/p4lang:p4runtime_v1_grpc_proto",
        "//stratum:gnmi_grpc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:timer_daemon",
        "//stratum/lib/channel",
        "//stratum/public/proto:hal_grpc_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
    ],
)

cc_library(
    name = "switch_mock",
    testonly = 1,
    hdrs = ["switch_mock.h"],
    deps = [
        ":switch_interface",
        "//testing/base/public:gunit_no_google3",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        ":common_proto",
        "//absl/base:core_headers",
        "//absl/strings",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//absl/container:flat_hash_set",
        "//absl/strings",
        "//protobuf",
        "//stratum/lib:constants",
        "//util/gtl:flat_hash_set",
    ],
)

sc_cc_lib(
    name = "openconfig_converter",
    srcs = [
        "openconfig_converter.cc",
    ],
    hdrs = [
        "openconfig_converter.h",
    ],
    deps = [
        ":common_proto",
        "//absl/strings",
        "//stratum/glue:logging",
        "//stratum/glue/openconfig/proto:old_openconfig_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "openconfig_converter_test",
    srcs = ["openconfig_converter_test.cc"],
    deps = [
        ":openconfig_converter",
        ":test_main",
        ":testdata",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
    ],
)

cc_library(
    name = "testdata",
    data = glob(["testdata/**"]),
)
