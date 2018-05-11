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
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_lib(
    name = "client_sync_reader_writer",
    hdrs = ["client_sync_reader_writer.h"],
    deps = [
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum/glue:logging",
    ],
)

sc_proto_lib(
    name = "common_proto",
    hdrs = ["common.proto"],
    deps = [
        "//third_party/sandblaze/p4lang:p4runtime_proto",
    ],
)

sc_cc_lib(
    name = "config_monitoring_service",
    srcs = [
        "config_monitoring_service.cc",
        "gnmi_publisher.cc",
        "yang_parse_tree.cc",
    ],
    hdrs = [
        "config_monitoring_service.h",
        "gnmi_publisher.h",
        "yang_parse_tree.h",
    ],
    deps = [
        ":channel_writer_wrapper",
        ":common_proto",
        ":conversion",
        ":error_buffer",
        ":switch_interface",
        ":writer_interface",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/strings:str_format",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:timer_daemon",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/security:auth_policy_checker",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:hal_grpc_proto",
        "//third_party/stratum/public/proto:openconfig_proto",
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
        "//third_party/absl/memory",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:timer_daemon",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/security:auth_policy_checker_mock",
        "//third_party/stratum/public/lib:error",
    ],
)

cc_library(
    name = "subscribe_reader_writer_mock",
    testonly = 1,
    hdrs = ["mock_subscribe_reader_writer.h"],
    deps = [
        "//testing/base/public:gmock_no_google3",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/sandblaze/prebuilt/grpc",
    ],
)

cc_library(
    name = "gnmi_publisher_mock",
    testonly = 1,
    hdrs = [
        "gnmi_events.h",
        "gnmi_publisher.h",
        "mock_gnmi_publisher.h",
        "yang_parse_tree.h",
    ],
    deps = [
        ":switch_interface",
        ":switch_mock",
        ":writer_interface",
        "//testing/base/public:gmock_no_google3",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/protobuf",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/lib:timer_daemon",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:hal_grpc_proto",
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
    name = "error_buffer",
    srcs = ["error_buffer.cc"],
    hdrs = ["error_buffer.h"],
    deps = [
        "//base",
        "//file/base:path",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/lib:macros",
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
        "//third_party/absl/memory",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "hal",
    srcs = ["hal.cc"],
    hdrs = ["hal.h"],
    deps = [
        ":common_proto",
        ":config_monitoring_service",
        ":error_buffer",
        ":p4_service",
        ":switch_interface",
        "//base",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/security:auth_policy_checker",
        "//third_party/stratum/lib/security:credentials_manager",
        "//util/gtl:flat_hash_map",
    ],
)

sc_cc_test(
    name = "hal_test",
    srcs = [
        "hal_test.cc",
    ],
    data = ["//tools/test:requires_ipv4_loopback"],
    deps = [
        ":hal",
        ":switch_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/strings",
        "//third_party/stratum/glue/net_util:ports",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/security:auth_policy_checker_mock",
        "//third_party/stratum/lib/security:credentials_manager_mock",
        "//third_party/stratum/public/lib:error",
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
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/numeric:int128",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/absl/time",
        "//third_party/protobuf",
        "//third_party/sandblaze/p4lang:p4info_proto",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum:code_proto",
        "//third_party/stratum:status_proto",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/channel",
        "//third_party/stratum/lib/security:auth_policy_checker",
        "//third_party/stratum/public/lib:error",
        "//util/gtl:cleanup",
        "//util/gtl:map_util",
    ],
)

sc_cc_test(
    name = "p4_service_test",
    srcs = [
        "p4_service_test.cc",
    ],
    data = ["//tools/test:requires_ipv4_loopback"],
    deps = [
        ":error_buffer",
        ":p4_service",
        ":switch_mock",
        ":test_main",
        "//base",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/memory",
        "//third_party/absl/numeric:int128",
        "//third_party/absl/strings",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/prebuilt/grpc",
        "//third_party/stratum:code_proto",
        "//third_party/stratum/glue/net_util:ports",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
        "//third_party/stratum/lib/security:auth_policy_checker_mock",
        "//third_party/stratum/public/lib:error",
    ],
)

sc_cc_lib(
    name = "phal_interface",
    hdrs = ["phal_interface.h"],
    deps = [
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib/channel",
        "//third_party/stratum/public/proto:hal_grpc_proto",
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
        "//third_party/sandblaze/prebuilt/grpc",
    ],
)

sc_cc_lib(
    name = "channel_writer_wrapper",
    hdrs = ["channel_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "//third_party/stratum/lib/channel",
    ],
)

sc_cc_lib(
    name = "switch_interface",
    hdrs = [
        "gnmi_events.h",
        "switch_interface.h",
    ],
    deps = [
        ":writer_interface",
        "//third_party/absl/base:core_headers",
        "//third_party/absl/synchronization",
        "//third_party/sandblaze/gnmi:gnmi_grpc_proto",
        "//third_party/sandblaze/p4lang:p4runtime_grpc_proto",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:timer_daemon",
        "//third_party/stratum/lib/channel",
        "//third_party/stratum/public/proto:hal_grpc_proto",
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
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue:logging",
    ],
)

sc_cc_lib(
    name = "utils",
    srcs = ["utils.cc"],
    hdrs = ["utils.h"],
    deps = [
        "//third_party/absl/base:core_headers",
        "//third_party/absl/strings",
        "//third_party/stratum/hal/lib/common:common_proto",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/lib:macros",
        "//third_party/stratum/public/lib:error",
        "//third_party/stratum/public/proto:hal_grpc_proto",
    ],
)

sc_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//third_party/absl/strings",
        "//third_party/protobuf",
        "//third_party/stratum/lib:constants",
    ],
)

sc_cc_lib(
    name = "conversion",
    srcs = [
        "hal_config_to_oc.cc",
        "oc_to_hal_config.cc",
    ],
    hdrs = [
        "hal_config_to_oc.h",
        "oc_to_hal_config.h",
    ],
    deps = [
        "//third_party/absl/strings",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/glue/status",
        "//third_party/stratum/glue/status:status_macros",
        "//third_party/stratum/glue/status:statusor",
        "//third_party/stratum/lib:constants",
        "//third_party/stratum/public/proto:hal_proto",
        "//third_party/stratum/public/proto:openconfig_proto",
        "//util/gtl:map_util",
    ],
)

cc_test(
    name = "oc_to_hal_config_test",
    srcs = ["oc_to_hal_config_test.cc"],
    deps = [
        ":conversion",
        ":test_main",
        ":testdata",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
    ],
)

cc_test(
    name = "hal_config_to_oc_test",
    srcs = ["hal_config_to_oc_test.cc"],
    deps = [
        ":conversion",
        ":test_main",
        ":testdata",
        ":utils",
        "//testing/base/public:gunit_no_google3",
        "//third_party/stratum/glue/status:status_test_util",
        "//third_party/stratum/lib:utils",
    ],
)

cc_library(
    name = "testdata",
    data = glob(["testdata/**"]),
)
