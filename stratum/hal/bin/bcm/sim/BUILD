#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
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

licenses(["notice"])  # Apache v2

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_cc_binary",
    "stratum_package",
    "HOST_ARCHES",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
    # TODO(b/76020938); doesn't work with header modules yet.
    features = ["-use_header_modules"],
)

stratum_cc_binary(
    name = "stratum_sim",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    linkopts = [
        "-lpthread",
        "-ldl",
        "-lrt",
        "-lutil",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/synchronization",
        "//stratum/glue:init_google",
        "//stratum/glue:logging",
        "//stratum/hal/lib/bcm:bcm_acl_manager",
        "//stratum/hal/lib/bcm:bcm_chassis_manager",
        "//stratum/hal/lib/bcm:bcm_diag_shell",
        "//stratum/hal/lib/bcm:bcm_l2_manager",
        "//stratum/hal/lib/bcm:bcm_l3_manager",
        "//stratum/hal/lib/bcm:bcm_node",
        "//stratum/hal/lib/bcm:bcm_packetio_manager",
        "//stratum/hal/lib/bcm:bcm_sdk_sim",
        "//stratum/hal/lib/bcm:bcm_serdes_db_manager",
        "//stratum/hal/lib/bcm:bcm_switch",
        "//stratum/hal/lib/common:hal",
        "//stratum/hal/lib/p4:p4_table_mapper",
        "//stratum/hal/lib/phal:phal_sim",
        "//stratum/lib/security:auth_policy_checker",
        "//stratum/lib/security:credentials_manager",
    ],
)

stratum_package(
    name = "stratum_pkg",
    bins = [
        ":stratum_sim",
    ],
)
