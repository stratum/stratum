P4C_BUILD_DEFAULT_COPTS = [
    "-DCONFIG_PKGDATADIR=\\\"external/com_github_p4lang_p4c\\\"",
    # This is a bit of a hack, but will work if the binary is executed by Bazel
    # For a more comprehensive solution, we need to make p4c aware of Bazel, specifically:
    # https://github.com/bazelbuild/bazel/blob/master/tools/cpp/runfiles/runfiles_src.h
]

P4C_COMMON_DEPS = [
]
