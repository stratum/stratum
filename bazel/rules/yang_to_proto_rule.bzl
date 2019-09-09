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

# Use proto generator from ygot to generate protobuf files from yang model
# https://github.com/openconfig/ygot

def yang_to_proto(
        name,
        srcs = [],
        hdrs = [],
        outs = [],
        paths = ["."],
        output_dir = "",
        pkg_name = "",
        exclude_modules = [],
        base_import_path = ""):

    if pkg_name == "":
        pkg_name = name

    if output_dir == "" and paths:
        output_dir = paths[0]

    exclude_modules = ",".join(exclude_modules)
    paths = ",".join(paths)

    cmd = " $(location @com_github_openconfig_ygot//proto_generator:proto_generator) -generate_fakeroot" + \
          " -base_import_path=" + base_import_path + \
          " -path=" + paths + \
          " -output_dir=$(@D)" + \
          " -package_name=" + pkg_name + \
          " -exclude_modules=" + exclude_modules + \
          " -compress_paths"

    for src in srcs:
        cmd += " $(locations " + src + ")"

    native.genrule(
        name = name,
        srcs = srcs + hdrs,
        outs = outs,
        cmd = cmd,
        tools = [
            "@com_github_openconfig_ygot//proto_generator:proto_generator"
        ]
    )
