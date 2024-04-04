# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

"""A portable build system for Stratum P4 switch stack.

To use this, load() this file in a BUILD file, specifying the symbols needed.

The public symbols are the macros:

  decorate(path)
  sc_cc_lib     Declare a portable Library.
  sc_proto_lib  Declare a portable .proto Library.
  sc_cc_bin     Declare a portable Binary.
  sc_package    Declare a portable tarball package.

and the variables/lists:

  ALL_ARCHES           All known arches.
  EMBEDDED_ARCHES      All embedded arches.
  EMBEDDED_PPC         Name of PowerPC arch - "ppc".
  EMBEDDED_X86         Name of "x86" arch.
  HOST_ARCH            Name of default "host" arch.
  HOST_ARCHES          All host arches.
  STRATUM_INTERNAL     For declaring Stratum internal visibility.

The macros are like cc_library(), proto_library(), and cc_binary(), but with
different options and some restrictions. The key difference: you can
supply lists of architectures for which they should be compiled - defaults
to all if left unstated. Internally, libraries and binaries are generated
for every listed architecture. The names are decorated to keep them different
and allow all to be generated and addressed independently.

This aspect of the system is suboptimal - something along the lines of
augmenting context with a user defined configuration fragment would be a
much cleaner solution.

Currently supported architectures:
  ppc
  x86
"""

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//devtools/build_cleaner/skylark:build_defs.bzl",
    "register_extension_info",
)
load("//tools/build_defs/label:def.bzl", "parse_label")

# Generic path & label helpers. ============================================

def _normpath(path):
    """Normalize a path.

    Normalizes a path by removing unnecessary path-up segments and its
    corresponding directories. Providing own implementation because import os
    is not allowed in build defs.
    For example
       ../../dir/to/deeply/nested/path/../../../other/path
    will become
       ../../dir/to/other/path

    Args:
      path: A valid absolute or relative path to normalize.

    Returns:
      A path equivalent to the input path with minimal use of path-up segments.
      Invalid input paths will stay invalid.
    """
    sep = "/"
    level = 0
    result = []
    for d in path.split(sep):
        if d in ("", "."):
            if result:
                continue
        elif d == "..":
            if level > 0:
                result.pop()
                level += -1
                continue
        else:
            level += 1
        result.append(d)
    return sep.join(result)

# Adds a suffix to a label, expanding implicit targets if needed.
def decorate(label, suffix):
    if label.endswith(":"):  # .../bar: -> .../bar
        label = label[:-1]
    if ":" in label:  # .../bar:bat -> .../bar:bat_suffix
        return "%s_%s" % (label, suffix)
    elif label.startswith("//"):  # //foo/bar -> //foo/bar:bar_suffix
        return "%s:%s_%s" % (label, label.split("/")[-1], suffix)
    else:  # bar -> bar_suffix
        return "%s_%s" % (label, suffix)

# Creates a relative filename from a label, replacing "//" and ":".
def _make_filename(label):
    if label.startswith("//"):  # //foo/bar:bat/baz -> google3_foo/bar/bat/baz
        return label.replace("//", "google3/").replace(":", "/")
    elif label.startswith(":"):  # :bat/baz -> bat/baz
        return label[1:]
    else:  # bat/baz -> bat/baz
        return label

# Adds dquotes around a string.
def dquote(s):
    return '"' + s + '"'

# Adds squotes around a string.
def squote(s):
    return "'" + s + "'"

# Emulate Python 2.5+ str(startswith([prefix ...])
def starts_with(s, prefix_list):
    for prefix in prefix_list:
        if s.startswith(prefix):
            return prefix
    return None

def sc_platform_select(host = None, ppc = None, x86 = None, default = None):
    """Public macro to alter blaze rules based on the platform architecture.

    Generates a blaze select(...) statement that can be used in most contexts to
    alter a blaze rule based on the target platform architecture. If no selection
    is provided for a given platform, {default} is used instead. A specific value
    or default must be provided for every target platform.

    Args:
      host: The value to use for host builds.
      ppc: The value to use for ppc builds.
      x86: The value to use for x86 builds.
      default: The value to use for any of {host,ppc,x86} that isn't specified.

    Returns:
      The requested selector.
    """
    if default == None and (host == None or ppc == None or x86 == None):
        fail("Missing a select value for at least one platform in " +
             "sc_platform_select. Please add.")
    config_label_prefix = "//stratum:stratum_"
    return select({
        "//conditions:default": (host or default),
        config_label_prefix + "ppc": (ppc or default),
        config_label_prefix + "x86": (x86 or default),
    })

# Generates an sc_platform_select based on a textual list of arches.
def sc_platform_filter(value, default, arches):
    return sc_platform_select(
        host = value if "host" in arches else default,
        ppc = value if "ppc" in arches else default,
        x86 = value if "x86" in arches else default,
    )

def sc_platform_alias(
        name,
        host = None,
        ppc = None,
        x86 = None,
        default = None,
        visibility = None):
    """Public macro to create an alias that changes based on target arch.

    Generates a blaze alias that will select the appropriate target. If no
    selection is provided for a given platform and no default is set, a
    dummy default target is used instead.

    Args:
      name: The name of the alias target.
      host: The result of the alias for host builds.
      ppc: The result of the alias for ppc builds.
      x86: The result of the alias for x86 builds.
      default: The result of the alias for any of {host,ppc,x86} that isn't
               specified.
      visibility: The visibility of the alias target.
    """
    native.alias(
        name = name,
        actual = sc_platform_select(
            default = default or "//stratum/portage:dummy",
            host = host,
            ppc = ppc,
            x86 = x86,
        ),
        visibility = visibility,
    )

# Embedded build definitions. ==============================================

EMBEDDED_PPC = "ppc"

EMBEDDED_X86 = "x86"

EMBEDDED_ARCHES = [
    EMBEDDED_PPC,
    EMBEDDED_X86,
]

HOST_ARCH = "host"

HOST_ARCHES = [HOST_ARCH]

ALL_ARCHES = EMBEDDED_ARCHES + HOST_ARCHES

# Identify Stratum platform arch for .pb.h shims and other portability hacks.
_ARCH_DEFINES = sc_platform_select(
    default = ["STRATUM_ARCH_HOST"],
    ppc = ["STRATUM_ARCH_PPC"],
    x86 = ["STRATUM_ARCH_X86"],
)

STRATUM_INTERNAL = [
    "//stratum:__subpackages__",
]

#
# Build options for all embedded architectures
#

# Set _TRACE_SRCS to show sources in embedded sc_cc_lib compile steps.
# This is more general than it may seem: genrule doesn't have hdrs or deps
# attributes, so all embedded dependencies appear as a `src'.
# TODO(unknown): if useful again then inject from cmdline else kill feature.
_TRACE_SRCS = False

# Used for all gcc invocations.
_EMBEDDED_FLAGS = [
    "-O0",  # Don't use this for program-sizing build
    #-- "-Os",  # Use this for program-sizing build
    "-g",  # Don't use this for program-sizing build
    "-Wall",
    "-Werror",  # Warn lots, and force fixing warnings.
    "-no-canonical-prefixes",  # Don't mangle paths and confuse blaze.
    "-fno-builtin-malloc",  # We'll use tcmalloc
    "-fno-builtin-calloc",
    "-fno-builtin-realloc",
    "-fno-builtin-free",
    "-D__STDC_FORMAT_MACROS=1",
    # TODO(unknown): Figure out how we can use $(CC_FLAGS) instead of this.
    "-D__GOOGLE_STL_LEGACY_COMPATIBILITY",
]

# Used for C and C++ compiler invocations.
_EMBEDDED_CFLAGS = [
    "-I$(GENDIR)",
]

# Used for C++ compiler invocations.
_EMBEDDED_CXXFLAGS = [
    "-std=gnu++11",  # Allow C++11 features _and_ GNU extensions.
]

# Used for linking binaries.
_EMBEDDED_LDFLAGS = [
    # "-static",  # Use this for program-sizing build
    # "-Wl,--gc-sections,--no-wchar-size-warning", # Use this for program-sizing build
]

# PPC ======================================================================

_PPC_GRTE = "//unsupported_toolchains/crosstoolng_powerpc32_8540/sysroot"

# X86 ======================================================================

_X86_GRTE = "//grte/v4_x86/release/usr/grte/v4"

# Portability definitions ===================================================

def sc_cc_test(
        name,
        size = None,
        srcs = None,
        deps = None,
        data = None,
        defines = None,
        copts = None,
        linkopts = None,
        visibility = None):
    """Creates a cc_test rule that interacts safely with Stratum builds.

    Generates a cc_test rule that doesn't break the build when an embedded arch
    is selected. During embedded builds this target will generate a dummy binary
    and will not attempt to build any dependencies.

    Args:
      name: Analogous to cc_test name argument.
      size: Analogous to cc_test size argument.
      srcs: Analogous to cc_test srcs argument.
      deps: Analogous to cc_test deps argument.
      data: Analogous to cc_test data argument.
      defines: Analogous to cc_test defines argument.
      copts: Analogous to cc_test copts argument.
      linkopts: Analogous to cc_test linkopts argument.
      visibility: Analogous to cc_test visibility argument.
    """
    cc_test(
        name = name,
        size = size or "small",
        srcs = sc_platform_select(host = srcs or [], default = []),
        deps = sc_platform_select(
            host = deps or [],
            default = ["//stratum/portage:dummy_with_main"],
        ),
        data = data or [],
        defines = defines,
        copts = copts,
        linkopts = linkopts,
        visibility = visibility,
    )

register_extension_info(
    extension_name = "sc_cc_test",
    label_regex_for_dep = "{extension_name}",
)

def sc_cc_lib(
        name,
        deps = None,
        srcs = None,
        hdrs = None,
        arches = None,
        copts = None,
        defines = None,
        includes = None,
        include_prefix = None,
        strip_include_prefix = None,
        data = None,
        testonly = None,
        textual_hdrs = None,
        visibility = None,
        xdeps = None):
    """Creates rules for the given portable library and arches.

    Args:
      name: Analogous to cc_library name argument.
      deps: Analogous to cc_library deps argument.
      srcs: Analogous to cc_library srcs argument.
      hdrs: Analogous to cc_library hdrs argument.
      arches: List of architectures to generate this way.
      copts: Analogous to cc_library copts argument.
      defines: Symbols added as "-D" compilation options.
      includes: Paths to add as "-I" compilation options.
      include_prefix: Analogous to cc_library include_prefix argument.
      strip_include_prefix: Analogous to cc_library strip_include_prefix argument.
      data: Files to provide as data at runtime (host builds only).
      testonly: Standard blaze testonly parameter.
      textual_hdrs: Analogous to cc_library.
      visibility: Standard blaze visibility parameter.
      xdeps: External (file) dependencies of this library - no decorations
          assumed, used and exported as header, not for flags, libs, etc.
    """
    alwayslink = 0
    deps = depset(deps or [])
    srcs = depset(srcs or [])
    hdrs = depset(hdrs or [])
    xdeps = depset(xdeps or [])
    copts = depset(copts or [])
    includes = depset(includes or [])
    data = depset(data or [])
    textual_hdrs = depset(textual_hdrs or [])
    if srcs:
        if [s for s in srcs.to_list() if not s.endswith(".h")]:
            alwayslink = 1
    if not arches:
        arches = ALL_ARCHES
    defs_plus = (defines or []) + _ARCH_DEFINES
    textual_plus = textual_hdrs | depset(deps.to_list())
    cc_library(
        name = name,
        deps = sc_platform_filter(deps, [], arches),
        srcs = sc_platform_filter(srcs, [], arches),
        hdrs = sc_platform_filter(hdrs, [], arches),
        alwayslink = alwayslink,
        copts = sc_platform_filter(copts, [], arches),
        defines = defs_plus,
        includes = sc_platform_filter(includes, [], arches),
        include_prefix = include_prefix,
        strip_include_prefix = strip_include_prefix,
        testonly = testonly,
        textual_hdrs = sc_platform_filter(
            textual_plus | xdeps,
            [],
            arches,
        ),
        data = sc_platform_filter(data, [], arches),
        visibility = visibility,
    )

register_extension_info(
    extension_name = "sc_cc_lib",
    label_regex_for_dep = "{extension_name}",
)

def sc_cc_bin(
        name,
        deps = None,
        srcs = None,
        arches = None,
        copts = None,
        defines = None,
        includes = None,
        testonly = None,
        visibility = None):
    """Creates rules for the given portable binary and arches.

    Args:
      name: Analogous to cc_binary name argument.
      deps: Analogous to cc_binary deps argument.
      srcs: Analogous to cc_binary srcs argument.
      arches: List of architectures to generate this way.
      copts: Analogous to cc_binary copts argument.
      defines: Symbols added as "-D" compilation options.
      includes: Paths to add as "-I" compilation options.
      testonly: Standard blaze testonly parameter.
      visibility: Standard blaze visibility parameter.
    """
    deps = depset(deps or [])
    srcs = depset(srcs or [])
    if not arches:
        arches = ALL_ARCHES
    defs_plus = (defines or []) + _ARCH_DEFINES
    cc_binary(
        name = name,
        deps = sc_platform_filter(
            deps,
            ["//stratum/portage:dummy_with_main"],
            arches,
        ),
        srcs = sc_platform_filter(srcs, [], arches),
        copts = copts,
        defines = defs_plus,
        includes = includes,
        linkopts = ["-ldl", "-lutil"],
        testonly = testonly,
        visibility = visibility,
    )

register_extension_info(
    extension_name = "sc_cc_bin",
    label_regex_for_dep = "{extension_name}",
)

# Protobuf =================================================================

_SC_GRPC_DEPS = [
    "//sandblaze/prebuilt/grpc",
    "//sandblaze/prebuilt/grpc:grpc++_codegen_base",
    "//sandblaze/prebuilt/grpc:grpc++_codegen_proto_lib",
]

_PROTOC = "@com_google_protobuf//:protobuf:protoc"

_PROTOBUF = "@com_google_protobuf//:protobuf"

_SC_GRPC_PLUGIN = "//sandblaze/prebuilt/protobuf:grpc_cpp_plugin"
_GRPC_PLUGIN = "//grpc:grpc_cpp_plugin"

def _loc(target):
    """Return target location for constructing commands.

    Args:
      target: Blaze target name available to this build.

    Returns:
      $(location target)
    """
    return "$(location %s)" % target

def _gen_proto_lib(
        name,
        srcs,
        hdrs,
        deps,
        arch,
        visibility,
        testonly,
        proto_include,
        grpc_shim_rule):
    """Creates rules and filegroups for embedded protobuf library.

    For every given ${src}.proto, generate:
      :${src}_${arch}.pb rule to run protoc
          ${src}.proto => ${src}.${arch}.pb.{h,cc}
      :${src}_${arch}.grpc.pb rule to run protoc w/ erpc plugin:
          ${src}.proto => ${src}.${arch}.grpc.pb.{h,cc}
      :${src}_${arch}_proto_rollup collects include options for protoc:
           ${src}_${arch}_proto_rollup.flags

    Feed each set into sc_cc_lib to wrap them them up into a usable library;
    note that ${src}_${arch}_erpc_proto depends on ${src}_${arch}_proto.

    Args:
      name: Base name for this library.
      srcs: List of proto files
      hdrs: More files to build into this library, but also exported for
            dependent rules to utilize.
      deps: List of deps for this library
      arch: Which architecture to build this library for.
      visibility: Standard blaze visibility parameter, passed through to
                  subsequent rules.
      testonly: Standard blaze testonly parameter.
      proto_include: Include path for generated sc_cc_libs.
      grpc_shim_rule: If needed, the name of the grpc shim for this proto lib.
    """
    bash_vars = ["g3=$${PWD}"]

    # TODO(unknown): Switch protobuf to using the proto_include mechanism
    protoc_label = _PROTOC
    protobuf_label = _PROTOBUF
    protobuf_hdrs = "%s:well_known_types_srcs" % protobuf_label
    protobuf_srcs = [protobuf_hdrs]
    protobuf_include = "$${g3}/protobuf/src"
    if arch in EMBEDDED_ARCHES:
        grpc_plugin = _SC_GRPC_PLUGIN
    else:
        grpc_plugin = _GRPC_PLUGIN
    protoc_deps = []
    for dep in deps:
        if dep.endswith("_proto"):
            protoc_deps.append("%s_%s_headers" % (dep, arch))

    name_arch = decorate(name, arch)

    # We use this filegroup to accumulate the set of .proto files needed to
    # compile this proto.
    native.filegroup(
        name = decorate(name_arch, "headers"),
        srcs = hdrs + protoc_deps,
        visibility = visibility,
    )
    my_proto_rollup = decorate(name_arch, "proto_rollup.flags")
    protoc_srcs_set = (srcs + hdrs + protoc_deps +
                       protobuf_srcs + [my_proto_rollup])
    gen_srcs = []
    gen_hdrs = []
    grpc_gen_hdrs = []
    grpc_gen_srcs = []
    tools = [protoc_label]
    grpc_tools = [protoc_label, grpc_plugin]
    protoc = "$${g3}/%s" % _loc(protoc_label)
    grpc_plugin = "$${g3}/%s" % _loc(grpc_plugin)
    cpp_out = "$${g3}/$(GENDIR)/%s/%s" % (native.package_name(), arch)
    accum_flags = []
    full_proto_include = None
    if proto_include == ".":
        full_proto_include = native.package_name()
    elif proto_include:
        full_proto_include = "%s/%s" % (native.package_name(), proto_include)
    if full_proto_include:
        temp_prefix = "%s/%s" % (cpp_out, native.package_name()[len(full_proto_include):])

        # We do a bit of extra work with these include flags to avoid generating
        # warnings.
        accum_flags.append(
            "$$(if [[ -e $(GENDIR)/%s ]]; then echo -IG3LOC/$(GENDIR)/%s; fi)" %
            (full_proto_include, full_proto_include),
        )
        accum_flags.append(
            "$$(if [[ -e %s ]]; then echo -IG3LOC/%s; fi)" %
            (full_proto_include, full_proto_include),
        )
    else:
        temp_prefix = "%s/%s" % (cpp_out, native.package_name())
    proto_rollups = [
        decorate(decorate(dep, arch), "proto_rollup.flags")
        for dep in deps
        if dep.endswith("_proto")
    ]
    proto_rollup_cmds = ["printf '%%s\n' %s" % flag for flag in accum_flags]
    proto_rollup_cmds.append("cat $(SRCS)")
    proto_rollup_cmd = "{ %s; } | sort -u -o $(@)" % "; ".join(proto_rollup_cmds)
    native.genrule(
        name = decorate(name_arch, "proto_rollup"),
        srcs = proto_rollups,
        outs = [my_proto_rollup],
        cmd = proto_rollup_cmd,
        visibility = visibility,
        testonly = testonly,
    )
    for src in srcs + hdrs:
        if src.endswith(".proto"):
            src_stem = src[0:-6]
            src_arch = "%s_%s" % (src_stem, arch)
            temp_stem = "%s/%s" % (temp_prefix, src_stem)
            gen_stem = "%s.%s" % (src_stem, arch)

            # We can't use $${PWD} until this step, because our rollup command
            # might be generated on another forge server.
            proto_path_cmds = ["rollup=$$(sed \"s,G3LOC,$${PWD},g\" %s)" %
                               _loc(my_proto_rollup)]
            proto_rollup_flags = ["$${rollup}"]
            if proto_include:
                # We'll be cd-ing to another directory before protoc, so
                # adjust our .proto path accordingly.
                proto_src_loc = "%s/%s" % (native.package_name(), src)
                if proto_src_loc.startswith(full_proto_include + "/"):
                    proto_src_loc = proto_src_loc[len(full_proto_include) + 1:]
                else:
                    print("Invalid proto include '%s' doesn't match src %s" %
                          (full_proto_include, proto_src_loc))

                # By cd-ing to another directory, we force protoc to produce
                # different symbols. Careful, our proto might be in GENDIR!
                proto_path_cmds.append("; ".join([
                    "if [[ -e %s ]]" % ("%s/%s" % (full_proto_include, proto_src_loc)),
                    "then cd %s" % full_proto_include,
                    "else cd $(GENDIR)/%s" % full_proto_include,
                    "fi",
                ]))
                gendir_include = ["-I$${g3}/$(GENDIR)", "-I$${g3}", "-I."]
            else:
                proto_src_loc = "%s/%s" % (native.package_name(), src)
                proto_path_cmds.append("[[ -e %s ]] || cd $(GENDIR)" % proto_src_loc)
                gendir_include = ["-I$(GENDIR)", "-I."]

            # Generate messages
            gen_pb_h = gen_stem + ".pb.h"
            gen_pb_cc = gen_stem + ".pb.cc"
            gen_hdrs.append(gen_pb_h)
            gen_srcs.append(gen_pb_cc)
            cmds = bash_vars + [
                "mkdir -p %s" % temp_prefix,
            ] + proto_path_cmds + [
                " ".join([protoc] +
                         gendir_include +
                         proto_rollup_flags +
                         [
                             "-I%s" % protobuf_include,
                             "--cpp_out=%s" % cpp_out,
                             proto_src_loc,
                         ]),
                "cd $${g3}",
                "cp %s.pb.h %s" % (temp_stem, _loc(gen_pb_h)),
                "cp %s.pb.cc %s" % (temp_stem, _loc(gen_pb_cc)),
            ]
            pb_outs = [gen_pb_h, gen_pb_cc]
            native.genrule(
                name = src_arch + ".pb",
                srcs = protoc_srcs_set,
                outs = pb_outs,
                tools = tools,
                cmd = " && ".join(cmds),
                heuristic_label_expansion = 0,
                visibility = visibility,
            )

            # Generate GRPC
            if grpc_shim_rule:
                gen_grpc_pb_h = gen_stem + ".grpc.pb.h"
                gen_grpc_pb_cc = gen_stem + ".grpc.pb.cc"
                grpc_gen_hdrs.append(gen_grpc_pb_h)
                grpc_gen_srcs.append(gen_grpc_pb_cc)
                cmds = bash_vars + [
                    "mkdir -p %s" % temp_prefix,
                ] + proto_path_cmds + [
                    " ".join([
                                 protoc,
                                 "--plugin=protoc-gen-grpc-cpp=%s" % grpc_plugin,
                             ] +
                             gendir_include +
                             proto_rollup_flags +
                             [
                                 "-I%s" % protobuf_include,
                                 "--grpc-cpp_out=%s" % cpp_out,
                                 proto_src_loc,
                             ]),
                    "cd $${g3}",
                    "cp %s.grpc.pb.h %s" % (temp_stem, _loc(gen_grpc_pb_h)),
                    "cp %s.grpc.pb.cc %s" % (temp_stem, _loc(gen_grpc_pb_cc)),
                ]
                grpc_pb_outs = [gen_grpc_pb_h, gen_grpc_pb_cc]
                native.genrule(
                    name = src_arch + ".grpc.pb",
                    srcs = protoc_srcs_set,
                    outs = grpc_pb_outs,
                    tools = grpc_tools,
                    cmd = " && ".join(cmds),
                    heuristic_label_expansion = 0,
                    visibility = visibility,
                )

    dep_set = depset(deps) | [protobuf_label]
    includes = []
    if proto_include:
        includes = [proto_include]

    # Note: Public sc_proto_lib invokes this once per (listed) arch;
    # which then calls sc_cc_lib with same name for each arch;
    # multiple such calls are OK as long as the arches are disjoint.
    sc_cc_lib(
        name = decorate(name, arch),
        deps = dep_set,
        srcs = gen_srcs,
        hdrs = hdrs + gen_hdrs,
        arches = [arch],
        copts = [],
        includes = includes,
        testonly = testonly,
        textual_hdrs = gen_hdrs,
        visibility = visibility,
    )
    if grpc_shim_rule:
        grpc_name = name[:-6] + "_grpc_proto"
        grpc_dep_set = dep_set | [name] | _SC_GRPC_DEPS
        grpc_gen_hdrs_plus = grpc_gen_hdrs + gen_hdrs
        sc_cc_lib(
            name = decorate(grpc_name, arch),
            deps = grpc_dep_set,
            srcs = grpc_gen_srcs,
            hdrs = hdrs + grpc_gen_hdrs_plus + [grpc_shim_rule],
            arches = [arch],
            copts = [],
            includes = includes,
            testonly = testonly,
            textual_hdrs = grpc_gen_hdrs_plus,
            visibility = visibility,
        )

def _gen_proto_shims(name, pb_modifier, srcs, arches, visibility):
    """Macro to build .pb.h multi-arch master switch for sc_proto_lib.

    For each src path.proto, generates path.pb.h consisting of:
    #ifdef logic to select path.${arch}.pb.h
    Also generates an alias that will select the appropriate proto target
    based on the currently selected platform architecture.

    Args:
      name: Base name for this library.
      pb_modifier: protoc plugin-dependent file extension (e.g.: .pb)
      srcs: List of proto files.
      arches: List of arches this shim should support.
      visibility: The blaze visibility of the generated alias.

    Returns:
      Name of shim rule for use in follow-on hdrs and/or src lists.
    """
    outs = []
    cmds = []
    hdr_ext = pb_modifier + ".h"
    for src in srcs:
        pkg, filename = parse_label(src)
        if not filename.endswith(".proto"):
            continue
        hdr_stem = filename[0:-6]
        new_hdr_name = hdr_stem + hdr_ext
        outs.append(new_hdr_name)

        # Generate lines for shim switch file.
        # Lines expand inside squotes, so quote accordingly.
        include_fmt = "#include " + dquote(pkg + "/" + hdr_stem + ".%s" + hdr_ext)
        lines = [
            "#if defined(STRATUM_ARCH_%s)" % "PPC",
            include_fmt % "ppc",
            "#elif defined(STRATUM_ARCH_%s)" % "X86",
            include_fmt % "x86",
            "#elif defined(STRATUM_ARCH_%s)" % "HOST",
            include_fmt % "host",
            "#else",
            "#error Unknown STRATUM_ARCH",
            "#endif",
        ]
        gen_cmds = [("printf '%%s\\n' '%s'" % line) for line in lines]
        new_hdr_loc = "$(location %s)" % new_hdr_name
        cmds.append("{ %s; } > %s" % (" && ".join(gen_cmds), new_hdr_loc))
    shim_rule = decorate(name, "shims")
    native.genrule(
        name = shim_rule,
        srcs = srcs,
        outs = outs,
        cmd = " && ".join(cmds) or "true",
    )
    sc_platform_alias(
        name = name,
        host = decorate(name, "host") if "host" in arches else None,
        ppc = decorate(name, "ppc") if "ppc" in arches else None,
        x86 = decorate(name, "x86") if "x86" in arches else None,
        visibility = visibility,
    )
    return shim_rule

def _gen_py_proto_lib(name, srcs, deps, visibility, testonly):
    """Creates a py_proto_library from the given srcs.

    There's no clean way to make python protos work with sc_proto_lib's
    proto_include field, so we keep this simple.

    For library "name", generates:
    * ${name}_default_pb, a regular proto library.
    * ${name}_py, a py_proto_library based on ${name}_default_pb.

    Args:
      name: Standard blaze name argument.
      srcs: Standard blaze srcs argument.
      deps: Standard blaze deps argument.
      visibility: Standard blaze visibility argument.
      testonly: Standard blaze testonly argument.
    """
    regular_proto_name = decorate(name, "default_pb")
    py_name = decorate(name, "py")
    proto_library(
        name = regular_proto_name,
        srcs = srcs,
        deps = [decorate(dep, "default_pb") for dep in deps],
        visibility = visibility,
        testonly = testonly,
    )
    native.py_proto_library(
        name = py_name,
        api_version = 2,
        deps = [regular_proto_name],
        visibility = visibility,
        testonly = testonly,
    )

# TODO(unknown): Add support for depending on normal proto_library rules.
def sc_proto_lib(
        name = None,
        srcs = [],
        hdrs = [],
        deps = [],
        arches = [],
        visibility = None,
        testonly = None,
        proto_include = None,
        python_support = False,
        services = []):
    """Public macro to build multi-arch library from Message protobuf(s).

    For library "name", generates:
    * ${name}_shim aka .pb.h master switch - see _gen_proto_shims, above.
    * ${name}_${arch}_pb protobuf compile rules - one for each arch.
    * sc_cc_lib(name) with those as input.
    * ${name}_py a py_proto_library version of this library. Only generated
          if python_support == True.

    Args:
      name: Base name for this library.
      srcs: List of .proto files - private to this library.
      hdrs: As above, but also exported for dependent rules to utilize.
      deps: List of deps for this library
      arches: Which architectures to build this library for, None => ALL.
      visibility: Standard blaze visibility parameter, passed through to
                  subsequent rules.
      testonly: Standard blaze testonly parameter.
      proto_include: Path to add to include path. This will affect the
                     symbols generated by protoc, as well as the include
                     paths used for both sc_cc_lib and sc_proto_lib rules
                     that depend on this rule. Typically "."
      python_support: Defaults to False. If True, generate a python proto library
                      from this rule. Any sc_proto_lib with python support may
                      only depend on sc_proto_libs that also have python support,
                      and may not use the proto_include field in this rule.
      services: List of services to enable {"grpc", "rpc"};
                Only "grpc" is supported. So "rpc" and "grpc" are equivalent.
    """
    if not arches:
        if testonly:
            arches = HOST_ARCHES
        else:
            arches = ALL_ARCHES
    service_enable = {
        "grpc": 0,
    }
    for service in services or []:
        if service == "grpc":
            service_enable["grpc"] = 1
        elif service == "rpc":
            service_enable["grpc"] = 1
        else:
            fail("service='%s' not in (grpc, rpc)" % service)
    deps = depset(deps or [])
    shim_rule = _gen_proto_shims(
        name = name,
        pb_modifier = ".pb",
        srcs = srcs + hdrs,
        arches = arches,
        visibility = visibility,
    )
    grpc_shim_rule = None
    if (service_enable["grpc"]):
        grpc_shim_rule = _gen_proto_shims(
            name = decorate(name[:-6], "grpc_proto"),
            pb_modifier = ".grpc.pb",
            srcs = srcs + hdrs,
            arches = arches,
            visibility = visibility,
        )
    for arch in arches:
        _gen_proto_lib(
            name = name,
            srcs = srcs,
            hdrs = [shim_rule] + hdrs,
            deps = deps,
            arch = arch,
            visibility = visibility,
            testonly = testonly,
            proto_include = proto_include,
            grpc_shim_rule = grpc_shim_rule,
        )
    if python_support:
        if proto_include:
            fail("Cannot use proto_include on an sc_proto_lib with python support.")
        _gen_py_proto_lib(
            name = name,
            srcs = depset(srcs + hdrs),
            deps = deps,
            visibility = visibility,
            testonly = testonly,
        )

register_extension_info(
    extension_name = "sc_proto_lib",
    label_regex_for_dep = "{extension_name}",
)

def sc_package(
        name = None,
        bins = None,
        data = None,
        deps = None,
        arches = None,
        visibility = None):
    """Public macro to package binaries and data for deployment.

    For package "name", generates:
    * ${name}_${arch}_bin and ${name}_${arch}_data filesets containing
      respectively all of the binaries and all of the data needed for this
      package and all dependency packages.
    * ${name}_${arch} fileset containing the corresponding bin and data
      filesets, mapped to bin/ and share/ respectively.
    * ${name}_${arch}_tarball rule builds that .tar.gz package.

    Args:
      name: Base name for this package.
      bins: List of sc_cc_bin rules to be packaged.
      data: List of files (and file producing rules) to be packaged.
      deps: List of other sc_packages to add to this package.
      arches: Which architectures to build this library for,
              None => EMBEDDED_ARCHES (HOST_ARCHES not generally supported).
      visibility: Standard blaze visibility parameter, passed through to
                  all filesets.

    """

    bins = depset(bins or [])
    data = depset(data or [])
    deps = depset(deps or [])
    if not arches:
        arches = EMBEDDED_ARCHES
    fileset_name = decorate(name, "fs")
    for extension, inputs in [
        ("bin", ["%s.stripped" % b for b in bins.to_list()]),
        ("data", data),
    ]:
        native.Fileset(
            name = decorate(fileset_name, extension),
            out = decorate(name, extension),
            entries = [
                native.FilesetEntry(
                    files = inputs,
                ),
            ] + [
                native.FilesetEntry(srcdir = decorate(dep, extension))
                for dep in deps.to_list()
            ],
            visibility = visibility,
        )

    # Add any platform specific files to the final tarball.
    platform_entries = sc_platform_select(
        # We use a different ppc toolchain for Stratum.
        # This means that we must provide portable shared libs for our ppc
        # executables.
        ppc = [native.FilesetEntry(
            srcdir = "%s:BUILD" % _PPC_GRTE,
            files = [":libs"],
            destdir = "lib/stratum",
            symlinks = "dereference",
        )],
        default = [],
    )
    native.Fileset(
        name = fileset_name,
        out = name,
        entries = [
            native.FilesetEntry(
                srcdir = decorate(name, "bin"),
                destdir = "bin",
            ),
            native.FilesetEntry(
                srcdir = decorate(name, "data"),
                destdir = "share",
            ),
        ] + platform_entries,
        visibility = visibility,
    )

    outs = ["%s.tar.gz" % name]

    # Copy our files into a temporary directory and make any necessary changes
    # before tarballing.
    cmds = [
        "TEMP_DIR=$(@D)/stratum_packaging_temp",
        "mkdir $${TEMP_DIR}",
        "cp -r %s $${TEMP_DIR}/tarball" % _loc(fileset_name),
        "if [[ -e $${TEMP_DIR}/tarball/bin ]]",
        "then for f in $${TEMP_DIR}/tarball/bin/*.stripped",
        "    do mv $${f} $${f%.stripped}",  # rename not available.
        "done",
        "fi",
        "tar czf %s -h -C $${TEMP_DIR}/tarball ." % _loc(name + ".tar.gz"),
        "rm -rf $${TEMP_DIR}",
    ]
    native.genrule(
        name = decorate(name, "tarball"),
        srcs = [":%s" % fileset_name],
        outs = outs,
        cmd = "; ".join(cmds),
        visibility = visibility,
    )
