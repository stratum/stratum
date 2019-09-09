#
# Copyright 2019 Google LLC
# Copyright 2019-present Open Networking Foundation
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

load("@io_bazel_rules_go//go:def.bzl", "go_binary", "go_library", "go_test")
load("//stratum/testing/cdlang:antlr.bzl", "go_antlr4_combined")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # Apache v2

go_binary(
    name = "cdl_tool",
    srcs = ["main.go"],
    visibility = ["//visibility:public"],
    deps = [
        "//stratum/testing/cdlang:cdlang",
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
    deps = [
        "//stratum/testing/cdlang:cdlang",
        "@com_github_antlr//runtime/Go/antlr:go_default_library",
    ],
    importpath = "cdl",
)

go_test(
    name = "cdl_test",
    size = "small",
    srcs = ["main_test.go"],
    embed = ["//stratum/testing/cdlang:cdl"],
)
