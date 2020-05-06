# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

load("//stratum/testing/cdlang:cdlang.bzl", "cc_cdlang_library")

cc_cdlang_library(
    name = "scenarios",
    testonly = True,
    srcs = glob(["*.cdlang"]),
    libs = [
        "lib.cc.tmpl",
    ],
    template = "scenarios.cc.tmpl",
    deps = [
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_openconfig_gnmi_proto//:gnmi_cc_grpc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status",
    ],
)

cc_test(
    name = "scenarios_test",
    testonly = 1,
    local = 1,
    deps = [
        ":scenarios",
        ":test_main",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)
