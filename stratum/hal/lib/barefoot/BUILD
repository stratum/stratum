# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
    "stratum_cc_test",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "bf_switch",
    srcs = ["bf_switch.cc"],
    hdrs = ["bf_switch.h"],
    deps = [
        ":bf_cc_proto",
        ":bf_chassis_manager",
        ":bf_pd_interface",
        ":bf_pipeline_utils",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/pi:pi_node_bf",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "bf_chassis_manager",
    srcs = ["bf_chassis_manager.cc"],
    hdrs = ["bf_chassis_manager.h"],
    deps = [
        ":bf_pal_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:optional",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_test(
    name = "bf_chassis_manager_test",
    srcs = ["bf_chassis_manager_test.cc"],
    deps = [
        ":bf_chassis_manager",
        ":bf_pal_mock",
        ":test_main",
        "//stratum/glue:integral_types",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status",
        "//stratum/glue/status:status_test_util",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/hal/lib/common:constants",
        "//stratum/hal/lib/common:phal_mock",
        "//stratum/hal/lib/common:utils",
        "//stratum/hal/lib/common:writer_interface",
        "//stratum/lib:utils",
        "//stratum/lib/test_utils:matchers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_pal_interface",
    hdrs = ["bf_pal_interface.h"],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
    ],
)

stratum_cc_library(
    name = "bf_pal_mock",
    testonly = 1,
    hdrs = ["bf_pal_mock.h"],
    deps = [
        ":bf_pal_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_pal_wrapper",
    srcs = ["bf_pal_wrapper.cc"],
    hdrs = ["bf_pal_wrapper.h"],
    deps = [
        ":bf_pal_interface",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/common:common_cc_proto",
        "//stratum/lib:constants",
        "//stratum/lib/channel",
        "@com_google_absl//absl/base:core_headers",
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "bf_pd_interface",
    hdrs = ["bf_pd_interface.h"],
    deps = [
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
    ],
)

stratum_cc_library(
    name = "bf_pd_mock",
    testonly = 1,
    hdrs = ["bf_pd_mock.h"],
    deps = [
        ":bf_pd_interface",
        "@com_google_googletest//:gtest",
    ],
)

stratum_cc_library(
    name = "bf_pd_wrapper",
    srcs = ["bf_pd_wrapper.cc"],
    hdrs = ["bf_pd_wrapper.h"],
    deps = [
        ":bf_pd_interface",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/lib:macros",
        "@com_google_absl//absl/synchronization",
        "@local_barefoot_bin//:bfsde",
    ],
)

stratum_cc_library(
    name = "bf_pipeline_utils",
    srcs = ["bf_pipeline_utils.cc"],
    hdrs = ["bf_pipeline_utils.h"],
    deps = [
        ":bf_cc_proto",
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:cleanup",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "//stratum/public/lib:error",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_libarchive_libarchive//:libarchive",
        "@com_github_nlohmann_json//:json",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/strings",
    ],
)

stratum_cc_test(
    name = "bf_pipeline_utils_test",
    srcs = ["bf_pipeline_utils_test.cc"],
    deps = [
        ":bf_pipeline_utils",
        ":test_main",
        "//stratum/glue/status:status_test_util",
        "//stratum/lib:utils",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_p4lang_p4runtime//:p4runtime_cc_proto",
        "@com_google_absl//absl/strings",
    ],
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

proto_library(
    name = "bf_proto",
    srcs = ["bf.proto"],
    deps = [
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)

cc_proto_library(
    name = "bf_cc_proto",
    deps = [":bf_proto"],
)
