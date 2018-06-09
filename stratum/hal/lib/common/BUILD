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
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_cc_proto_library",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "client_sync_reader_writer",
    hdrs = ["client_sync_reader_writer.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_grpc//:grpc++",
        "//stratum/glue:logging",
    ],
)

stratum_cc_proto_library(
    name = "common_proto",
    hdrs = ["common.proto"],
    deps = [
        "@com_github_p4lang_PI//:p4runtime_proto",
    ],
)

stratum_cc_library(
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_openconfig_gnmi//:gnmi_proto",
        "@com_google_grpc//:grpc++",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/public/proto:hal_proto",
        "//stratum/public/proto:openconfig_proto",
        "//util/gtl:flat_hash_map",
        "//util/gtl:map_util",
        "//util/time:clock",
    ],
)

stratum_cc_test(
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_openconfig_gnmi//:gnmi_proto",
        "@com_google_grpc//:grpc++",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/public/lib:error",
    ],
)

cc_library(
    name = "subscribe_reader_writer_mock",
    testonly = 1,
    hdrs = ["mock_subscribe_reader_writer.h"],
    deps = [
        "//testing/base/public:gmock_no_google3",
        "@com_github_openconfig_gnmi//:gnmi_proto",
        "@com_google_grpc//:grpc++",
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
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_openconfig_gnmi//:gnmi_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:timer_daemon",
        "//stratum/public/lib:error",
        "//stratum/public/proto:hal_proto",
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
    name = "error_buffer",
    srcs = ["error_buffer.cc"],
    hdrs = ["error_buffer.h"],
    deps = [
        "@com_github_google_glog//:glog",
        #FIXME "//file/base:path",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//util/gtl:source_location",
    ],
)

stratum_cc_test(
    name = "error_buffer_test",
    srcs = [
        "error_buffer_test.cc",
    ],
    deps = [
        ":error_buffer",
        ":test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "//stratum/glue/status:status_test_util",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "hal",
    srcs = ["hal.cc"],
    hdrs = ["hal.h"],
    deps = [
        ":common_proto",
        ":config_monitoring_service",
        ":error_buffer",
        ":p4_service",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_grpc//:grpc++",
        "//stratum/glue:logging",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//util/gtl:flat_hash_map",
    ],
)

stratum_cc_test(
    name = "hal_test",
    srcs = [
        "hal_test.cc",
    ],
    data = ["//tools/test:requires_ipv4_loopback"],
    deps = [
        ":hal",
        ":switch_mock",
        ":test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/strings",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/security:credentials_manager_mock",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "p4_service",
    srcs = ["p4_service.cc"],
    hdrs = ["p4_service.h"],
    deps = [
        ":channel_writer_wrapper",
        ":common_proto",
        ":error_buffer",
        ":server_writer_wrapper",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_github_p4lang_PI//:p4info_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
        "@com_google_grpc//:grpc++",
        "@com_github_googleapis//:code_proto",
        "@com_github_googleapis//:status_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//util/gtl:cleanup",
        "//util/gtl:map_util",
    ],
)

stratum_cc_test(
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_grpc//:grpc++",
        "@com_github_googleapis//:code_proto",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "phal_interface",
    hdrs = ["phal_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib/channel",
        "//stratum/public/proto:hal_proto",
    ],
)

cc_library(
    name = "phal_mock",
    testonly = 1,
    hdrs = ["phal_mock.h"],
    deps = [
        ":phal_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
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

stratum_cc_library(
    name = "server_writer_wrapper",
    hdrs = ["server_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "@com_google_grpc//:grpc++",
    ],
)

stratum_cc_library(
    name = "channel_writer_wrapper",
    hdrs = ["channel_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "switch_interface",
    hdrs = [
        "gnmi_events.h",
        "switch_interface.h",
    ],
    deps = [
        ":writer_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_github_openconfig_gnmi//:gnmi_proto",
        "@com_github_p4lang_PI//:p4runtime_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:timer_daemon",
        "//stratum/lib/channel",
        "//stratum/public/proto:hal_proto",
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
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
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
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "//stratum/public/proto:hal_proto",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "//stratum/lib:constants",
    ],
)

stratum_cc_library(
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
        "@com_google_absl//absl/strings",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:constants",
        "//stratum/public/proto:hal_proto",
        "//stratum/public/proto:openconfig_proto",
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
        "@com_google_googletest//:gtest",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
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
        "@com_google_googletest//:gtest",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
    ],
)

cc_library(
    name = "testdata",
    data = glob(["testdata/**"]),
)
