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

# Compiler warnings for different toolchains.
STRATUM_COMPILER_WARNINGS_COMMON = [
    "-Wall",
]
STRATUM_DISABLED_COMPILER_WARNINGS_COMMON = [
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
]
STRATUM_DISABLED_COMPILER_WARNINGS_GCC = []
STRATUM_DISABLED_COMPILER_WARNINGS_LLVM = [
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-missing-variable-declarations",
    "-Wno-suggest-destructor-override",
    "-Wno-unused-lambda-capture",
    "-Wno-unused-template",
]

# TODO(max): Bazel currently does not detect GCC, therefore we have to match on
# the default conditions. See: https://github.com/bazelbuild/bazel/issues/12707
STRATUM_DISABLED_COMPILER_WARNINGS = STRATUM_DISABLED_COMPILER_WARNINGS_COMMON + select({
    "//stratum:llvm_compiler": STRATUM_DISABLED_COMPILER_WARNINGS_LLVM,
    "//conditions:default": STRATUM_DISABLED_COMPILER_WARNINGS_GCC,
})

# Compiler warnings that are threated as errors.
STRATUM_COMPILER_ERRORS_COMMON = [
    "-Werror=delete-non-virtual-dtor",
    "-Werror=ignored-attributes",
    "-Werror=ignored-qualifiers",
    "-Werror=parentheses",
    "-Werror=shift-negative-value",
    "-Werror=uninitialized",
    "-Werror=unreachable-code",
]
STRATUM_COMPILER_ERRORS_GCC = [
    "-fdiagnostics-color=always",
    "-Werror=reorder",
    "-Wno-error=maybe-uninitialized",  # GCC detects some false positives
]
STRATUM_COMPILER_ERRORS_LLVM = [
    "-Xclang -coverage-version='800*'",  # Target correct gcov version
    "-Werror=implicit-fallthrough",  # TODO(max): move to common after gcc 7
    "-Werror=inconsistent-missing-destructor-override",
    "-Werror=inconsistent-missing-override",
    "-Werror=overloaded-virtual",
    "-Werror=reorder-ctor",
    "-Werror=return-type",  # TODO(max): move to common after gcc update
    "-Werror=thread-safety-analysis",
    "-Werror=unreachable-code-aggressive",
]
STRATUM_COMPILER_ERRORS = STRATUM_COMPILER_ERRORS_COMMON + select({
    "//stratum:llvm_compiler": STRATUM_COMPILER_ERRORS_LLVM,
    "//conditions:default": STRATUM_COMPILER_ERRORS_GCC,
})

# Exported default compiler options for use.
STRATUM_DEFAULT_COPTS = select({
                            "//stratum:llvm_compiler": ABSL_LLVM_FLAGS,
                            "//conditions:default": ABSL_GCC_FLAGS,
                        }) + \
                        STRATUM_COMPILER_WARNINGS_COMMON + \
                        STRATUM_DISABLED_COMPILER_WARNINGS + \
                        STRATUM_COMPILER_ERRORS

STRATUM_TEST_COPTS = select({
    "//stratum:llvm_compiler": ABSL_LLVM_TEST_FLAGS,
    "//conditions:default": ABSL_GCC_TEST_FLAGS,
}) + STRATUM_DEFAULT_COPTS

STRATUM_DEFAULT_LINKOPTS = select({
    "//conditions:default": [],
})
