# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Starlark rule imports the DPDK static libraries and headers. The
# DPDK_INSTALL or DPDK_INSTALL_TAR environment variable needs to be set,
# otherwise the Stratum rules for DPDK targets cannot be built.

def _impl(repository_ctx):
    if ("DPDK_INSTALL" not in repository_ctx.os.environ and
        "DPDK_INSTALL_TAR" not in repository_ctx.os.environ):
        print("DPDK_INSTALL_TAR is not defined")
        repository_ctx.file("BUILD", "")
        return
    local_install_path = "dpdk-install"
    if "DPDK_INSTALL_TAR" in repository_ctx.os.environ:
        dpdk_install_tar_path = repository_ctx.os.environ["DPDK_INSTALL_TAR"]
        repository_ctx.extract(dpdk_install_tar_path, local_install_path)
    elif "DPDK_INSTALL" in repository_ctx.os.environ:
        print("DPDK_INSTALL is deprecated, please use DPDK_INSTALL_TAR")
        dpdk_install_tar_path = repository_ctx.os.environ["DPDK_INSTALL"]
        repository_ctx.symlink(dpdk_install_tar_path, local_install_path)

    repository_ctx.template(
        "BUILD",
        Label("@//bazel:external/dpdk.BUILD"),
        {"{SDE_VERSION}": "1"},
        executable = False,
    )

dpdk_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["DPDK_INSTALL", "DPDK_INSTALL_TAR"],
)
