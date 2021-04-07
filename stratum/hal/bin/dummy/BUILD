# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_python//python:defs.bzl", "py_binary")
load("//bazel:rules.bzl", "HOST_ARCHES", "stratum_cc_binary")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

# TODO(Yi) Move to CC
py_binary(
    name = "cli",
    srcs = ["cli.py"],
    deps = [
        "//stratum/hal/lib/common:common_py_proto",
        "//stratum/hal/lib/dummy:dummy_test_py_grpc",
    ],
)

py_binary(
    name = "port_counter_sim",
    srcs = ["port_counter_sim.py"],
    deps = [
        "//stratum/hal/bin/dummy:cli",
        "//stratum/hal/lib/common:common_py_proto",
        "//stratum/hal/lib/dummy:dummy_test_py_grpc",
    ],
)

stratum_cc_binary(
    name = "stratum_dummy",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/dummy:dummy_box",
        "//stratum/hal/lib/dummy:dummy_switch",
        "//stratum/hal/lib/phal",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
        "@com_github_google_glog//:glog",
    ],
)
