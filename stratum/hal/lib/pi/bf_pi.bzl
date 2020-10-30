# Copyright 2020-present Barefoot Networks, Inc.
# SPDX-License-Identifier: Apache-2.0

load("//bazel:deps.bzl", "BF_SDE_PI_VER")

def barefoot_pi_deps():

    pi_dep_map = {
        "//conditions:default": "@com_github_p4lang_PI_bf_9_2_0//proto/frontend:pifeproto",
    }

    for sde_ver in BF_SDE_PI_VER:
        ver = sde_ver.replace("_", ".")
        native.config_setting(
            name = "sde_" + sde_ver,
            define_values = {
                "sde_ver": ver,
            },
        )
        dep = "@com_github_p4lang_PI_bf_" + sde_ver + "//proto/frontend:pifeproto"
        pi_dep_map[":sde_" + sde_ver] = dep

    native.alias(
        name = "pi_bf",
        actual = select(pi_dep_map),
        visibility = ["//visibility:public"],
    )
