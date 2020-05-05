# This Skylark rule imports the netcope shared libraries and headers since
# there is not yet any native support for Bazel in netcope. The NP4_INSTALL
# environment variable needs to be set, otherwise the Stratum rules which
# depend on netcope cannot be built.

def _impl(repository_ctx):
    if "NP4_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    netcope_path = repository_ctx.os.environ["NP4_INSTALL"]
    repository_ctx.symlink(netcope_path, "netcope-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "np4_headers",
    hdrs = glob(["netcope-bin/include/np4/*.hpp", 
                 "netcope-bin/include/np4/p4*.h"]),
    includes = ["netcope-bin/include"],
)

cc_import(
  name = "np4_atom",
  hdrs = [], # see cc_library rule above 
  shared_library = "netcope-bin/lib/libnp4atom.so",
)

""")

np4intel_configure = repository_rule(
    implementation = _impl,
    local = True,
    environ = ["NP4_INSTALL"],
)
