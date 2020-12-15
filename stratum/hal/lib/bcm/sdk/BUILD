# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# We use some strip_prefix magic to make the SDKs look the same externally.

load("@rules_cc//cc:defs.bzl", "cc_library")
load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

cc_library(
    name = "bcm_diag_shell",
    srcs = ["bcm_diag_shell.cc"],
    defines = ["NDEBUG"],
    deps = [
        ":sdk_build_flags",
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/bcm:bcm_diag_shell_interface",
        "//stratum/hal/lib/bcm:sdk_build_undef",
        "//stratum/lib:macros",
        "@com_github_broadcom_opennsa//:headers",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "bcm_sdk_wrapper",
    srcs = ["bcm_sdk_wrapper.cc"],
    hdrs = ["bcm_sdk_wrapper.h"],
    defines = ["NDEBUG"],
    include_prefix = "stratum/hal/lib/bcm",
    strip_include_prefix = "./",
    deps = [
        ":bcm_diag_shell",
        ":macros",
        ":sdk_build_flags",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:cleanup",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/gtl:stl_util",
        "//stratum/glue/net_util:ipaddress",
        "//stratum/glue/status",
        "//stratum/glue/status:status_macros",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/bcm:bcm_sdk_interface",
        "//stratum/hal/lib/bcm:constants",
        "//stratum/hal/lib/bcm:sdk_build_undef",
        "//stratum/hal/lib/common:constants",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "//stratum/lib:utils",
        "@com_github_broadcom_opennsa//:headers",
        "@com_github_broadcom_opennsa//:libsdk",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    include_prefix = "stratum/hal/lib/bcm",
    strip_include_prefix = "./",
    deps = [
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "@com_github_broadcom_opennsa//:headers",
        "@com_github_broadcom_opennsa//:libsdk",
    ],
)

cc_library(
    name = "sdk_build_flags",
    hdrs = ["sdk_build_flags.h"],
    includes = ["sdk"],
    strip_include_prefix = "./",
)
