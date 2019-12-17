package(default_visibility = ["//visibility:public"])

genrule(
    name = "stub_build",
    srcs = [":meta_build"],
    outs = [
        "libtai.so",
        ],
    cmd = "make -C $(location :stub) && cp $(location :stub)/libtai.so \
    $(@D)",
    visibility = [ '//visibility:public' ],
    # TODO: need to find solution how to use tai directories in the 'cmd' script
    tools = ["stub"]
)

genrule(
    name = "meta_build",
    outs = [
        "taimetadata.h",
        "libmetatai.so",
        ],
    cmd = "make -C $(location :meta) && cp $(location :meta)/libmetatai.so $(location :meta)/taimetadata.h  $(@D)",
    visibility = [ '//visibility:public' ],
    # TODO: need to find solution how to use tai directories in the 'cmd' script
    tools = ["meta"]
)

# for writing tai realisation for your modules, include this library to the your bazel build.
cc_library(
    name = "tai_hdrs",
    srcs = glob(["**/*.h"]),
    includes = ["inc/", "meta/"],
)

# next cc_library are tai stub realisation and switch metadata
cc_library(
    name = "stub_lib",
    srcs = [":stub_build"],
    linkstatic = True,
)

cc_library(
    name = "meta_lib",
    srcs = [":meta_build"],
    linkstatic = True,
)
