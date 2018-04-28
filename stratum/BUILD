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
    "sc_package",
    "sc_imageset",
    "sc_platform_select",
)

package(
    default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

config_setting(
    name = "stratum_ppc",
    values = {
        "crosstool_top": "//third_party/unsupported_toolchains/crosstoolng_powerpc32_8540",
        "copt": "-DSTRATUM_ARCH_PPC",
    },
)

config_setting(
    name = "stratum_x86",
    values = {
        "crosstool_top": "//third_party/crosstool:stable",
        "copt": "-DSTRATUM_ARCH_X86",
    },
)

sc_package(
    name = "stratum_package_standalone",
    deps = [
        "//third_party/stratum/hal/bin/bcm/standalone:stratum_hal_pkg",
        "//third_party/stratum/hal/stub/embedded:stratum_stub_pkg",
    ],
)
