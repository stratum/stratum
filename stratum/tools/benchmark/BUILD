# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_binary(
    name = "stratum_benchmark",
    srcs = ["stratum_benchmark.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/p4:p4_info_manager",
        "//stratum/hal/lib/p4:utils",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/p4runtime:entity_management",
        "//stratum/lib/p4runtime:p4runtime_session",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_grpc_grpc//:grpc++",
        "@com_github_p4lang_p4runtime//:p4info_cc_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/random",
        "@com_google_protobuf//:protobuf",
    ],
)

# TODO(max): fix stratum_cc_test with missing args
cc_test(
    name = "fabric_test",
    srcs = ["fabric_test.cc"],
    linkstatic = True,
    tags = ["manual"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib:test_main",
        "//stratum/lib:utils",
        "//stratum/lib/p4runtime:p4runtime_fixture",
        "//stratum/lib/p4runtime:p4runtime_session",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_gflags_gflags//:gflags",
        "@com_google_googletest//:gtest",
    ],
)
