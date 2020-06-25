# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Starlark rule imports the BF SDE shared libraries and headers. The
# SDE_INSTALL environment variable needs to be set, otherwise the Stratum
# rules for barefoot platforms cannot be built.

def _impl(repository_ctx):
    if "SDE_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", "")
        return
    bf_sde_path = repository_ctx.os.environ["SDE_INSTALL"]
    repository_ctx.symlink(bf_sde_path, "barefoot-bin")
    repository_ctx.symlink(Label("@//bazel:external/bfsde.BUILD"), "BUILD")

barefoot_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["SDE_INSTALL"],
)
