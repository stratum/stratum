#
# Copyright 2018 Google LLC
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

exports_files(["LICENSE"])

load(
    "//third_party/stratum/portage:build_defs.bzl",
    "STRATUM_INTERNAL",
    "sc_cc_bin",
    "sc_package",
    "HOST_ARCHES",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

sc_cc_bin(
    name = "stratum_sim",
    srcs = [
        "main.cc",
    ],
    arches = HOST_ARCHES,
    copts = [
        "-lpthread",
        "-ldl",
        "-lrt",
        "-lutil",
    ],
    deps = [
        "//base",
        "//third_party/absl/memory",
        "//third_party/absl/synchronization",
        "//third_party/stratum/glue:init_google",
        "//third_party/stratum/glue:logging",
        "//third_party/stratum/hal/lib/bcm:bcm_acl_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_chassis_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_diag_shell",
        "//third_party/stratum/hal/lib/bcm:bcm_l2_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_l3_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_node",
        "//third_party/stratum/hal/lib/bcm:bcm_packetio_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_sdk_sim",
        "//third_party/stratum/hal/lib/bcm:bcm_serdes_db_manager",
        "//third_party/stratum/hal/lib/bcm:bcm_switch",
        "//third_party/stratum/hal/lib/common:hal",
        "//third_party/stratum/hal/lib/p4:p4_runtime_real",
        "//third_party/stratum/hal/lib/p4:p4_table_mapper",
        "//third_party/stratum/hal/lib/phal:phal_sim",
        "//third_party/stratum/lib/security:auth_policy_checker",
        "//third_party/stratum/lib/security:credentials_manager",
    ],
)

sc_package(
    name = "stratum_pkg",
    bins = [
        ":stratum_sim",
    ],
)
