# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "libarchive",
    srcs = [
        "local-install/lib/libarchive.a",
    ],
    hdrs = [
        "local-install/include/archive.h",
        "local-install/include/archive_entry.h",
    ],
    include_prefix = "libarchive",
    linkopts = [
        "-lbz2",
    ],
    strip_include_prefix = "local-install/include",
)
