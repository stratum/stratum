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
load("//bazel/rules:test_rule.bzl", "stratum_cc_test")
load("//bazel:defs.bzl", "HOST_ARCHES", "STRATUM_INTERNAL")

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# The third-party protoc is not producing the same namespaces as the google3
# version. We therefore copy the protofiles needed using genrules and do
# manual modifications.
#FIXME(boc) google only
'''
genrule(
    name = "aaa_membership_proto_srcs",
    srcs = [
        "//platforms/networking/sandcastle/stack/lib/ipc:aaa_membership_proto_srcs",
    ],
    outs = ["aaa_membership.proto"],
    cmd = "sed '/^option.*api_version.*$$/d' $(SRCS) > $(OUTS)",
    visibility = ["//visibility:private"],
)

genrule(
    name = "authorization_policy_proto_srcs",
    srcs = [
        "//platforms/networking/sandcastle/public/proto:authorization_policy_proto_srcs",
    ],
    outs = ["authorization_policy.proto"],
    cmd = "sed '/^option.*api_version.*$$/d' $(SRCS) > $(OUTS)",
    visibility = ["//visibility:private"],
)

proto_library(
    name = "aaa_membership_proto",
    srcs = ["aaa_membership.proto"],
)

cc_proto_library(
    name = "aaa_membership_cc_proto",
    deps = [":aaa_membership_proto"],
)

proto_library(
    name = "authorization_policy_proto",
    srcs = ["authorization_policy.proto"],
)

cc_proto_library(
    name = "authorization_policy_cc_proto",
    deps = [":authorization_policy_proto"],
)
'''

stratum_cc_library(
    name = "auth_policy_checker",
    srcs = ["auth_policy_checker.cc"],
    hdrs = ["auth_policy_checker.h"],
    deps = [
        #FIXME(boc)
        #":aaa_membership_cc_proto",
        #":authorization_policy_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/proto:error_cc_proto",
        "//stratum/glue/gtl:map_util",
    ],
)

stratum_cc_library(
    name = "auth_policy_checker_mock",
    testonly = 1,
    hdrs = ["auth_policy_checker_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":auth_policy_checker",
        "@com_google_googletest//:gtest",
    ],
)

''' FIXME(boc) google only
stratum_cc_test(
    name = "auth_policy_checker_test",
    srcs = [
        "auth_policy_checker_test.cc",
    ],
    deps = [
        ":auth_policy_checker",
        ":test_main",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/proto:error_proto",
    ],
)
'''

stratum_cc_library(
    name = "credentials_manager",
    srcs = ["credentials_manager.cc"],
    hdrs = ["credentials_manager.h"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_github_grpc_grpc//:grpc++",
        "//stratum/glue:logging",
        "//stratum/glue/status",
    ],
)

stratum_cc_library(
    name = "credentials_manager_mock",
    testonly = 1,
    hdrs = ["credentials_manager_mock.h"],
    arches = HOST_ARCHES,
    deps = [
        ":credentials_manager",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
    ],
)
