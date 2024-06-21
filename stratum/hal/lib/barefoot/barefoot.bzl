# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# This Starlark rule imports the BF SDE shared libraries and headers. The
# SDE_INSTALL or SDE_INSTALL_TAR environment variable needs to be set;
# otherwise, the Stratum rules for barefoot platforms cannot be built.

def _impl(repository_ctx):
    if ("SDE_INSTALL" not in repository_ctx.os.environ and
        "SDE_INSTALL_TAR" not in repository_ctx.os.environ):
        print("SDE_INSTALL_TAR is not defined")
        repository_ctx.file("BUILD", "")
        return
    local_install_path = "barefoot-bin"
    if "SDE_INSTALL_TAR" in repository_ctx.os.environ:
        bf_sde_install_tar_path = repository_ctx.os.environ["SDE_INSTALL_TAR"]
        repository_ctx.extract(bf_sde_install_tar_path, local_install_path)
    elif "SDE_INSTALL" in repository_ctx.os.environ:
        print("SDE_INSTALL is deprecated, please use SDE_INSTALL_TAR")
        bf_sde_install_path = repository_ctx.os.environ["SDE_INSTALL"]
        repository_ctx.symlink(bf_sde_install_path, local_install_path)
    ver = repository_ctx.read(local_install_path + "/share/VERSION").strip("\n")
    with_bsp = repository_ctx.path(local_install_path + "/lib/libpltfm_mgr.so").exists
    print("SDE with BSP: " + str(with_bsp) + ".")
    repository_ctx.template(
        "BUILD",
        Label("@//bazel:external/bfsde.BUILD"),
        {"{SDE_VERSION}": ver},
        executable = False,
    )
    print("Detected SDE version: " + ver + ".")

barefoot_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["SDE_INSTALL", "SDE_INSTALL_TAR"],
)
