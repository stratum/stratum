#
# Copyright 2019 Open Networking Foundation
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
package(default_visibility = ["//visibility:public"])

cc_library(
    name = "yaml_cpp_headers",
    hdrs = glob([
        "include/yaml-cpp/**/*.h",
    ]),
    strip_include_prefix = "include",
)

cc_library(
    name = "yaml_cpp_contrib",
    hdrs = glob([
        "src/contrib/*.h",
    ]),
    srcs = glob([
        "src/contrib/*.cpp"
    ]),
    strip_include_prefix = "src/contrib",
    deps = [
        "yaml_cpp_headers"
    ]
)

cc_library(
    name = "yaml_cpp",
    hdrs = glob([
        "include/yaml-cpp/**/*.h",
        ]),
    srcs = glob(["src/*.cpp",
                 "src/*.h"]),
    strip_include_prefix = "include",
    copts = [
        "-Wall",
        "-DYAML_CPP_VERSION=\\\"0.6.2\\\"",
        "-DYAML_CPP_VERSION_MAJOR=0",
        "-DYAML_CPP_VERSION_MINOR=6",
        "-DYAML_CPP_VERSION_PATCH=2",
    ],
    deps = [
        ":yaml_cpp_contrib"
    ]
)