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

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

P4C_BUILD_DEFAULT_COPTS = [
    "-DCONFIG_PKGDATADIR=\\\"external/com_github_p4lang_p4c\\\"",
    # This is a bit of a hack, but will work if the binary is executed by Bazel
    # For a more comprehensive solution, we need to make p4c aware of Bazel, specifically:
    # https://github.com/bazelbuild/bazel/blob/master/tools/cpp/runfiles/runfiles_src.h
]

P4C_COMMON_DEPS = [
]

# The p4_bmv2_compile rule below runs the p4c_bmv2 compiler backend on P4_16
# sources.  The P4_16 code should be targeted to the P4 v1model.  This
# example BUILD rule compiles one of the p4lang/p4c test samples:
#
#   p4_bmv2_compile(
#       name = "p4c_bmv2_compile_test",
#       src = "testdata/p4_16_samples/key-bmv2.p4",
#       hdrs = [
#           "testdata/p4_16_samples/arith-inline-skeleton.p4",
#       ],
#       out_p4_info = "p4c_bmv2_test_p4_info.pb.txt",
#       out_p4_pipeline_json = "p4c_bmv2_test_p4_pipeline.pb.json",
#   )

def _generate_bmv2_config(ctx):
    """Preprocesses P4 sources and runs p4c on pre-processed P4 file."""

    # Preprocess all files and create 'p4_preprocessed_file'.
    p4_preprocessed_file = ctx.new_file(
        ctx.genfiles_dir.path + ctx.label.name + ".pp.p4",
    )
    cpp_toolchain = find_cpp_toolchain(ctx)
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
    gcc_args.add(ctx.attr.copts)

    ctx.action(
        arguments = [gcc_args],
        inputs = ([ctx.file.src] + ctx.files.hdrs + [ctx.file._model] +
                  [ctx.file._core]),
        outputs = [p4_preprocessed_file],
        progress_message = "Preprocessing...",
        executable = cpp_toolchain.compiler_executable,
    )

    # Run p4c on pre-processed P4_16 sources to obtain p4info and bmv2 config.
    gen_files = [
        ctx.outputs.out_p4_info,
        ctx.outputs.out_p4_pipeline_json,
    ]

    ctx.action(
        arguments = [
            "--nocpp",
            "--p4v",
            "16",
            "--p4runtime-format",
            "text",
            "--p4runtime-file",
            gen_files[0].path,
            "-o",
            gen_files[1].path,
            p4_preprocessed_file.path,
        ],
        inputs = [p4_preprocessed_file],
        outputs = [gen_files[0], gen_files[1]],
        progress_message = "Compiling P4 sources to generate bmv2 config",
        executable = ctx.executable._p4c_bmv2,
    )

    return struct(files = depset(gen_files))

# Compiles P4_16 source into bmv2 target JSON configuration and p4info.
p4_bmv2_compile = rule(
    implementation = _generate_bmv2_config,
    fragments = ["cpp"],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        "hdrs": attr.label_list(
            allow_files = True,
            mandatory = True,
        ),
        "out_p4_info": attr.output(mandatory = True),
        "out_p4_pipeline_json": attr.output(mandatory = False),
        "copts": attr.string_list(),
        "_model": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("@com_github_p4lang_p4c//:p4include/v1model.p4"),
        ),
        "_core": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("@com_github_p4lang_p4c//:p4include/core.p4"),
        ),
        "_p4c_bmv2": attr.label(
            cfg = "target",
            executable = True,
            default = Label("@com_github_p4lang_p4c//:p4c_bmv2"),
        ),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    output_to_genfiles = True,
)
