# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_deb", "pkg_tar")

pkg_tar(
    name = "stratum_tools_data",
    srcs = [
        "//stratum/hal/bin/barefoot:bf_pipeline_builder",
        "//stratum/hal/bin/dummy:stratum_dummy",
        "//stratum/hal/config:chassis_config_migrator",
        "//stratum/hal/lib/phal:phal_cli",
        "//stratum/hal/lib/phal/onlp:onlp_cli",
        "//stratum/hal/stub/embedded:stratum_stub",
        "//stratum/procmon:procmon_main",
        "//stratum/tools/gnmi:gnmi_cli",
        "//stratum/tools/stratum_replay",
    ],
    extension = "tar.xz",
    mode = "0755",
    package_dir = "/usr/bin",
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
