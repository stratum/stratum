# stratum/hal/lib/yang/BUILD

# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "parse_tree",
    hdrs = ["yang_parse_tree.h"],
)

stratum_cc_library(
    name = "parse_tree_paths",
    hdrs = ["yang_parse_tree_paths.h"],
)

stratum_cc_library(
    name = "parse_tree_mock",
    hdrs = ["yang_parse_tree_mock.h"],
)

stratum_cc_library(
    name = "yang_parse_tree",
    srcs = [
        "yang_parse_tree.cc",
    ],
    hdrs = [
        "yang_parse_tree.h",
        "yang_parse_tree_paths.h",
    ],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:gnmi_events",
        "//stratum/hal/lib/common:gnmi_publisher_hdr",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "yang_parse_tree_mock",
    testonly = 1,
    hdrs = ["yang_parse_tree_mock.h"],
    deps = [
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "yang_parse_tree_paths",
    srcs = [
        "yang_add_subtree_interface.cc",
        "yang_parse_tree_chassis.cc",
        "yang_parse_tree_component.cc",
        "yang_parse_tree_helpers.cc",
        "yang_parse_tree_interface.cc",
        "yang_parse_tree_node.cc",
        "yang_parse_tree_optical.cc",
        "yang_parse_tree_paths.cc",
        "yang_parse_tree_singleton.cc",
        "yang_parse_tree_system.cc",
    ],
    hdrs = [
        "yang_parse_tree.h",
        "yang_parse_tree_component.h",
        "yang_parse_tree_helpers.h",
        "yang_parse_tree_interface.h",
        "yang_parse_tree_paths.h",
    ],
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/hal/lib/common:gnmi_events",
        "//stratum/hal/lib/common:gnmi_publisher_hdr",
        "//stratum/hal/lib/common:openconfig_converter",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/lib:timer_daemon",
        "//stratum/lib:utils",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_github_openconfig_hercules//:openconfig_cc_proto",
    ],
)

stratum_cc_test(
    name = "yang_parse_tree_test",
    srcs = [
        "yang_parse_tree_test.cc",
    ],
    deps = [
        ":yang_parse_tree",
        ":yang_parse_tree_mock",
        ":yang_parse_tree_paths",
        "//stratum/glue/status:status_test_util",
        "//stratum/hal/lib/common:gnmi_publisher",
        "//stratum/hal/lib/common:subscribe_reader_writer_mock",
        "//stratum/hal/lib/common:switch_mock",
        "//stratum/hal/lib/common:test_main",
        "//stratum/hal/lib/common:testdata",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_mock",
        "//stratum/lib:constants",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_github_openconfig_hercules//:openconfig_cc_proto",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)
