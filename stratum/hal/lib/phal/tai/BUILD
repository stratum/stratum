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
)

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "tai_interface",
    hdrs = ["tai_interface.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "taish_wrapper",
    srcs = ["taish_wrapper.cc"],
    hdrs = ["taish_wrapper.h"],
    deps = [
        ":tai_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_telecominfraproject_oopt_tai_taish//:taish_cc_grpc",
        "@com_github_telecominfraproject_oopt_tai_taish//:taish_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "tai_phal",
    srcs = ["tai_phal.cc"],
    hdrs = ["tai_phal.h"],
    deps = [
        ":tai_switch_configurator",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:optics_adapter",
        "//stratum/hal/lib/phal:phal_backend_interface",
        "//stratum/lib:macros",
        "//stratum/lib/channel",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "tai_switch_configurator",
    srcs = ["tai_switch_configurator.cc"],
    hdrs = ["tai_switch_configurator.h"],
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)
