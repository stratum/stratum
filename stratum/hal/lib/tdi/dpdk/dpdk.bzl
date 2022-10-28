# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# This Starlark rule imports the DPDK SDE shared libraries and headers.
# The DPDK_INSTALL environment variable needs to be set; otherwise, the
# Stratum rules for DPDK platforms cannot be built.

def _impl(repository_ctx):
    if "DPDK_INSTALL" in repository_ctx.os.environ:
        dpdk_sde_path = repository_ctx.os.environ["DPDK_INSTALL"]
    elif "SDE_INSTALL" in repository_ctx.os.environ:
        dpdk_sde_path = repository_ctx.os.environ["SDE_INSTALL"]
    else:
        repository_ctx.file("BUILD", "")
        return
    repository_ctx.symlink(dpdk_sde_path, "dpdk-bin")
    repository_ctx.symlink(Label("@//bazel:external/dpdk.BUILD"), "BUILD")

dpdk_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["DPDK_INSTALL", "SDE_INSTALL"],
)
