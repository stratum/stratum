# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

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

stratum_cc_test(
    name = "auth_policy_checker_test",
    srcs = [
        "auth_policy_checker_test.cc",
    ],
    deps = [
        ":auth_policy_checker",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/proto:error_cc_proto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "credentials_manager",
    srcs = ["credentials_manager.cc"],
    hdrs = ["credentials_manager.h"],
    deps = [
        "//stratum/glue:logging",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_google_glog//:glog",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/memory",
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

stratum_cc_test(
    name = "credentials_manager_test",
    srcs = ["credentials_manager_test.cc"],
    deps = [
        ":certificate",
        ":credentials_manager",
        ":test_cc_grpc",
        ":test_main",
        "//stratum/glue:logging",
        "//stratum/glue/net_util:ports",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

proto_library(
    name = "test_proto",
    testonly = True,
    srcs = ["test.proto"],
)

cc_proto_library(
    name = "test_cc_proto",
    testonly = True,
    deps = [":test_proto"],
)

cc_grpc_library(
    name = "test_cc_grpc",
    testonly = True,
    srcs = [":test_proto"],
    grpc_only = True,
    deps = [":test_cc_proto"],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "certificate",
    srcs = ["certificate.cc"],
    hdrs = ["certificate.h"],
    deps = [
        "//stratum/lib:utils",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
        "@com_google_absl//absl/time",
    ],
)

stratum_cc_test(
    name = "certificate_test",
    srcs = ["certificate_test.cc"],
    deps = [
        ":certificate",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)
