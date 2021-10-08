# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_tar")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

pkg_tar(
    name = "stratum_legal",
    srcs = [
        "LICENSE",
        "NOTICE",
    ],
    mode = "0644",
    package_dir = "/usr/share/doc/stratum/",
)
