# This Skylark rule imports the onlp shared libraries and headers
# The ONLP_INSTALL environment variable needs to be set
# otherwise will download the prebuit libraries

ONLP_URL = "https://github.com/opennetworkinglab/OpenNetworkLinux/releases/download/onlpv2-dev-1.0.0/onlp-dev_1.0.0_amd64.tar.gz"
SHA = "fe74e16c5a74d446cfb83a07082c0cf13406bb39f006ad6ae7d2fcdc2de8772e"
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _impl(repository_ctx):
    if "ONLP_INSTALL" not in repository_ctx.os.environ:
        # Download prebuild version of ONLP library if ONLP_INSTALL not exists
        repository_ctx.download_and_extract(url=ONLP_URL,
                                            output="onlp-bin",
                                            sha256=SHA,
                                            stripPrefix="onlp-dev_1.0.0_amd64")
    else:
        onlp_path = repository_ctx.os.environ["ONLP_INSTALL"]
        repository_ctx.symlink(onlp_path, "onlp-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "onlp_headers",
    hdrs = glob(["onlp-bin/include/**/*.h", "onlp-bin/include/**/*.x"]),
    includes = ["onlp-bin/include"],
)
cc_import(
  name = "onlp",
  hdrs = [],  # see cc_library rule above
  shared_library = "onlp-bin/lib/libonlp.so",
  # If alwayslink is turned on, libonlp.so will be forcely linked
  # into any binary that depends on it.
      alwayslink = 1,
)
cc_import(
  name = "onlp_platform",
  hdrs = [],  # see cc_library rule above
  shared_library = "onlp-bin/lib/libonlp-platform.so",
  # If alwayslink is turned on, libonlp-platform.so will be forcely linked
  # into any binary that depends on it.
  alwayslink = 1,
)
cc_import(
  name = "onlp_platform_defaults",
  hdrs = [],  # see cc_library rule above
  shared_library = "onlp-bin/lib/libonlp-platform-defaults.so",
  # If alwayslink is turned on, libonlp-platform-defaults.so will be forcely linked
  # into any binary that depends on it.
  alwayslink = 1,
)
""")

onlp_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["ONLP_INSTALL"])
