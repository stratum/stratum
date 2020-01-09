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
    "stratum_package",
    "stratum_platform_select",
)

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

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
        "crosstool_top": "//unsupported_toolchains/crosstoolng_planetx86",
        "copt": "-DSTRATUM_ARCH_X86",
    },
)

stratum_package(
    name = "stratum_package_standalone",
    deps = [
        "//stratum/hal/bin/bcm/standalone:stratum_hal_pkg",
        "//stratum/hal/stub/embedded:stratum_stub_pkg",
    ],
)
