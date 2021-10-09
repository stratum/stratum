# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "@com_github_stratum_stratum//bazel/rules:p4c_ir_defs.bzl",
    "P4C_BACKEND_IR_FILES",
)

licenses(["notice"])  # Apache v2

exports_files(["LICENSE"])

filegroup(
  name = "ir_extensions",
  srcs = P4C_BACKEND_IR_FILES,
  visibility = ["//visibility:public"],  # So p4c can compile these.
)
