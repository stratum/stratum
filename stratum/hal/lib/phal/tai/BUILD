#
# Copyright 2020-present Open Networking Foundation
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
    default_visibility = STRATUM_INTERNAL,
)

DEFAULT_COPTS = [
    "-Werror=thread-safety",
]

stratum_cc_library(
    name = "tai_wrapper",
    srcs = ["tai_wrapper.cc"],
    hdrs = ["tai_wrapper.h"],
    copts = DEFAULT_COPTS,
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:macros",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "tai_phal",
    srcs = ["tai_phal.cc"],
    hdrs = ["tai_phal.h"],
    deps = [
        ":tai_wrapper",
        ":tai_switch_configurator",
        ":tai_optics_datasource",
        "//stratum/lib/channel:channel",
        "//stratum/lib:macros",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:phal_backend_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

# stratum_cc_test(
#     name = "onlp_event_handler_test",
#     srcs = ["onlp_event_handler_test.cc"],
#     deps = [
#         ":onlp_event_handler",
#         ":onlp_event_handler_mock",
#         ":onlp_wrapper_mock",
#         "@com_google_googletest//:gtest_main",
#         "@com_google_absl//absl/synchronization",
#         "@com_google_absl//absl/time",
#         "//stratum/glue/status",
#         "//stratum/lib:macros",
#         "//stratum/lib/test_utils:matchers",
#         "//stratum/glue/status:status_test_util",
#     ],
# )

stratum_cc_library(
    name = "tai_switch_configurator",
    srcs = ["tai_switch_configurator.cc"],
    hdrs = ["tai_switch_configurator.h"],
    copts = DEFAULT_COPTS,
    deps = [
        # ":onlp_phal_interface",
        # ":onlp_fan_datasource",
        # ":onlp_led_datasource",
        # ":onlp_event_handler",
        # ":onlp_wrapper",
        # ":onlp_psu_datasource",
        # ":onlp_sfp_configurator",
        # ":onlp_sfp_datasource",
        # ":onlp_thermal_datasource",
        ":tai_wrapper",
        ":tai_optics_datasource",
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
    name = "tai_optics_datasource",
    srcs = ["tai_optics_datasource.cc"],
    hdrs = ["tai_optics_datasource.h"],
    copts = DEFAULT_COPTS,
    deps = [
        ":tai_wrapper",
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
