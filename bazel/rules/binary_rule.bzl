load("@rules_cc//cc:defs.bzl", "cc_binary")
load(
    "//bazel:defs.bzl",
    "STRATUM_DEFAULT_COPTS",
)

def stratum_cc_binary(
        name,
        deps = None,
        srcs = None,
        data = None,
        args = None,
        copts = [],
        defines = None,
        includes = None,
        linkopts = [],
        testonly = None,
        visibility = None,
        arches = None):
    if arches and arches != ["x86"] and arches != ["host"]:
        fail("Stratum does not currently support non-x86 architectures")

    cc_binary(
        name = name,
        deps = deps,
        srcs = srcs,
        data = data,
        args = args,
        copts = STRATUM_DEFAULT_COPTS + copts,
        defines = defines,
        includes = includes,
        linkopts = linkopts,
        testonly = testonly,
        visibility = visibility,
    )
