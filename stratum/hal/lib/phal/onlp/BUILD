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
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_proto",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:flat_hash_map",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_test(
    name = "onlp_event_handler_test",
    srcs = ["onlp_event_handler_test.cc"],
    deps = [
        ":onlp_event_handler",
        ":onlp_wrapper_mock",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
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
        "@com_google_googletest//:gtest",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
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
