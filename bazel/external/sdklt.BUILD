# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

# trick to export headers in a convenient way
cc_library(
    name = "bcm_headers",
    hdrs = glob(["include/sdklt/**/*.h"]),
    includes = ["include/sdklt"],
)

cc_import(
    name = "bcm_sdklt",
    hdrs = [],  # see cc_library rule above
    static_library = "lib/libsdklt.a",
)

filegroup(
    name = "kernel_modules",
    srcs = [
        "linux_ngbde.ko",
        "linux_ngknet.ko",
    ],
)

filegroup(
    name = "sdklt_cli",
    srcs = [
        "sdklt",
    ],
)
