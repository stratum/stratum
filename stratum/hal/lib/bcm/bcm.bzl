# This Skylark rule imports the Broadcom SDK-LT shared libraries and headers. 
# The SDKLT_INSTALL environment variable needs to be set, otherwise the Stratum
# rules for Broadcom platforms cannot be built.

def _impl(repository_ctx):
    if "SDKLT_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    sdklt_path = repository_ctx.os.environ["SDKLT_INSTALL"]
    repository_ctx.symlink(sdklt_path, "bcm-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "bcm_headers",
    hdrs = glob(["bcm-bin/include/sdklt/shr/*.h" ]),
    includes = ["bcm-bin/include/sdklt"],
)
cc_import(
  name = "bcm_sdklt",
  hdrs = [],  # see cc_library rule above
  shared_library = "bcm-bin/lib/libsdklt.so",
  alwayslink = 1,
)
""")

bcm_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["SDKT_INSTALL"])
