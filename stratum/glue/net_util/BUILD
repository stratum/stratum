# Copyright 2018 Google LLC
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
    name = "bits",
    srcs = [
        "bits.cc",
    ],
    hdrs = [
        "bits.h",
    ],
    deps = [
        #"//testing/production_stub/public:gunit_prod",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "//stratum/glue:logging",
        "//stratum/glue:integral_types",
    ],
)

stratum_cc_test(
    name = "bits_test",
    size = "small",
    srcs = ["bits_test.cc"],
    deps = [
        ":bits",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "ipaddress",
    srcs = [
        "ipaddress.cc",
    ],
    hdrs = [
        "ipaddress.h",
    ],
    deps = [
        ":bits",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "ipaddress_test",
    size = "small",
    srcs = ["ipaddress_test.cc"],
    deps = [
        ":ipaddress",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

stratum_cc_library(
    name = "ports",
    srcs = [
        "ports.cc",
    ],
    hdrs = [
        "ports.h",
    ],
    deps = [
        "//stratum/glue:logging",
    ],
)

stratum_cc_test(
    name = "absl_test",
    srcs = ["absl_test.cc"],
    deps = [
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
