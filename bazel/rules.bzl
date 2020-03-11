#
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

load("//bazel/rules:library_rule.bzl", _stratum_cc_library = "stratum_cc_library")
load("//bazel/rules:binary_rule.bzl", _stratum_cc_binary = "stratum_cc_binary")
load("//bazel/rules:test_rule.bzl", _stratum_cc_test = "stratum_cc_test")
load(
     "//bazel/rules:platform_rules.bzl",
     _stratum_platform_select = "stratum_platform_select",
     _stratum_platform_filter = "stratum_platform_filter",
     _stratum_platform_alias = "stratum_platform_alias",
)
load("//bazel/rules:package_rule.bzl", _stratum_package = "stratum_package")
load(
     ":defs.bzl",
     _EMBEDDED_ARCHES = "EMBEDDED_ARCHES",
     _HOST_ARCHES = "HOST_ARCHES",
     _STRATUM_INTERNAL = "STRATUM_INTERNAL",
)

stratum_cc_library = _stratum_cc_library
stratum_cc_binary = _stratum_cc_binary
stratum_cc_test = _stratum_cc_test
stratum_platform_select = _stratum_platform_select
stratum_platform_filter = _stratum_platform_filter
stratum_platform_alias = _stratum_platform_alias
stratum_package = _stratum_package
EMBEDDED_ARCHES = _EMBEDDED_ARCHES
HOST_ARCHES = _HOST_ARCHES
STRATUM_INTERNAL = _STRATUM_INTERNAL
