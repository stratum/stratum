# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "json",
    hdrs = [
        "single_include/nlohmann/json.hpp",
    ],
    strip_include_prefix = "single_include",
)
