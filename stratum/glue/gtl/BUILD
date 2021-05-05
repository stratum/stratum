# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "stl_util",
    hdrs = ["stl_util.h"],
)

stratum_cc_library(
    name = "map_util",
    hdrs = ["map_util.h"],
)

stratum_cc_test(
    name = "map_util_test",
    size = "small",
    srcs = ["map_util_test.cc"],
    deps = [
        ":map_util",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "source_location",
    hdrs = ["source_location.h"],
)

stratum_cc_library(
    name = "cleanup",
    hdrs = ["cleanup.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_test(
    name = "cleanup_test",
    srcs = ["cleanup_test.cc"],
    deps = [
        ":cleanup",
        "//stratum/glue:integral_types",
        "@com_google_googletest//:gtest_main",
    ],
)
