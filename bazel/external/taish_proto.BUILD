# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])  # Apache v2

package(
    default_visibility = [ "//visibility:public" ],
)

proto_library(
  name = "taish_proto",
  srcs = ["taish/taish.proto"],
)

cc_proto_library(
  name = "taish_cc_proto",
  deps = [":taish_proto"],
)

cc_grpc_library(
  name = "taish_cc_grpc",
  srcs = [":taish_proto"],
  deps = [":taish_cc_proto"],
  grpc_only = True,
)
