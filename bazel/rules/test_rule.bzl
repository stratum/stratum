# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_test")
load(
    "//bazel:defs.bzl",
    "STRATUM_DEFAULT_LINKOPTS",
    "STRATUM_TEST_COPTS",
)

def stratum_cc_test(
        name,
        deps = None,
        srcs = None,
        data = None,
        copts = [],
        defines = None,
        linkopts = [],
        size = "small",
        visibility = None):
    cc_test(
        name = name,
        deps = deps,
        srcs = srcs,
        data = data,
        copts = STRATUM_TEST_COPTS + copts,
        defines = defines,
        linkopts = STRATUM_DEFAULT_LINKOPTS + linkopts,
        size = size,
        visibility = visibility,
    )
