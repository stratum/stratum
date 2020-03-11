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

load("@rules_proto//proto:defs.bzl", "proto_library")

def wrapped_proto_library(name,
                          srcs = [],
                          new_proto_dir = "",
                          rewrite_proto_imports = None,
                          deps = [],
                          proto_source_root = ""):

    if rewrite_proto_imports:
        for item in rewrite_proto_imports.items():
            old_prefix = item[0]
            prefix = item[1]
            if "#" in prefix or "#" in old_prefix:
              fail("prefix and old_prefix cannot contain #")
            cmd = "sed 's#import[ \t]*\"%s#import \"%s#g' $< > $(OUTS)" % (old_prefix, prefix)
    else:
        cmd = "cp $< $(OUTS)"

    gen_srcs = []
    for src in srcs:
        if new_proto_dir or proto_source_root:
            gen_src = src
            if proto_source_root:
                gen_src = gen_src[len(proto_source_root):]
            gen_src = new_proto_dir + gen_src
            gen_name = name + "_generated_" + src
            gen_name = gen_name.replace("/", "_")
            native.genrule(
                name = gen_name,
                srcs = [src],
                cmd = cmd,
                outs = [gen_src],
                visibility = ["//visibility:public"],
            )
            gen_srcs.append(":" + gen_name)
        else:
            gen_srcs.append(src)

    proto_library(
      name = name,
      srcs = gen_srcs,
      deps = deps
    )

# --------------- NOTES ------------------
'''
sc_proto_lib(name = None, srcs = [], hdrs = [], deps = [], arches = [],
                 visibility = None, testonly = None, proto_include = None,
                 python_support = False, services = []):
--- or ---
pubref.cpp_proto_library(
    name,
    langs = [str(Label("//cpp"))],
    protos = [], imports = [], inputs = [], proto_deps = [], output_to_workspace = False,
    protoc = None, pb_plugin = None, pb_options = [],
    grpc_plugin = None, grpc_options = [], proto_compile_args = {},
    with_grpc = True, srcs = [], deps = [], verbose = 0)
--- or ---
native.proto_library(name, deps, srcs, data, compatible_with, deprecation,
        distribs, features, licenses, proto_source_root, restricted_to,
        tags, testonly, visibility)
native.cc_proto_library(name, deps, data, compatible_with, deprecation, distribs,
        features, licenses, restricted_to, tags, testonly, visibility)
grpc.cc_grpc_library(name, srcs, deps, proto_only, well_known_protos,
        generate_mocks = False, use_external = False, **kwargs):

- hdrs vs. protos vs. srcs (in proto_library)
- srcs vs. None vs. None
- deps vs. proto_deps vs. deps (in proto_library)
- arches vs. None vs. None <<same as cc_library>>
- testonly vs. None vs. testonly (in proto_library)
- proto_include vs. imports? vs. use deps? << need to figure out if this is used>>
- python_support vs. separate python rule vs. separate python rule
- services vs. with_grpc vs. separate rule

proposed rule:
stratum_proto_library(name, deps = [], srcs = [], proto_source_root = None,
                      testonly = False, arches = [],
                      with_grpc = True, include_wkt = False)
'''