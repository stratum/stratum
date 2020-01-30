#
# Copyright 2020-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

load("//bazel:rules.bzl", "stratum_cc_library")
load("//bazel:deps.bzl", "BF_SDE_PI_VER")

def barefoot_pi_deps(pi_node_common_deps):

    select_deps = {
        "//conditions:default": ["@com_github_p4lang_PI_bf_9_1_0//proto/frontend:pifeproto"],
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

