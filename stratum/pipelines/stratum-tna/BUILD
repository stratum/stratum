# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Use sh_binary?

load("@rules_cc//cc:defs.bzl", "cc_test")
load("//stratum/hal/bin/barefoot:bf_pipeline_builder.bzl", "packed_bf_pipeline")
load("//bazel/rules:p4c_build_defs.bzl", "p4_tna_compile")

p4_tna_compile(
    name = "stratum_tna",
    src = "stratum_tna.p4",
)

# packed_bf_pipeline(
#     name = "stratum_tna_device_config",
#     src = ["stratum_tna.tar"],
# )

cc_test(
    name = "stratum_tna_test",
    srcs = ["stratum_tna_test.cc"],
    linkstatic = True,
    tags = ["manual"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:macros",
        "//stratum/lib:test_main",
        "//stratum/lib:utils",
        "//stratum/lib/p4runtime:entity_management",
        "//stratum/lib/p4runtime:p4runtime_fixture",
        "//stratum/lib/p4runtime:p4runtime_session",
        "//stratum/lib/test_utils:matchers",
        "//stratum/public/lib:error",
        "@com_github_gflags_gflags//:gflags",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest",
    ],
)
