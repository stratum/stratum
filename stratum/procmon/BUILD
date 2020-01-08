#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

licenses(["notice"])  # Apache v2

load("//bazel/rules:library_rule.bzl", "stratum_cc_library")
load("//bazel/rules:binary_rule.bzl", "stratum_cc_binary")
load("//bazel/rules:test_rule.bzl", "stratum_cc_test")
load("//bazel/rules:package_rule.bzl", "stratum_package")
load("//bazel:defs.bzl", "STRATUM_INTERNAL")

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

proto_library(
    name = "procmon_proto",
    srcs = ["procmon.proto"],
)

cc_proto_library(
    name = "procmon_cc_proto",
    deps = [":procmon_proto"]
)

cc_grpc_library(
    name = "procmon_cc_grpc",
    srcs = [":procmon_proto"],
    deps = [":procmon_cc_proto"],
    grpc_only = True,
)

stratum_cc_library(
    name = "procmon",
    srcs = ["procmon.cc"],
    hdrs = ["procmon.h"],
    deps = [
        ":procmon_cc_proto",
        ":procmon_cc_grpc",
        "@com_github_google_glog//:glog",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status",
    ],
)

stratum_cc_library(
    name = "procmon_service_impl",
    srcs = ["procmon_service_impl.cc"],
    hdrs = ["procmon_service_impl.h"],
    deps = [
        ":procmon_cc_proto",
        ":procmon_cc_grpc",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

stratum_cc_test(
    name = "procmon_test",
    srcs = ["procmon_test.cc"],
    deps = [
        ":procmon",
        ":procmon_cc_proto",
        ":procmon_cc_grpc",
        "@com_github_google_glog//:glog",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/glue/status:status_test_util",
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_binary(
    name = "procmon_main",
    srcs = ["procmon_main.cc"],
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
    ],
    deps = [
        ":procmon",
        ":procmon_cc_proto",
        ":procmon_cc_grpc",
        ":procmon_service_impl",
        "@com_github_google_glog//:glog",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_grpc_grpc//:grpc++",
        #"//sandblaze/prebuilt/openssl",
        "//stratum/glue/status:status",
    ],
)

stratum_package(
    name = "procmon_main_pkg",
    bins = [
        ":procmon_main",
    ],
)
