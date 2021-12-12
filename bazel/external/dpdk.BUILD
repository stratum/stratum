# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "dpdk",
    srcs = glob([
        "dpdk-install/lib/*.a",
    ]),
    hdrs = glob([
        "dpdk-install/include/*.h",
        "dpdk-install/include/generic/*.h",
    ]),
    copts = [
        "-Wno-error=implicit-fallthrough",
    ],
    linkopts = [
        "-lnuma",
        "-ldl",
    ],
    strip_include_prefix = "dpdk-install/include",
)
