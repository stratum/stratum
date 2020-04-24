#
# Copyright 2020-present Open Networking Foundation
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

load("@rules_pkg//:pkg.bzl", "pkg_tar")

licenses(["notice"])  # Apache v2

package(default_visibility = ["//visibility:private"])

genrule(
    name = "dummy_init",
    outs = ["__init__.py"],
    cmd = "touch $@",
)

# Rebuild the expected directory structure
pkg_tar(
    name = "p4",
    srcs = [
        "dummy_init",
    ],
    mode = "0644",
    package_dir = "/p4",
)

pkg_tar(
    name = "p4_config",
    srcs = [
        "dummy_init",
    ],
    mode = "0644",
    package_dir = "/p4/config",
)

pkg_tar(
    name = "p4_config_v1",
    srcs = [
        "@com_github_p4lang_p4runtime//:p4info_py_proto",
        "@com_github_p4lang_p4runtime//:p4types_py_proto",
    ],
    mode = "0644",
    package_dir = "/p4/config/v1",
)

pkg_tar(
    name = "p4_v1",
    srcs = [
        "dummy_init",
        "@com_github_p4lang_p4runtime//:p4data_py_proto",
        "@com_github_p4lang_p4runtime//:p4runtime_py_grpc",
        "@com_github_p4lang_p4runtime//:p4runtime_py_proto",
    ],
    mode = "0644",
    package_dir = "/p4/v1",
)

pkg_tar(
    name = "p4runtime_py",
    mode = "0644",
    deps = [
        ":p4",
        ":p4_config",
        ":p4_config_v1",
        ":p4_v1",
    ],
)
