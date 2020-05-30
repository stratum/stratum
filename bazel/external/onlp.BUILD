# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "onlp_headers",
    hdrs = glob([
        "onlp-bin/include/**/*.h",
        "onlp-bin/include/**/*.x",
    ]),
    includes = ["onlp-bin/include"],
)

cc_library(
    name = "onlp",
    srcs = [
        "onlp-bin/lib/libonlp.so",
        "onlp-bin/lib/libonlp.so.1",
    ],
    hdrs = [],  # see cc_library rule above
)

cc_library(
    name = "onlp_platform",
    srcs = [
        "onlp-bin/lib/libonlp-platform.so",
        "onlp-bin/lib/libonlp-platform.so.1",
    ],
    hdrs = [],  # see cc_library rule above
)

cc_library(
    name = "onlp_platform_defaults",
    srcs = [
        "onlp-bin/lib/libonlp-platform-defaults.so",
        "onlp-bin/lib/libonlp-platform-defaults.so.1",
    ],
    hdrs = [],  # see cc_library rule above
)
