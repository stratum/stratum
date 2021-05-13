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
    name = "pipeline_pusher",
    srcs = ["pipeline_pusher.cc"],
    deps = [
        "//stratum/lib/p4runtime:entity_management",
        "//stratum/lib/p4runtime:p4runtime_session",
    ],
)
