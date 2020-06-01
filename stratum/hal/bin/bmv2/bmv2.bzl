# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Skylark rule imports the bmv2 shared libraries and headers since there is
# not yet any native support for Bazel in bmv2. The BMV2_INSTALL environment
# variable needs to be set, otherwise the Stratum rules which depend on bmv2
# cannot be built.

def _impl(repository_ctx):
    if "BMV2_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", "")
        return
    bmv2_path = repository_ctx.os.environ["BMV2_INSTALL"]
    repository_ctx.symlink(bmv2_path, "bmv2-bin")
    repository_ctx.symlink(Label("@//bazel:external/bmv2.BUILD"), "BUILD")

bmv2_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["BMV2_INSTALL"],
)
