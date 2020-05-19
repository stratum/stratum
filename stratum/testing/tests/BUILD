# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load(
    "//bazel:rules.bzl",
    "HOST_ARCHES",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
)

licenses(["notice"])  # Apache v2

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
    # TODO(b/76020938); doesn't work with header modules yet.
    features = ["-use_header_modules"],
)

cc_library(
    name = "bcm_sim_test_fixture",
    testonly = 1,
    srcs = ["bcm_sim_test_fixture.cc"],
    hdrs = [
        "bcm_sim_test_fixture.h",
    ],
    data = [
        "//stratum/hal/bin/bcm/sim:bcm_pcid_sim",
        "//stratum/hal/config:bcm_sim_config_files",
        "//stratum/testing/protos:bcm_sim_test_protos",
    ],
    deps = [
        "//devtools/build/runtime:get_runfiles_dir",
        "//stratum/hal/lib/bcm:bcm_acl_manager",
        "//stratum/hal/lib/bcm:bcm_chassis_manager",
        "//stratum/hal/lib/bcm:bcm_l2_manager",
        "//stratum/hal/lib/bcm:bcm_l3_manager",
        "//stratum/hal/lib/bcm:bcm_node",
        "//stratum/hal/lib/bcm:bcm_packetio_manager",
        "//stratum/hal/lib/bcm:bcm_sdk_sim",
        "//stratum/hal/lib/bcm:bcm_serdes_db_manager",
        "//stratum/hal/lib/bcm:bcm_switch",
        "//stratum/hal/lib/bcm:bcm_table_manager",
        "//stratum/hal/lib/bcm:bcm_tunnel_manager",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_github_google_glog//:glog",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "test_main",
    testonly = 1,
    srcs = ["test_main.cc"],
    arches = HOST_ARCHES,
    deps = [
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "bcm_sim_test",
    timeout = "long",
    srcs = ["bcm_sim_test.cc"],
    data = [
        "//stratum/testing/protos:bcm_sim_test_protos",
    ],
    local = 1,
    deps = [
        ":bcm_sim_test_fixture",
        ":test_main",
        "//stratum/glue:logging",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:macros",
        "//stratum/lib/test_utils:matchers",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_grpc",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)
