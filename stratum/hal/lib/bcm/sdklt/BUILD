# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# We use some strip_prefix magic to make the SDKs look the same externally.

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_library",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_library(
    name = "bcm_diag_shell",
    srcs = ["bcm_diag_shell.cc"],
    defines = ["NDEBUG"],
    deps = [
        "//stratum/glue:logging",
        "//stratum/glue/status",
        "//stratum/glue/status:statusor",
        "//stratum/hal/lib/bcm:bcm_diag_shell_interface",
        "//stratum/hal/lib/bcm:sdk_build_undef",
        "//stratum/lib:macros",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

stratum_cc_library(
    name = "bcm_sdk_wrapper",
    srcs = ["bcm_sdk_wrapper.cc"],
    hdrs = ["bcm_sdk_wrapper.h"],
    copts = ["-Wno-thread-safety-analysis"],  # TODO(max)
    defines = ["NDEBUG"],
    include_prefix = "stratum/hal/lib/bcm/",
    strip_include_prefix = "./",
    deps = [
        ":bcm_diag_shell",
        ":macros",
        "//stratum/glue:logging",
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
        "@com_github_google_glog//:glog",
        "@com_github_jbeder_yaml_cpp//:yaml-cpp",
        "@com_github_opennetworkinglab_sdklt//:bcm_headers",
        "@com_github_opennetworkinglab_sdklt//:bcm_sdklt",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

stratum_cc_library(
    name = "macros",
    hdrs = ["macros.h"],
    include_prefix = "stratum/hal/lib/bcm",
    strip_include_prefix = "./",
    deps = [
        "//stratum/glue/status",
        "//stratum/lib:macros",
        "//stratum/public/lib:error",
        "@com_github_opennetworkinglab_sdklt//:bcm_headers",
        "@com_github_opennetworkinglab_sdklt//:bcm_sdklt",
    ],
)
