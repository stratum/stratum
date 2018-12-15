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
#FIXME remove when onlp_wrapper.h is stable
#        ":onlp_wrapper_fake",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:flat_hash_map",
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
    textual_hdrs = [
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlp_hdrs",
    ],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
        "@com_github_opennetworklinux//:onlp",
        #"@com_github_opennetworklinux//:onlp_headers",
        "@com_github_opennetworklinux//:onlp_platform_as7712",
    ],
)

stratum_cc_library(
    name = "onlp_wrapper_mock",
    testonly = 1,
    hdrs = [
        "onlp_wrapper_mock.h",
    ],
    arches = HOST_ARCHES,
    deps = [
        ":onlp_wrapper",
#FIXME remove when onlp_wrapper.h is stable
#        ":onlp_wrapper_fake",
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
        "//stratum/hal/lib/common:common_proto",
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
#FIXME remove when onlp_wrapper.h is stable
#        ":onlp_wrapper_fake",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/hal/lib/phal:system_interface",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
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
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_proto",
        "//stratum/hal/lib/phal:system_interface",
        "//stratum/hal/lib/phal:test_util",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        #       TODO: Fix dependency
        #        "//stratum/onlp:onlpsim",
    ],
)

stratum_cc_library(
    name = "onlpphal",
    srcs = [
        "onlpphal.cc",
    ],
    hdrs = [
        "onlpphal.h",
    ],
    deps = [
        ":onlp_event_handler",
        ":onlp_wrapper",
#FIXME remove when onlp_wrapper.h is stable
#        ":onlp_wrapper_fake",
        ":sfp_datasource",
        "//stratum/lib:macros",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/hal/lib/common:phal_interface",
        "@com_google_absl//absl/memory",
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