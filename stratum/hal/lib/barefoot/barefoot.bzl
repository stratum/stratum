# This Skylark rule imports the BF SDE shared libraries and headers. The
# BF_SDE_INSTALL environment variable needs to be set, otherwise the Stratum
# rules for barefoot platforms cannot be built.

def _impl(repository_ctx):
    if "BF_SDE_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    bf_sde_path = repository_ctx.os.environ["BF_SDE_INSTALL"]
    repository_ctx.symlink(bf_sde_path, "barefoot-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "bf_headers",
    hdrs = glob(["barefoot-bin/include/bf_switchd/*.h",
                 "barefoot-bin/include/bfsys/**/*.h",
                 "barefoot-bin/include/bfutils/**/*.h",
                 "barefoot-bin/include/bf_types/*.h",
                 "barefoot-bin/include/dvm/*.h",
                 "barefoot-bin/include/mc_mgr/*.h",
                 "barefoot-bin/include/port_mgr/*.h",
                 "barefoot-bin/include/tofino/bf_pal/*.h"]),
    includes = ["barefoot-bin/include"],
)
cc_import(
  name = "bf_switchd",
  hdrs = [],  # see cc_library rule above
  shared_library = "barefoot-bin/lib/libbf_switchd_lib.so",
  # If alwayslink is turned on, libbf_switchd_lib.so will be forcely linked
  # into any binary that depends on it.
  alwayslink = 1,
)
cc_import(
  name = "bf_drivers",
  hdrs = [],  # see cc_library rule above
  shared_library = "barefoot-bin/lib/libdriver.so",
  alwayslink = 1,
)
cc_import(
  name = "bf_sys",
  hdrs = [],  # see cc_library rule above
  shared_library = "barefoot-bin/lib/libbfsys.so",
  alwayslink = 1,
)
cc_import(
  name = "bf_utils",
  hdrs = [],  # see cc_library rule above
  shared_library = "barefoot-bin/lib/libbfutils.so",
  alwayslink = 1,
)
""")

barefoot_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["BF_SDE_INSTALL"])
