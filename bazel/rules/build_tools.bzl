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

load("//bazel:workspace_rule.bzl", "remote_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def build_tools_deps():
    # -----------------------------------------------------------------------------
    #      Golang specific libraries.
    # -----------------------------------------------------------------------------
#    if "net_zlib" not in native.existing_rules():
#        native.bind(
#            name = "zlib",
#            actual = "@net_zlib//:zlib",
#        )
#        http_archive(
#            name = "net_zlib",
#            build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
#            sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
#            strip_prefix = "zlib-1.2.11",
#            urls = ["https://zlib.net/zlib-1.2.11.tar.gz"],
#        )
    if "io_bazel_rules_go" not in native.existing_rules():
        remote_workspace(
            name = "io_bazel_rules_go",
            remote = "https://github.com/bazelbuild/rules_go",
            commit = "2eb16d80ca4b302f2600ffa4f9fc518a64df2908",
        )
    if "bazel_gazelle" not in native.existing_rules():
        remote_workspace(
            name = "bazel_gazelle",
            remote = "https://github.com/bazelbuild/bazel-gazelle",
            commit = "e443c54b396a236e0d3823f46c6a931e1c9939f2",
        )
