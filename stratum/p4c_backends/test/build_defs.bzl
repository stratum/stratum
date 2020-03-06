#
# Copyright 2018 Google LLC
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

"""P4c test IR and configuration generation rules."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

#  Compiles P4_16 source into a JSON-encoded p4c Internal Representation.

#  Runs the p4c_ir_json_saver on the P4_16 sources. The P4_16 code
#  should be targeted to the v1model in p4lang_p4c/p4include.

def _generate_p4c_ir(ctx):
    """Preprocesses P4 sources, then runs p4c_ir_json_saver to produce JSON IR."""

    # Preprocess all files and create 'p4_preprocessed_file'
    p4_preprocessed_file = ctx.actions.declare_file(
        ctx.genfiles_dir.path + ctx.label.name + ".pp.p4",
    )
    cpp_toolchain = find_cpp_toolchain(ctx)

    # Construct GCC CLI arguments
    gcc_args = ctx.actions.args()
    gcc_args.add("-E")
    gcc_args.add("-x")
    gcc_args.add("c")
    gcc_args.add(ctx.file.src.path)
    gcc_args.add("-I.")
    gcc_args.add("-I")
    gcc_args.add(ctx.file._model.dirname)
    gcc_args.add("-I")
    gcc_args.add(ctx.file._core.dirname)
    for hdr in ctx.files.hdrs:
        gcc_args.add("-I " + hdr.dirname)
    gcc_args.add("-o")
    gcc_args.add(p4_preprocessed_file.path)

    ctx.actions.run(
        arguments = [gcc_args],
        inputs = ([ctx.file.src] + ctx.files.hdrs + [ctx.file._model] +
                  [ctx.file._core] + ctx.files.cpp),
        outputs = [p4_preprocessed_file],
        progress_message = "Preprocessing...",
        executable = cpp_toolchain.compiler_executable,
    )

    # Run p4c_ir_json_saver on pre-processed P4_16 sources.
    gen_files = [ctx.outputs.out_ir]

    ctx.actions.run(
        arguments = [
            "--skip_p4c_cpp",
            "--p4_to_json_in",
            p4_preprocessed_file.path,
            "--p4_to_json_out",
            gen_files[0].path,
            "--p4c_fe_options=--Wdisable=legacy --Wwarn",
        ],
        inputs = [p4_preprocessed_file],
        outputs = gen_files,
        progress_message = "Compiling P4 sources to generate JSON IR",
        executable = ctx.executable._p4c_ir_json_saver,
    )

    return struct(files = depset(gen_files))

p4c_save_ir = rule(
    implementation = _generate_p4c_ir,
    fragments = ["cpp"],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        "hdrs": attr.label_list(
            allow_files = True,
            mandatory = False,
        ),
        "out_ir": attr.output(mandatory = True),
        "_model": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("@com_github_p4lang_p4c//:p4include/v1model.p4"), # FIXME
        ),
        "_core": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("@com_github_p4lang_p4c//:p4include/core.p4"), # FIXME
        ),
        "_p4c_ir_json_saver": attr.label(
            cfg = "host",
            executable = True,
            default = Label("//stratum/p4c_backends/test:p4c_ir_json_saver"),
        ),
        "cpp": attr.label_list(default = [Label("@bazel_tools//tools/cpp:current_cc_toolchain")]), # FIXME
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
    },
)
