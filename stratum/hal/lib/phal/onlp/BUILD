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
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
    "EMBEDDED_ARCHES",
    "HOST_ARCHES",
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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",

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
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "//stratum/glue/status:status_test_util",
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
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@local_onlp_bin//:onlp",
        "@local_onlp_bin//:onlp_headers",
        "@local_onlp_bin//:onlp_platform",
        "@local_onlp_bin//:onlp_platform_defaults",
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
        "@com_google_googletest//:gtest",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
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
    name = "sfp_datasource",
    srcs = [
        "sfp_datasource.cc",
    ],
    hdrs = [
        "sfp_datasource.h",
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
    name = "sfp_datasource_test",
    srcs = [
        "sfp_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":sfp_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "fan_datasource",
    srcs = [
        "fan_datasource.cc",
    ],
    hdrs = [
        "fan_datasource.h",
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
    name = "fan_datasource_test",
    srcs = [
        "fan_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":fan_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "psu_datasource",
    srcs = [
        "psu_datasource.cc",
    ],
    hdrs = [
        "psu_datasource.h",
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
    name = "psu_datasource_test",
    srcs = [
        "psu_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":psu_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "thermal_datasource",
    srcs = [
        "thermal_datasource.cc",
    ],
    hdrs = [
        "thermal_datasource.h",
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
    name = "led_datasource",
    srcs = [
        "led_datasource.cc",
    ],
    hdrs = [
        "led_datasource.h",
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
    name = "thermal_datasource_test",
    srcs = [
        "thermal_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":thermal_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "led_datasource_test",
    srcs = [
        "led_datasource_test.cc",
    ],
    deps = [
        ":onlp_wrapper_mock",
        ":led_datasource",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlpphal_header",
    hdrs = [
        "onlpphal.h",
    ],
    deps = [
        ":onlp_event_handler",
        ":sfp_configurator",
        ":sfp_datasource",
        "//stratum/lib:macros",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:sfp_adapter",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlpphal",
    srcs = [
        "onlpphal.cc",
    ],
    deps = [
        ":onlpphal_header",
        ":onlp_event_handler",
        ":onlp_wrapper",
        ":switch_configurator",
        ":sfp_configurator",
        ":sfp_datasource",
        "//stratum/lib:macros",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "onlpphal_mock",
    hdrs = ["onlpphal_mock.h"],
    deps = [
        ":onlpphal_header",
        ":onlp_event_handler",
        ":onlp_wrapper_mock",
        ":sfp_datasource",
        "//stratum/lib:macros",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "switch_configurator",
    srcs = ["switch_configurator.cc"],
    hdrs = ["switch_configurator.h"],
    deps = [
        ":fan_datasource",
        ":led_datasource",
        ":onlp_event_handler",
        ":onlp_wrapper",
        ":psu_datasource",
        ":sfp_configurator",
        ":sfp_datasource",
        ":thermal_datasource",
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
    ],
)

stratum_cc_library(
    name = "sfp_configurator",
    srcs = ["sfp_configurator.cc"],
    hdrs = ["sfp_configurator.h"],
    deps = [
        ":sfp_datasource",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:sfp_configurator",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/common:phal_interface",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_binary(
    name = "onlpphal_cli",
    srcs = ["onlpphal_cli.cc"],
    arches = EMBEDDED_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":onlpphal",
        ":switch_configurator",
        ":sfp_configurator",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_database_interface",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "//stratum/glue:init_google",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_googlesource_code_re2//:re2",
    ],
)

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
        ":onlpphal",
        ":switch_configurator",
        ":sfp_configurator",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "//stratum/glue:init_google",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
    ],
)

stratum_cc_test(
    name = "switch_configurator_test",
    srcs = ["switch_configurator_test.cc"],
    deps = [
        ":onlpphal_mock",
        ":switch_configurator",
        ":sfp_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group_mock",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:sfp_configurator",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "sfp_configurator_test",
    srcs = ["sfp_configurator_test.cc"],
    deps = [
        ":onlpphal_mock",
        ":switch_configurator",
        ":sfp_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:dummy_threadpool",
        "//stratum/hal/lib/phal:db_cc_proto",
        "//stratum/hal/lib/phal:adapter",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_test(
    name = "onlpphal_test",
    srcs = ["onlpphal_test.cc"],
    deps = [
        ":onlpphal",
        ":onlp_wrapper_mock",
        "@com_google_googletest//:gtest_main",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
    ],
)
