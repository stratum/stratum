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

load(
    "//bazel:rules.bzl",
    "STRATUM_INTERNAL",
    "stratum_package",
    "stratum_cc_test",
)
load(":platform_config_test.bzl", "platform_config_test")
load("@bazel_tools//tools/build_defs/pkg:pkg.bzl", "pkg_tar", "pkg_deb")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache v2

filegroup(
    name = "bcm_hardware_specs",
    srcs = [
        "bcm_hardware_specs.pb.txt"
    ]
)

platform_config_test("x86-64-accton-as7712-32x-r0", bcm_target=True)
platform_config_test("x86-64-dell-z9100-c2538-r0", bcm_target=True)
platform_config_test("x86-64-inventec-d7032q28b-r0", bcm_target=True)
platform_config_test("x86-64-quanta-ix1-rangeley-r0", bcm_target=True)

platform_config_test("x86-64-accton-wedge100bf-32x-r0")
platform_config_test("x86-64-accton-wedge100bf-65x-r0")
platform_config_test("x86-64-delta-ag9064v1-r0")
platform_config_test("x86-64-inventec-d5254-r0")
platform_config_test("x86-64-stordis-bf2556x-1t-r0")
platform_config_test("x86-64-stordis-bf6064x-t-r0")

filegroup(
    name="all_configs",
    srcs = glob([
        "*/base_bcm_chassis_map.pb.txt",
        "*/chassis_config.pb.txt",
        "*/phal_config.pb.txt",
        "*/SDKLT.yml",
    ]),
)

pkg_tar(
    name = "configs",
    extension = "tar.gz",
    strip_prefix = ".",  # needed to preserve sub-directory structure: https://github.com/bazelbuild/rules_pkg/issues/82
    package_dir = "stratum_configs",
    srcs = [
        ":bcm_hardware_specs",
        ":all_configs",
    ],
    mode = "0644",
)
