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
    "//build_util:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_package",
    "stratum_platform_select",
    "stratum_cc_proto_library",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# Removing Stratum's local copies of googleapis protos
#genrule(
#    name = "status_proto_copy",
#    srcs = ["//googleapis/google/rpc:status.proto"],
#    outs = ["google/rpc/status.proto"],
#    cmd = "cp $(location //googleapis/google/rpc:status.proto) $(location google/rpc/status.proto)",
#)
#
#genrule(
#    name = "code_proto_copy",
#    srcs = ["//googleapis/google/rpc:code.proto"],
#    outs = ["google/rpc/code.proto"],
#    cmd = "cp $(location //googleapis/google/rpc:code.proto) $(location google/rpc/code.proto)",
#)
#
#stratum_cc_proto_library(
#    name = "status_proto",
#    hdrs = ["google/rpc/status.proto"],
#    proto_include = ".",
#)
#
#stratum_cc_proto_library(
#    name = "code_proto",
#    hdrs = ["google/rpc/code.proto"],
#    proto_include = ".",
#)

config_setting(
    name = "stratum_ppc",
    values = {
        "crosstool_top": "//unsupported_toolchains/crosstoolng_powerpc32_8540",
        "copt": "-DSTRATUM_ARCH_PPC",
    },
)

config_setting(
    name = "stratum_x86",
    values = {
        "crosstool_top": "//crosstool:stable",
        "copt": "-DSTRATUM_ARCH_X86",
    },
)

sc_package(
    name = "stratum_package_standalone",
    deps = [
        "//stratum/hal/bin/bcm/standalone:stratum_hal_pkg",
        "//stratum/hal/stub/embedded:stratum_stub_pkg",
    ],
)
