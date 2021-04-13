# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

# This package contains testing libraries for Hercules.

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "matchers",
    testonly = 1,
    hdrs = ["matchers.h"],
    deps = [
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "p4_proto_builders",
    testonly = 1,
    srcs = ["p4_proto_builders.cc"],
    hdrs = ["p4_proto_builders.h"],
    deps = [
        "//stratum/hal/lib/p4:p4_control_cc_proto",
        "//stratum/public/proto:p4_annotation_cc_proto",
        "//stratum/public/proto:p4_table_defs_cc_proto",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "p4_proto_builders_test",
    srcs = ["p4_proto_builders_test.cc"],
    deps = [
        ":matchers",
        ":p4_proto_builders",
        "//stratum/lib:test_main",
        "//stratum/lib:utils",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)
