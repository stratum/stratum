# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load(
    "//bazel:defs.bzl",
    _EMBEDDED_ARCHES = "EMBEDDED_ARCHES",
    _HOST_ARCHES = "HOST_ARCHES",
    _STRATUM_DEFAULT_COPTS = "STRATUM_DEFAULT_COPTS",
    _STRATUM_DEFAULT_LINKOPTS = "STRATUM_DEFAULT_LINKOPTS",
    _STRATUM_INTERNAL = "STRATUM_INTERNAL",
    _STRATUM_TEST_COPTS = "STRATUM_TEST_COPTS",
)
load("//bazel/rules:binary_rule.bzl", _stratum_cc_binary = "stratum_cc_binary")
load("//bazel/rules:library_rule.bzl", _stratum_cc_library = "stratum_cc_library")
load("//bazel/rules:package_rule.bzl", _stratum_package = "stratum_package")
load(
    "//bazel/rules:platform_rules.bzl",
    _stratum_platform_alias = "stratum_platform_alias",
    _stratum_platform_filter = "stratum_platform_filter",
    _stratum_platform_select = "stratum_platform_select",
)
load("//bazel/rules:test_rule.bzl", _stratum_cc_test = "stratum_cc_test")
load("//bazel/rules:license.bzl", _stratum_license_tar = "stratum_license_tar")

stratum_cc_library = _stratum_cc_library
stratum_cc_binary = _stratum_cc_binary
stratum_cc_test = _stratum_cc_test
stratum_license_tar = _stratum_license_tar
stratum_platform_select = _stratum_platform_select
stratum_platform_filter = _stratum_platform_filter
stratum_platform_alias = _stratum_platform_alias
stratum_package = _stratum_package
EMBEDDED_ARCHES = _EMBEDDED_ARCHES
HOST_ARCHES = _HOST_ARCHES
STRATUM_INTERNAL = _STRATUM_INTERNAL
STRATUM_DEFAULT_COPTS = _STRATUM_DEFAULT_COPTS
STRATUM_TEST_COPTS = _STRATUM_TEST_COPTS
STRATUM_DEFAULT_LINKOPTS = _STRATUM_DEFAULT_LINKOPTS
