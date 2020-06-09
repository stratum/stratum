# Copyright 2020-present Barefoot Networks, Inc.
# SPDX-License-Identifier: Apache-2.0

load("//bazel:rules.bzl", "stratum_cc_library")
load("//bazel:deps.bzl", "BF_SDE_PI_VER")

def barefoot_pi_deps(pi_node_common_deps):

    select_deps = {
        "//conditions:default": ["@com_github_p4lang_PI_bf_9_2_0//proto/frontend:pifeproto"],
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
        select_deps[":sde_" + sde_ver] = [dep]


    # PI Node for Barefoot targets
    stratum_cc_library(
        name = "pi_node_bf",
        srcs = ["pi_node.cc"],
        hdrs = ["pi_node.h"],
        deps = pi_node_common_deps + select(select_deps)
    )

