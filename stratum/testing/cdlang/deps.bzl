#
# Copyright 2019 Google LLC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

"""Load dependencies needed for CDLang."""

load("@bazel_gazelle//:deps.bzl", "go_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_jar")

def cdlang_rules_dependencies():
    http_jar(
        name = "antlr4_tool",
        url = "https://repo1.maven.org/maven2/org/antlr/antlr4/4.7.1/antlr4-4.7.1.jar",
        sha256 = "a2cdc2f2f8eb893728832568dc54d080eb5a1495edb3b66e51b97122a60a0d87",
    )
    http_jar(
        name = "antlr4_runtime",
        url = "https://repo1.maven.org/maven2/org/antlr/antlr4-runtime/4.7.1/antlr4-runtime-4.7.1.jar",
        sha256 = "43516d19beae35909e04d06af6c0c58c17bc94e0070c85e8dc9929ca640dc91d",
    )
    http_jar(
        name = "antlr3_runtime",
        url = "https://repo1.maven.org/maven2/org/antlr/antlr-runtime/3.5.2/antlr-runtime-3.5.2.jar",
        sha256 = "ce3fc8ecb10f39e9a3cddcbb2ce350d272d9cd3d0b1e18e6fe73c3b9389c8734",
    )
    http_jar(
        name = "stringtemplate4",
        url = "https://repo1.maven.org/maven2/org/antlr/ST4/4.0.8/ST4-4.0.8.jar",
        sha256 = "58caabc40c9f74b0b5993fd868e0f64a50c0759094e6a251aaafad98edfc7a3b",
    )
    http_jar(
        name = "javax_json",
        url = "https://repo1.maven.org/maven2/org/glassfish/javax.json/1.0.4/javax.json-1.0.4.jar",
        sha256 = "0e1dec40a1ede965941251eda968aeee052cc4f50378bc316cc48e8159bdbeb4",
    )
    go_repository(
        name = "com_github_antlr",
        tag = "4.7.1",
        importpath = "github.com/antlr/antlr4",
    )
