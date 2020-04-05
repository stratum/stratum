# Copyright 2019 Google LLC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library", "go_test")
load("//stratum/testing/cdlang:antlr.bzl", "go_antlr4_combined")

licenses(["notice"])  # Apache v2

package(
    default_visibility = ["//visibility:public"],
)

go_binary(
    name = "cdl_tool",
    srcs = ["main.go"],
    visibility = ["//visibility:public"],
    deps = [
        "//stratum/testing/cdlang",
        "//stratum/testing/cdlang:cdl",
    ],
)

go_antlr4_combined(
    name = "cdlang",
    src = "CDLang.g4",
)

go_library(
    name = "cdl",
    srcs = [
        "dom.go",
        "dom_visitor.go",
        "dom_visitor_add_next_ref.go",
        "dom_visitor_add_var_decl.go",
        "dom_visitor_any_order.go",
        "dom_visitor_path_coverage.go",
        "dom_visitor_set_id.go",
        "dom_visitor_version.go",
        "tree_builder.go",
        "visitor.go",
    ],
    importpath = "cdl",
    deps = [
        "//stratum/testing/cdlang",
        "@com_github_antlr//runtime/Go/antlr:go_default_library",
    ],
)

go_test(
    name = "cdl_test",
    size = "small",
    srcs = ["main_test.go"],
    embed = ["//stratum/testing/cdlang:cdl"],
)
