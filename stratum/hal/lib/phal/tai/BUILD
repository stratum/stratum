#
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

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

config_setting(
    name = "with_grpc_tai",
    define_values = {
        "phal_with_grpc_tai": "true",
    },
)

tai_defines = select({
    ":with_grpc_tai": ["WITH_GRPC_TAI"],
    "//conditions:default": [],
})

tai_deps = select({
    ":with_grpc_tai": [
        ":taish_client",
        ":types_converter",
    ],
    "//conditions:default": [
        "//stratum/hal/lib/phal/tai/tai_wrapper:tai",
    ],
})

proto_library(
    name = "taish_proto",
    srcs = ["taish.proto"],
)

cc_proto_library(
    name = "taish_cc_proto",
    deps = [":taish_proto"]
)

cc_grpc_library(
    name = "taish_proto_grpc",
    srcs = [":taish_proto"],
    deps = [":taish_cc_proto"],
    grpc_only = True,
)

stratum_cc_library(
    name = "tai_phal",
    srcs = ["tai_phal.cc"],
    hdrs = ["tai_phal.h"],
    deps = [
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
        "//stratum/hal/lib/phal:optics_adapter",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
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
    name = "taish_client",
    srcs = ["taish_client.cc"],
    hdrs = ["taish_client.h"],
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        ":taish_proto_grpc",
        "//stratum/lib:macros",
        "//stratum/glue:integral_types",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_library(
    name = "types_converter",
    srcs = ["types_converter.cc"],
    hdrs = ["types_converter.h"],
    deps = ["//stratum/lib:utils"],
)

stratum_cc_library(
    name = "tai_optics_datasource",
    srcs = ["tai_optics_datasource.cc"],
    hdrs = ["tai_optics_datasource.h"],
    defines = tai_defines,
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/phal:datasource",
        "//stratum/hal/lib/phal:phal_cc_proto",
        "//stratum/lib:macros",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ] + tai_deps,
)
