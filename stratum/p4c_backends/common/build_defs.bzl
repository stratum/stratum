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


"""P4c configuration generation rules."""

load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

# Runs the p4c binary with the Stratum FPM backend on the P4_16 sources. The P4_16
# code should be targeted to the v1model in p4lang_p4c/p4include.
def _generate_p4c_stratum_config(ctx):
    """Preprocesses P4 sources and runs Stratum p4c on pre-processed P4 file."""

    # Preprocess all files and create 'p4_preprocessed_file'. This is necessary
    # because p4c invokes the GCC (cc1) binary, which is not available in an
    # isolated bazel build sandbox.
    p4_preprocessed_file = ctx.new_file(
        ctx.configuration.genfiles_dir,
        ctx.label.name + ".pp.p4",
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
    gcc_args.add(ctx.attr.copts)
    gcc_args.add("-o")
    gcc_args.add(p4_preprocessed_file.path)

    ctx.action(
        arguments = [gcc_args],
        inputs = ([ctx.file.src] + ctx.files.hdrs + [ctx.file._model] +
                  [ctx.file._core] + ctx.files.cpp),
        outputs = [p4_preprocessed_file],
        progress_message = "Preprocessing...",
        executable = cpp_toolchain.compiler_executable,
    )

    # Run Stratum p4c on pre-processed P4_16 sources to obtain the P4 info and
    # P4 pipeline config files for Stratum FPM switches.
    gen_files = [
        ctx.outputs.out_p4_pipeline_binary,
        ctx.outputs.out_p4_pipeline_text,
        ctx.outputs.out_p4_info,
    ]

    # This string specifies the open source p4c frontend and midend options,
    # which go into the Stratum p4c --p4c_fe_options flag.
    p4c_native_options = "--nocpp --Wwarn=all " + p4_preprocessed_file.path

    annotation_map_files = ""
    for map_file in ctx.files.annotation_maps:
        if annotation_map_files:
            annotation_map_files += ","
        annotation_map_files += map_file.path

    ctx.action(
        arguments = [
            "--p4c_fe_options=" + p4c_native_options,
            "--p4_info_file=" + gen_files[2].path,
            "--p4_pipeline_config_binary_file=" + gen_files[0].path,
            "--p4_pipeline_config_text_file=" + gen_files[1].path,
            "--p4c_annotation_map_files=" + annotation_map_files,
            "--slice_map_file=" + ctx.file.slice_map.path,
            "--target_parser_map_file=" + ctx.file.parser_map.path,
            "--colorlogtostderr",
            "--stderrthreshold=1",
            "--logtostderr",
            "--v=0",
        ],
        inputs = ([p4_preprocessed_file] + [ctx.file.parser_map] +
                  [ctx.file.slice_map] + ctx.files.annotation_maps),
        # Disable ASAN check, because P4C is known to leak memory b/63128624.
        env = {"ASAN_OPTIONS": "halt_on_error=0:detect_leaks=0"},
        outputs = gen_files,
        progress_message = "Compiling P4 sources to generate Stratum P4 config",
        executable = ctx.executable._p4c_stratum_fpm_binary,
    )

    return struct(files = depset(gen_files))

# Compiles P4_16 source into P4 info and P4 pipeline config files. The
# output file names are <name>_p4_info.pb.txt and <name>_p4_pipeline.pb.txt
# in the appropriate path under the genfiles directory.
p4_fpm_compile = rule(
    implementation = _generate_p4c_stratum_config,
    fragments = ["cpp"],
    attrs = {
        "src": attr.label(mandatory = True, allow_single_file = True),
        "hdrs": attr.label_list(
            allow_files = True,
            mandatory = True,
        ),
        "out_p4_info": attr.output(mandatory = True),
        "out_p4_pipeline_binary": attr.output(mandatory = True),
        "out_p4_pipeline_text": attr.output(mandatory = True),
        "annotation_maps": attr.label_list(
            allow_files = True,
            mandatory = False,
            default = [
                Label("//stratum/p4c_backends/fpm:annotation_map_files"),
            ],
        ),
        "parser_map": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("//stratum/p4c_backends/fpm:parser_map_files"),
        ),
        "slice_map": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("//stratum/p4c_backends/fpm:slice_map_files"),
        ),
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
        "_p4c_stratum_fpm_binary": attr.label(
            cfg = "host",
            executable = True,
            default = Label("//stratum/p4c_backends/fpm:p4c-fpm"),
        ),
        "cpp": attr.label_list(default = [Label("@bazel_tools//tools/cpp:current_cc_toolchain")]),
        "_cc_toolchain": attr.label(
            default = Label("@bazel_tools//tools/cpp:current_cc_toolchain"),
        ),
    },
)
