# Copyright 2018-present Open Networking Foundation
# Copyright 2022 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

# This Starlark rule imports the DPDK SDE shared libraries and headers. The
# SDE_INSTALL or SDE_INSTALL_TAR environment variable needs to be set,
# otherwise, the Stratum rules for DPDK platforms cannot be built.

def _impl(repository_ctx):
    if ("SDE_INSTALL" not in repository_ctx.os.environ and
        "SDE_INSTALL_TAR" not in repository_ctx.os.environ):
        print("SDE_INSTALL_TAR is not defined")
        repository_ctx.file("BUILD", "")
        return
    local_install_path = "tdi-bin"
    if "SDE_INSTALL_TAR" in repository_ctx.os.environ:
        p4_sde_install_tar_path = repository_ctx.os.environ["SDE_INSTALL_TAR"]
        repository_ctx.extract(p4_sde_install_tar_path, local_install_path)
    elif "SDE_INSTALL" in repository_ctx.os.environ:
        print("SDE_INSTALL is deprecated, please use SDE_INSTALL_TAR")
        p4_sde_install_path = repository_ctx.os.environ["SDE_INSTALL"]
        repository_ctx.symlink(p4_sde_install_path, local_install_path)
    ver = repository_ctx.read(local_install_path + "/share/VERSION").strip("\n")
    print("Detected SDE version: " + ver + ".")
    is_dpdk = repository_ctx.path(local_install_path + "/lib/x86_64-linux-gnu/librte_acl.so").exists
    if is_dpdk:
      print("TDI SDE for DPDK.")
      repository_ctx.template(
          "BUILD",
          Label("@//bazel:external/dpdk.BUILD"),
          {"{SDE_VERSION}": ver},
          executable = False,
      )
    else:
      fail("Tofino TDI is not yet supported.")

tdi_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["SDE_INSTALL", "SDE_INSTALL_TAR"],
)
