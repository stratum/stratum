# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
)

licenses(["notice"])  # Apache v2

package(
    default_visibility = STRATUM_INTERNAL,
)

stratum_cc_binary(
    name = "dpdk_hello",
    srcs = ["dpdk_hello.cc"],
    copts = [
        "-Wno-error=implicit-fallthrough",
    ],
    linkopts = [
        "-lnuma",
        "-ldl",
    ],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/gtl:map_util",
        "//stratum/glue/status:status_macros",
        "//stratum/hal/lib/common:phal_interface",
        "//stratum/hal/lib/common:switch_interface",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@local_dpdk_bin//:dpdk",
    ],
)

stratum_cc_binary(
    name = "dpdk_pipeline",
    srcs = [
        "cli.c",
        "cli.h",
        "conn.c",
        "conn.h",
        "dpdk_pipeline.c",
        "obj.c",
        "obj.h",
        "thread.c",
        "thread.h",
    ],
    copts = [
        "-Wno-deprecated-declarations",
        "-Wno-error=implicit-fallthrough",
        "-Wno-error=unreachable-code-return",
        "-Wno-error=unreachable-code",
        "-Wno-implicit-fallthrough",
    ],
    linkopts = [
        "-lnuma",
        "-ldl",
        "-lpcap",
    ],
    deps = [
        "//stratum/glue:integral_types",
        "//stratum/glue:logging",
        "//stratum/glue/status:status_macros",
        "//stratum/lib:constants",
        "//stratum/lib:macros",
        "@com_google_absl//absl/container:flat_hash_map",
        # "@local_dpdk_bin//:dpdk",
        "@local_dpdk_bin//:dpdk_shared",
    ],
)
