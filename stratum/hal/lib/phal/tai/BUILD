# Copyright 2020-present Open Networking Foundation
# Copyright 2020 PLVision
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

stratum_cc_library(
    name = "tai_phal",
    srcs = ["tai_phal.cc"],
    hdrs = ["tai_phal.h"],
    deps = [
        ":tai_switch_configurator",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:statusor",
        "//stratum/glue/status",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:optics_adapter",
        "//stratum/hal/lib/phal:phal_backend_interface",
        "//stratum/lib:macros",
        "//stratum/lib/channel:channel",
    ],
)

stratum_cc_library(
    name = "tai_switch_configurator",
    srcs = ["tai_switch_configurator.cc"],
    hdrs = ["tai_switch_configurator.h"],
    deps = [
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
    deps = [
        "//stratum/hal/lib/phal/tai/tai_wrapper:tai",
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
