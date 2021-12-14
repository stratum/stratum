# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
)
load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")

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

pkg_tar(
    name = "stratum_dpdk_bin",
    srcs = [
        "start-stratum.sh",
        ":dpdk_pipeline",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

pkg_tar(
    name = "stratum_dpdk_etc",
    srcs = [
        "l2fwd.cli",
        "l2fwd.spec",
        "//stratum/hal/lib/common:gnmi_caps.pb.txt",
    ],
    package_dir = "/etc/stratum",
    deps = [
        "//stratum/hal/config:platform_configs_tar",
    ],
)

pkg_tar(
    name = "stratum_dpdk_data",
    extension = "tar.bz2",
    deps = [
        ":stratum_dpdk_bin",
        # ":stratum_dpdk_legal",
        ":stratum_dpdk_etc",
        "@local_dpdk_bin//:dpdk_shared_library_files",
    ],
)

pkg_deb(
    name = "stratum_dpdk_deb",
    architecture = "amd64",
    conflicts = ["stratum-dpdk"],
    data = ":stratum_dpdk_data",
    depends = [
        # "kmod",
        # "libedit2",
        # "libssl1.1",
        # "systemd",
        # "telnet",
    ],
    description = "The Stratum package for DPDK SWX platform",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-dpdk",
    # postinst = "deb/postinst",
    # prerm = "deb/prerm",
    version = "0.0.1",
)
