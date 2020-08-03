# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

"""
    Currently, Stratum only supports the x86 architecture for testing
    and embedded system. We may support more architectures in the future.

    EMBEDDED_ARCHES is the set of supported embedded systems
      (e.g. switch platforms)
    HOST_ARCHES is the set of support host architectures
      (e.g. for running against a simulator or testing)
    STRATUM_DEFAULT_COPTS are the default flags passed to the compiler.
"""

load(
    "@com_google_absl//absl:copts/GENERATED_copts.bzl",
    "ABSL_GCC_FLAGS",
    "ABSL_GCC_TEST_FLAGS",
    "ABSL_LLVM_FLAGS",
    "ABSL_LLVM_TEST_FLAGS",
)

EMBEDDED_ARCHES = ["x86"]
HOST_ARCHES = ["x86"]
STRATUM_INTERNAL = ["//stratum:__subpackages__"]
STRATUM_DEFAULT_COPTS = select({
    "//stratum:llvm_compiler": ABSL_LLVM_FLAGS,
    "//conditions:default": ABSL_GCC_FLAGS,
})
STRATUM_TEST_COPTS = STRATUM_DEFAULT_COPTS + select({
    "//stratum:llvm_compiler": ABSL_LLVM_TEST_FLAGS,
    "//conditions:default": ABSL_GCC_TEST_FLAGS,
})
STRATUM_DEFAULT_LINKOPTS = select({
    "//conditions:default": [],
})
