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

"""Load dependencies needed for CDLang."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_jar", "http_archive")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")

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
