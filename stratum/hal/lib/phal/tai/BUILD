# Copyright 2020-present Open Networking Foundation
# Copyright 2020 PLVision
# SPDX-License-Identifier: Apache-2.0

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
        "@com_google_protobuf//:protobuf",
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/phal:attribute_database",
        "//stratum/hal/lib/phal:attribute_group",
        "//stratum/hal/lib/phal:phal_cc_proto",
    ],
)
