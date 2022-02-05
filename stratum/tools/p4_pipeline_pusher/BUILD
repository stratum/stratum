# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_binary(
    name = "p4_pipeline_pusher",
    srcs = ["p4_pipeline_pusher.cc"],
    deps = [
        "//stratum/lib:utils",
        "//stratum/lib/p4runtime:p4runtime_session",
        "//stratum/lib/security:credentials_manager",
    ],
)
