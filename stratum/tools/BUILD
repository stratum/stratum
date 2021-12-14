# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")
load("//bazel:rules.bzl", "stratum_license_tar")

pkg_tar(
    name = "stratum_tools_bin",
    srcs = [
        "//stratum/hal/bin/barefoot:bf_pipeline_builder",
        "//stratum/hal/bin/dummy:stratum_dummy",
        "//stratum/hal/config:chassis_config_migrator",
        "//stratum/hal/lib/phal:phal_cli",
        "//stratum/hal/stub/embedded:stratum_stub",
        "//stratum/procmon:procmon_main",
        "//stratum/tools/gnmi:gnmi_cli",
        "//stratum/tools/stratum_replay",
    ],
    mode = "0755",
    package_dir = "/usr/bin",
)

stratum_license_tar(
    name = "stratum_tools_legal",
    mode = "0644",
    package_dir = "/usr/share/doc/stratum-tools/",
)

pkg_tar(
    name = "stratum_tools_data",
    extension = "tar.xz",
    deps = [
        ":stratum_tools_bin",
        ":stratum_tools_legal",
    ],
)

pkg_deb(
    name = "stratum_tools_deb",
    architecture = "amd64",
    conflicts = [],
    data = ":stratum_tools_data",
    description = "The Stratum tools package",
    homepage = "https://stratumproject.org/",
    maintainer = "The Stratum Project",
    package = "stratum-tools",
    version = "0.0.1",
)
