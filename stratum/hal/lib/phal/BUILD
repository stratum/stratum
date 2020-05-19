# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//bazel:rules.bzl",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "stratum_package",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# The following config settings allow to specify which PHAL backends to use.
# Each can theoretically enable independently, as long as they configure
# different parts of the attribute database.

config_setting(
    name = "with_onlp",
    define_values = {
        "phal_with_onlp": "true",
    },
)

config_setting(
    name = "without_onlp",
    define_values = {
        "phal_with_onlp": "false",
    },
)

config_setting(
    name = "with_tai",
    define_values = {
        "phal_with_tai": "true",
    },
)

config_setting(
    name = "without_tai",
    define_values = {
        "phal_with_tai": "false",
    },
)

onlp_defines = [
    "WITH_ONLP",
]

onlp_deps = [
    "//stratum/hal/lib/phal/onlp:onlp_phal",
]

tai_defines = [
    "WITH_TAI",
]

tai_deps = [
    "//stratum/hal/lib/phal/tai:tai_phal",
]

# Default Phal selection (the absence of both with_* and without_*) is done by
# filling the //conditions:default" or leaving it empty.
phal_defines = select({
    ":with_onlp": onlp_defines,
    ":without_onlp": [],
    "//conditions:default": onlp_defines,
}) + select({
    ":with_tai": tai_defines,
    ":without_tai": [],
    "//conditions:default": [],
})

phal_deps = select({
    ":with_onlp": onlp_deps,
    ":without_onlp": [],
    "//conditions:default": onlp_deps,
}) + select({
    ":with_tai": tai_deps,
    ":without_tai": [],
    "//conditions:default": [],
})

stratum_cc_library(
    name = "attribute_database_interface",
    hdrs = ["attribute_database_interface.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/types:variant",
    ],
)

stratum_cc_library(
    name = "attribute_database_mock",
    testonly = 1,
    hdrs = ["attribute_database_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "buffer_tools",
    hdrs = ["buffer_tools.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "managed_attribute",
    hdrs = ["managed_attribute.h"],
    deps = [
        ":attribute_database_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "managed_attribute_mock",
    testonly = 1,
    hdrs = ["managed_attribute_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "//stratum/glue/status",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "attribute_database",
    srcs = ["attribute_database.cc"],
    hdrs = ["attribute_database.h"],
    deps = [
        ":attribute_database_interface",
        ":attribute_group",
        ":datasource",
        ":db_cc_proto",
        ":dummy_threadpool",
        ":managed_attribute",
        ":phal_cc_proto",
        ":phaldb_service",
        ":switch_configurator_interface",
        ":system_interface",
        ":threadpool_interface",
        ":udev_event_handler",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "attribute_database_test",
    srcs = ["attribute_database_test.cc"],
    data = [
        #FIXME(boc) google only"//stratum/hal/config:legacy_phal_init_config_generic_trident2_hercules.pb.txt",
    ],
    deps = [
        "system_fake",
        ":attribute_database",
        ":attribute_group_mock",
        ":db_cc_proto",
        ":dummy_threadpool",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "adapter",
    srcs = ["adapter.cc"],
    hdrs = ["adapter.h"],
    deps = [
        ":attribute_database_interface",
        ":db_cc_proto",
        "//stratum/glue/status",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
    ],
)

stratum_cc_library(
    name = "sfp_adapter",
    srcs = ["sfp_adapter.cc"],
    hdrs = ["sfp_adapter.h"],
    deps = [
        ":adapter",
        ":attribute_database_interface",
        ":db_cc_proto",
        ":managed_attribute",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "sfp_adapter_test",
    srcs = ["sfp_adapter_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":sfp_adapter",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_cc_proto",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "optics_adapter",
    srcs = ["optics_adapter.cc"],
    hdrs = ["optics_adapter.h"],
    deps = [
        ":adapter",
        ":attribute_database_interface",
        ":db_cc_proto",
        ":managed_attribute",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "optics_adapter_test",
    srcs = ["optics_adapter_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":optics_adapter",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "attribute_group",
    srcs = ["attribute_group.cc"],
    hdrs = ["attribute_group.h"],
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        ":threadpool_interface",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "attribute_group_test",
    srcs = ["attribute_group_test.cc"],
    deps = [
        ":attribute_group",
        ":datasource",
        ":datasource_mock",
        ":dummy_threadpool",
        ":managed_attribute",
        ":managed_attribute_mock",
        ":test_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_cc_proto",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "attribute_group_mock",
    testonly = 1,
    srcs = ["attribute_group_mock.cc"],
    hdrs = ["attribute_group_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_group",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "datasource",
    srcs = ["datasource.cc"],
    hdrs = ["datasource.h"],
    deps = [
        ":attribute_database_interface",
        ":managed_attribute",
        ":phal_cc_proto",
        "//stratum/glue/status",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "datasource_mock",
    testonly = 1,
    srcs = ["datasource_mock.cc"],
    hdrs = ["datasource_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":datasource",
        "//stratum/glue/status",
        "@com_google_googletest//:gtest",
    ],
)

proto_library(
    name = "db_proto",
    srcs = ["db.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

cc_proto_library(
    name = "db_cc_proto",
    deps = [":db_proto"],
)

cc_grpc_library(
    name = "db_cc_grpc",
    srcs = [":db_proto"],
    grpc_only = True,
    deps = [":db_cc_proto"],
)

stratum_cc_library(
    name = "dummy_threadpool",
    srcs = ["dummy_threadpool.cc"],
    hdrs = ["dummy_threadpool.h"],
    deps = [
        ":threadpool_interface",
        "//stratum/glue/status",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "filepath_stringsource",
    hdrs = ["filepath_stringsource.h"],
    deps = [
        ":stringsource_interface",
        ":system_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
    ],
)

stratum_cc_library(
    name = "fixed_layout_datasource",
    srcs = ["fixed_layout_datasource.cc"],
    hdrs = ["fixed_layout_datasource.h"],
    deps = [
        ":buffer_tools",
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
    ],
)

stratum_cc_test(
    name = "fixed_layout_datasource_test",
    srcs = ["fixed_layout_datasource_test.cc"],
    copts = ["-funsigned-char"],
    deps = [
        ":fixed_layout_datasource",
        ":fixed_stringsource",
        ":test_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/phal/test:test_cc_proto",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "fixed_stringsource",
    hdrs = ["fixed_stringsource.h"],
    deps = [
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "phaldb_service",
    srcs = ["phaldb_service.cc"],
    hdrs = ["phaldb_service.h"],
    deps = [
        ":adapter",
        ":attribute_database_interface",
        ":db_cc_grpc",
        ":db_cc_proto",
        ":managed_attribute",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:cleanup",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:channel_writer_wrapper",
        "//stratum/hal/lib/common:error_buffer",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/channel",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_binary(
    name = "phal_cli",
    srcs = ["phal_cli.cc"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":db_cc_grpc",
        ":db_cc_proto",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
    ],
)

stratum_cc_test(
    name = "phaldb_service_test",
    srcs = ["phaldb_service_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":phaldb_service",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/lib:utils",
        "//stratum/lib/security:auth_policy_checker_mock",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

''' FIXME(boc) google only
stratum_cc_library(
    name = "google_phal",
    srcs = ["google_phal.cc"],
    hdrs = ["google_phal.h"],
    deps = [
        ":attribute_database",
        ":attribute_database_interface",
        ":system_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
    ],
)

stratum_cc_test(
    name = "google_phal_test",
    srcs = ["google_phal_test.cc"],
    deps = [
        ":attribute_database_mock",
        ":google_phal",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/channel:channel_mock",
        "//stratum/lib/test_utils:matchers",
    ],
)
'''

stratum_package(
    name = "phal_cli_pkg",
    arches = EMBEDDED_ARCHES,
    bins = [":phal_cli"],
)

stratum_cc_library(
    name = "reader_writer_datasource",
    hdrs = ["reader_writer_datasource.h"],
    deps = [
        ":datasource",
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

cc_test(
    name = "reader_writer_datasource_test",
    srcs = ["reader_writer_datasource_test.cc"],
    deps = [
        ":filepath_stringsource",
        ":reader_writer_datasource",
        ":system_fake",
        ":test_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "regex_datasource",
    srcs = ["regex_datasource.cc"],
    hdrs = ["regex_datasource.h"],
    deps = [
        ":datasource",
        ":managed_attribute",
        ":stringsource_interface",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_googlesource_code_re2//:re2",
    ],
)

stratum_cc_test(
    name = "regex_datasource_test",
    srcs = ["regex_datasource_test.cc"],
    deps = [
        ":datasource",
        ":fixed_stringsource",
        ":regex_datasource",
        ":test_util",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "stringsource_interface",
    hdrs = ["stringsource_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "system_fake",
    testonly = 1,
    srcs = ["system_fake.cc"],
    hdrs = ["system_fake.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "system_interface",
    hdrs = ["system_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "system_real",
    srcs = ["system_real.cc"],
    hdrs = ["system_real.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":system_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_systemd_systemd//:libudev",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "system_interface_mock",
    testonly = 1,
    hdrs = ["system_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":system_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "test_util",
    testonly = 1,
    hdrs = ["test_util.h"],
    arches = HOST_ARCHES,
    deps = [
        ":attribute_database_interface",
        ":datasource",
        ":managed_attribute",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "threadpool_interface",
    hdrs = ["threadpool_interface.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "threadpool_interface_mock",
    testonly = 1,
    hdrs = ["threadpool_interface_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":threadpool_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "udev_event_handler",
    srcs = ["udev_event_handler.cc"],
    hdrs = ["udev_event_handler.h"],
    deps = [
        ":system_interface",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "udev_event_handler_mock",
    testonly = 1,
    hdrs = ["udev_event_handler_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_event_handler",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "udev_event_handler_test",
    srcs = ["udev_event_handler_test.cc"],
    deps = [
        ":system_fake",
        ":udev_event_handler",
        ":udev_event_handler_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

''' FIXME(boc) google only
stratum_cc_library(
    name = "legacy_phal",
    srcs = ["legacy_phal.cc"],
    hdrs = ["legacy_phal.h"],
    deps = [
        ":udev_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/glue/gtl:stl_util",
    ],
)
'''

proto_library(
    name = "phal_proto",
    srcs = ["phal.proto"],
    deps = [
        "//stratum/hal/lib/common:common_proto",
    ],
)

cc_proto_library(
    name = "phal_cc_proto",
    deps = [":phal_proto"],
)

'''FIXME(boc) google only
stratum_cc_library(
    name = "phal_proxy",
    srcs = ["phal_proxy.cc"],
    hdrs = ["phal_proxy.h"],
    deps = [
        ":phal_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/hal/lib/common:client_sync_reader_writer",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib/sandcastle:hardware_status_service_grpc_proto",
        "//stratum/public/proto:error_proto",
    ],
)
'''

stratum_cc_library(
    name = "phal_backend_interface",
    hdrs = ["phal_backend_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)

stratum_cc_library(
    name = "phal",
    srcs = ["phal.cc"],
    hdrs = ["phal.h"],
    defines = phal_defines,
    deps = [
        ":attribute_database",
        ":optics_adapter",
        ":phal_backend_interface",
        ":phal_cc_proto",
        ":sfp_adapter",
        ":sfp_configurator",
        ":switch_configurator_interface",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
    ] + phal_deps,
)

stratum_cc_test(
    name = "phal_test",
    srcs = ["phal_test.cc"],
    deps = [
        ":phal",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "phal_sim",
    srcs = ["phal_sim.cc"],
    hdrs = ["phal_sim.h"],
    deps = [
        ":phal_cc_proto",
        ":sfp_configurator",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "udev",
    srcs = ["udev.cc"],
    hdrs = ["udev.h"],
    arches = EMBEDDED_ARCHES,
    deps = [
        ":udev_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_github_systemd_systemd//:libudev",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "udev_interface",
    hdrs = ["udev_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "udev_mock",
    testonly = 1,
    hdrs = ["udev_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":udev_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "switch_configurator_interface",
    hdrs = ["switch_configurator_interface.h"],
    deps = [
        ":attribute_group",
        "//stratum/glue/status",
        "//stratum/hal/lib/phal:phal_cc_proto",
    ],
)

stratum_cc_library(
    name = "sfp_configurator",
    hdrs = ["sfp_configurator.h"],
    deps = [
        ":attribute_group",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
    ],
)
