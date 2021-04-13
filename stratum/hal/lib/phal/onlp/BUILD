# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "onlp_event_handler",
    srcs = ["onlp_event_handler.cc"],
    hdrs = ["onlp_event_handler.h"],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_library(
    name = "onlp_event_handler_mock",
    testonly = 1,
    hdrs = ["onlp_event_handler_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":onlp_event_handler",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "onlp_event_handler_test",
    srcs = ["onlp_event_handler_test.cc"],
    deps = [
        ":onlp_event_handler",
        ":onlp_event_handler_mock",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_wrapper",
    srcs = [
        "onlp_wrapper.cc",
    ],
    hdrs = [
        "onlp_wrapper.h",
    ],
    linkopts = ["-ldl"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@local_onlp_bin//:onlp_headers",
    ],
)

stratum_cc_library(
    name = "onlp_wrapper_mock",
    hdrs = [
        "onlp_wrapper_mock.h",
    ],
    arches = HOST_ARCHES,
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "@com_google_googletest//:gtest",
    ],
)

#FIXME remove when onlp_wrapper.h is stable
stratum_cc_library(
    name = "onlp_wrapper_fake",
    hdrs = [
        "onlp_wrapper_fake.h",
    ],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlp_sfp_datasource",
    srcs = [
        "onlp_sfp_datasource.cc",
    ],
    hdrs = [
        "onlp_sfp_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlp_sfp_datasource_test",
    srcs = [
        "onlp_sfp_datasource_test.cc",
    ],
    deps = [
        ":onlp_sfp_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_fan_datasource",
    srcs = [
        "onlp_fan_datasource.cc",
    ],
    hdrs = [
        "onlp_fan_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlp_fan_datasource_test",
    srcs = [
        "onlp_fan_datasource_test.cc",
    ],
    deps = [
        ":onlp_fan_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_psu_datasource",
    srcs = [
        "onlp_psu_datasource.cc",
    ],
    hdrs = [
        "onlp_psu_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlp_psu_datasource_test",
    srcs = [
        "onlp_psu_datasource_test.cc",
    ],
    deps = [
        ":onlp_psu_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_thermal_datasource",
    srcs = [
        "onlp_thermal_datasource.cc",
    ],
    hdrs = [
        "onlp_thermal_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlp_led_datasource",
    srcs = [
        "onlp_led_datasource.cc",
    ],
    hdrs = [
        "onlp_led_datasource.h",
    ],
    deps = [
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlp_thermal_datasource_test",
    srcs = [
        "onlp_thermal_datasource_test.cc",
    ],
    deps = [
        ":onlp_thermal_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "onlp_led_datasource_test",
    srcs = [
        "onlp_led_datasource_test.cc",
    ],
    deps = [
        ":onlp_led_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_phal_interface",
    hdrs = ["onlp_phal_interface.h"],
    deps = [
        ":onlp_event_handler",
        ":onlp_sfp_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:phal_backend_interface",
        "//stratum/hal/lib/phal:sfp_adapter",
        "//stratum/lib:macros",
    ],
)

stratum_cc_library(
    name = "onlp_phal",
    srcs = ["onlp_phal.cc"],
    hdrs = ["onlp_phal.h"],
    deps = [
        ":onlp_event_handler",
        ":onlp_phal_interface",
        ":onlp_sfp_configurator",
        ":onlp_sfp_datasource",
        ":onlp_switch_configurator",
        ":onlp_wrapper",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/lib:macros",
        "//stratum/lib/channel",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlp_phal_test",
    srcs = ["onlp_phal_test.cc"],
    deps = [
        ":onlp_event_handler_mock",
        ":onlp_phal",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "onlp_phal_mock",
    hdrs = ["onlp_phal_mock.h"],
    deps = [
        ":onlp_event_handler",
        ":onlp_phal_interface",
        ":onlp_sfp_datasource",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlp_switch_configurator",
    srcs = ["onlp_switch_configurator.cc"],
    hdrs = ["onlp_switch_configurator.h"],
    deps = [
        ":onlp_event_handler",
        ":onlp_fan_datasource",
        ":onlp_led_datasource",
        ":onlp_phal_interface",
        ":onlp_psu_datasource",
        ":onlp_sfp_configurator",
        ":onlp_sfp_datasource",
        ":onlp_thermal_datasource",
        ":onlp_wrapper",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "onlp_sfp_configurator",
    srcs = ["onlp_sfp_configurator.cc"],
    hdrs = ["onlp_sfp_configurator.h"],
    deps = [
        ":onlp_sfp_datasource",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:sfp_configurator",
        "//stratum/hal/lib/phal/onlp:onlp_event_handler",
        "@com_google_protobuf//:protobuf",
    ],
)

# TODO(max): fix and enable
# stratum_cc_binary(
#     name = "onlp_phal_cli",
#     srcs = ["onlp_phal_cli.cc"],
#     arches = EMBEDDED_ARCHES,
#     linkopts = [
#         "-lpthread",
#         "-ldl",
#         "-lrt",
#     ],
#     deps = [
#         ":onlp_phal",
#         ":onlp_switch_configurator",
#         ":onlp_sfp_configurator",
#         "//stratum/hal/lib/phal:attribute_database",
#         "//stratum/hal/lib/phal:attribute_database_interface",
#         "@com_github_google_glog//:glog",
#         "@com_google_absl//absl/strings",
#         "@com_google_absl//absl/time",
#         "//stratum/glue:init_google",
#         "//stratum/glue/status",
#         "//stratum/glue/status:status_macros",
#         "//stratum/glue/status:statusor",
#         "//stratum/lib:macros",
#         "@com_googlesource_code_re2//:re2",
#     ],
# )

stratum_cc_binary(
    name = "onlp_cli",
    srcs = ["onlp_cli.cc"],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":onlp_phal",
        ":onlp_sfp_configurator",
        ":onlp_switch_configurator",
        "//stratum/glue:init_google",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@local_onlp_bin//:onlp",
        "@local_onlp_bin//:onlp_platform",
        "@local_onlp_bin//:onlp_platform_defaults",
    ],
)

stratum_cc_test(
    name = "onlp_switch_configurator_test",
    srcs = ["onlp_switch_configurator_test.cc"],
    deps = [
        ":onlp_phal_mock",
        ":onlp_sfp_configurator",
        ":onlp_switch_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group_mock",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:sfp_configurator",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_test(
    name = "onlp_sfp_configurator_test",
    srcs = ["onlp_sfp_configurator_test.cc"],
    deps = [
        ":onlp_event_handler_mock",
        ":onlp_sfp_configurator",
        ":onlp_wrapper_mock",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:adapter",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)
