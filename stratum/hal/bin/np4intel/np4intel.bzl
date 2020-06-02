# Copyright 2019-present Barefoot Networks, Inc.
# Copyright 2019-present Dell EMC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Skylark rule imports the netcope shared libraries and headers since
# there is not yet any native support for Bazel in netcope. The NP4_INSTALL
# environment variable needs to be set, otherwise the Stratum rules which
# depend on netcope cannot be built.

def _impl(repository_ctx):
    if "NP4_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", "")
        return
    netcope_path = repository_ctx.os.environ["NP4_INSTALL"]
    repository_ctx.symlink(netcope_path, "netcope-bin")
    repository_ctx.symlink(Label("@//bazel:external/np4intel.BUILD"), "BUILD")

np4intel_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["NP4_INSTALL"],
)
