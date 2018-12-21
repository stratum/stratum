"""P4c test IR and configuration generation rules."""

load("//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain")

#  Compiles P4_16 source into a JSON-encoded p4c Internal Representation.

#  Runs the p4c_ir_json_saver on the P4_16 sources. The P4_16 code
#  should be targeted to the v1model in p4lang_p4c/p4include.

def _generate_p4c_ir(ctx):
    """Preprocesses P4 sources, then runs p4c_ir_json_saver to produce JSON IR."""

    # Preprocess all files and create 'p4_preprocessed_file'
    p4_preprocessed_file = ctx.new_file(
        ctx.configuration.genfiles_dir,
        ctx.label.name + ".pp.p4",
    )
    hdr_include_str = ""
    for hdr in ctx.files.hdrs:
        hdr_include_str += "-I " + hdr.dirname
    cpp_toolchain = find_cpp_toolchain(ctx)

    ctx.action(
        arguments = [
            "-E",
            "-x",
            "c",
            ctx.file.src.path,
            "-I.",
            "-I",
            ctx.file._model.dirname,
            "-I",
            ctx.file._core.dirname,
            hdr_include_str,
            "-o",
            p4_preprocessed_file.path,
        ],
        inputs = ([ctx.file.src] + ctx.files.hdrs + [ctx.file._model] +
                  [ctx.file._core] + ctx.files.cpp),
        outputs = [p4_preprocessed_file],
        progress_message = "Preprocessing...",
        executable = cpp_toolchain.compiler_executable,
    )

    # Run p4c_ir_json_saver on pre-processed P4_16 sources.
    gen_files = [ctx.outputs.out_ir]

    ctx.action(
        arguments = [
            "--skip_p4c_cpp",
            "--p4_to_json_in",
            p4_preprocessed_file.path,
            "-p4_to_json_out",
            gen_files[0].path,
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
            default = Label("//p4lang_p4c:p4include/v1model.p4"),
        ),
        "_core": attr.label(
            allow_single_file = True,
            mandatory = False,
            default = Label("//p4lang_p4c:p4include/core.p4"),
        ),
        "_p4c_ir_json_saver": attr.label(
            cfg = "host",
            executable = True,
            default = Label("//platforms/networking/hercules/p4c_backend/test:p4c_ir_json_saver"),
        ),
        "cpp": attr.label_list(default = [Label("//tools/cpp:crosstool")]),
        "_cc_toolchain": attr.label(
            default = Label("//tools/cpp:current_cc_toolchain"),
        ),
    },
)
