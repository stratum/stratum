# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

load("//bazel:deps.bzl", "P4RUNTIME_VER")

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "admin_service",
    srcs = [
        "admin_service.cc",
    ],
    hdrs = [
        "admin_service.h",
    ],
    deps = [
        ":admin_utils",
        ":admin_utils_interface",
        "@com_google_googletest//:gtest",
        ":common_cc_proto",
        ":error_buffer",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "@com_github_openconfig_gnoi//:system_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_test(
    name = "admin_service_test",
    srcs = [
        "admin_service_test.cc",
    ],
    deps = [
        ":admin_service",
        ":admin_utils_mock",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "@com_github_google_glog//:glog",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        #FIXME(boc) "@com_github_openconfig_gnoi//:system_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "admin_utils_interface",
    hdrs = ["admin_utils_interface.h"],
)

cc_library(
    name = "admin_utils_mock",
    testonly = 1,
    hdrs = ["admin_utils_mock.h"],
    deps = [
        ":admin_utils_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "admin_utils",
    srcs = ["admin_utils.cc"],
    deps = [
        ":admin_utils_interface",
        "//stratum/glue:logging",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_openconfig_gnoi//:types_cc_proto",
        "@com_github_openconfig_gnoi//:common_cc_proto",
        "@com_github_openconfig_gnoi//:system_cc_grpc",
        "@com_google_absl//absl/memory",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

stratum_cc_library(
    name = "certificate_management_service",
    srcs = [
        "certificate_management_service.cc",
    ],
    hdrs = [
        "certificate_management_service.h",
    ],
    deps = [
        ":common_cc_proto",
        ":error_buffer",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "@com_github_openconfig_gnoi//:cert_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_test(
    name = "certificate_management_service_test",
    srcs = [
        "certificate_management_service_test.cc",
    ],
    deps = [
        ":certificate_management_service",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "@com_github_google_glog//:glog",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        #FIXME(boc) "@com_github_openconfig_gnoi//:system_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "client_sync_reader_writer",
    hdrs = ["client_sync_reader_writer.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
    ],
)

proto_library(
    name = "common_proto",
    srcs = ["common.proto"],
)

cc_proto_library(
    name = "common_cc_proto",
    deps = [":common_proto"]
)

py_proto_library(
    name = "common_py_proto",
    deps = [
        ":common_proto"
    ],
)

stratum_cc_library(
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
        ":common_cc_proto",
        ":error_buffer",
        ":openconfig_converter",
        ":switch_interface",
        ":writer_interface",
        ":utils",
        ":constants",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "//stratum/glue:logging",
        "@com_github_openconfig_hercules//:openconfig_cc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        #FIXME(boc)
        #"//util/time:clock",
    ],
    data = [
        # TODO(Yi): generate this when we generating the OpenConfig proto files
        "gnmi_caps.pb.txt"
    ],
)

stratum_cc_test(
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
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:constants",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        ":testdata",
        "@com_github_openconfig_hercules//:openconfig_cc_proto",
    ],
    data = [
        # TODO(Yi): generate this when we generating the OpenConfig proto files
        "gnmi_caps.pb.txt"
    ],
)

cc_library(
    name = "subscribe_reader_writer_mock",
    testonly = 1,
    hdrs = ["subscribe_reader_writer_mock.h"],
    deps = [
        "@com_google_googletest//:gtest",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
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
        ":common_cc_proto",
        ":switch_interface",
        ":switch_mock",
        ":writer_interface",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/lib:timer_daemon",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "constants",
    hdrs = ["constants.h"],
    deps = [
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "diag_service",
    srcs = [
        "diag_service.cc",
    ],
    hdrs = [
        "diag_service.h",
    ],
    deps = [
        ":common_cc_proto",
        ":error_buffer",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "@com_github_openconfig_gnoi//:diag_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "diag_service_test",
    srcs = [
        "diag_service_test.cc",
    ],
    deps = [
        ":diag_service",
        ":error_buffer",
        ":switch_mock",
        ":test_main",
        "@com_github_google_glog//:glog",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        #FIXME(boc) "@com_github_openconfig_gnoi//:system_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "error_buffer",
    srcs = ["error_buffer.cc"],
    hdrs = ["error_buffer.h"],
    deps = [
        "@com_github_google_glog//:glog",
        #"//file/base:path",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/glue/gtl:source_location",
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
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "file_service",
    srcs = [
        "file_service.cc",
    ],
    hdrs = [
        "file_service.h",
    ],
    deps = [
        ":common_cc_proto",
        ":error_buffer",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "@com_github_openconfig_gnoi//:file_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_test(
    name = "file_service_test",
    srcs = [
        "file_service_test.cc",
    ],
    deps = [
        ":error_buffer",
        ":file_service",
        ":switch_mock",
        ":test_main",
        "@com_github_google_glog//:glog",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        #FIXME(boc) "@com_github_openconfig_gnoi//:system_cc_grpc",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_library(
    name = "hal",
    srcs = ["hal.cc"],
    hdrs = ["hal.h"],
    deps = [
        ":admin_service",
        ":certificate_management_service",
        ":common_cc_proto",
        ":config_monitoring_service",
        ":diag_service",
        ":error_buffer",
        ":file_service",
        ":p4_service",
        ":switch_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "//stratum/procmon:procmon_cc_grpc",
    ],
)

stratum_cc_test(
    name = "hal_test",
    srcs = [
        "hal_test.cc",
    ],
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
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "//stratum/procmon:procmon_cc_grpc",
    ],
)

stratum_cc_library(
    name = "p4_service",
    srcs = ["p4_service.cc"],
    hdrs = ["p4_service.h"],
    deps = [
        ":channel_writer_wrapper",
        ":common_cc_proto",
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
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/p4:forwarding_pipeline_configs_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "//stratum/glue/gtl:cleanup",
        "//stratum/glue/gtl:map_util",
    ],
    defines = [
        "P4RUNTIME_VER=" + P4RUNTIME_VER,
    ]
)

stratum_cc_test(
    name = "p4_service_test",
    srcs = [
        "p4_service_test.cc",
    ],
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
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
    ],
    defines = [
        "P4RUNTIME_VER=" + P4RUNTIME_VER,
    ]
)

stratum_cc_library(
    name = "phal_interface",
    hdrs = ["phal_interface.h"],
    deps = [
        ":common_cc_proto",
        "//stratum/hal/lib/phal:sfp_configurator",
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
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "server_writer_wrapper",
    hdrs = ["server_writer_wrapper.h"],
    deps = [
        ":writer_interface",
        "@com_github_grpc_grpc//:grpc++",
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
        ":common_cc_proto",
        ":writer_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:timer_daemon",
        "//stratum/lib/channel",
        "//stratum/glue/gtl:map_util",
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
        ":common_cc_proto",
        ":constants",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_proto",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
    ],
)

stratum_cc_test(
    name = "utils_test",
    srcs = ["utils_test.cc"],
    deps = [
        ":test_main",
        ":utils",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
        "//stratum/lib:constants",
    ],
)

stratum_cc_library(
    name = "openconfig_converter",
    srcs = [
        "openconfig_converter.cc",
    ],
    hdrs = [
        "openconfig_converter.h",
    ],
    deps = [
        ":common_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_github_openconfig_hercules//:openconfig_cc_proto",
        "//stratum/public/proto:openconfig_goog_bcm_cc_proto",
        "//stratum/glue:logging",
        #FIXME(boc) "//stratum/glue/openconfig/proto:old_openconfig_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:utils",
    ],
)

stratum_cc_test(
    name = "openconfig_converter_test",
    srcs = ["openconfig_converter_test.cc"],
    deps = [
        ":openconfig_converter",
        ":test_main",
        ":testdata",
        ":utils",
        "@com_google_googletest//:gtest",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
    ],
)

cc_library(
    name = "testdata",
    data = glob(["testdata/**"]),
)
