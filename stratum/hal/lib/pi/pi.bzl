# This Skylark rule imports the PI shared libraries and headers since there is
# not yet any native support for Bazel in PI. The PI_INSTALL environment
# variable needs to be set, otherwise the Stratum rules which depend on PI
# cannot be built.

def _impl(repository_ctx):
    if "PI_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    pi_path = repository_ctx.os.environ["PI_INSTALL"]
    repository_ctx.symlink(pi_path, "pi-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "pi_headers",
    hdrs = glob(["pi-bin/include/PI/**/*.h"]),
    includes = ["pi-bin/include"],
)
cc_import(
    name = "pifeproto",
    hdrs = [],
    shared_library = "pi-bin/lib/libpifeproto.so",
)
cc_import(
    name = "piall",
    hdrs = [],
    shared_library = "pi-bin/lib/libpiall.so",
)
""")

pi_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["PI_INSTALL"])
