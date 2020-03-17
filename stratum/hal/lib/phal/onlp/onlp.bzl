#
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This Skylark rule imports the onlp shared libraries and headers
# The ONLP_INSTALL environment variable needs to be set
# otherwise will download the prebuit libraries

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
ONLP_URL = "https://github.com/opennetworkinglab/OpenNetworkLinux/releases/download/onlpv2-dev-1.1.0/onlp-dev_1.1.0_amd64.tar.gz"
SHA = "bd359f2429368e5645b4d4a77fe7c95fb0702e753f04fee48690adad653c6503"

# When using cc_import, Bazel links against the SONAME versions of the library,
# even if they are not available (see
# https://github.com/bazelbuild/bazel/issues/1534). The common workaround is to
# use cc_library instead and pass all library names (see
# https://github.com/tweag/rules_nixpkgs/issues/60). Because the SONAME version
# does not even exist in the default ONLP tarball we download, we create
# symbolic links for those.

def _impl(repository_ctx):
    if "ONLP_INSTALL" not in repository_ctx.os.environ:
        # Download prebuild version of ONLP library if ONLP_INSTALL not defined
        repository_ctx.download_and_extract(url=ONLP_URL,
                                            output="onlp-bin",
                                            sha256=SHA,
                                            stripPrefix="onlp-dev_1.1.0_amd64")
        repository_ctx.symlink("onlp-bin/lib/libonlp.so",
                               "onlp-bin/lib/libonlp.so.1")
        repository_ctx.symlink("onlp-bin/lib/libonlp-platform.so",
                               "onlp-bin/lib/libonlp-platform.so.1")
        repository_ctx.symlink("onlp-bin/lib/libonlp-platform-defaults.so",
                               "onlp-bin/lib/libonlp-platform-defaults.so.1")
        repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "onlp_headers",
    hdrs = glob(["onlp-bin/include/**/*.h", "onlp-bin/include/**/*.x"]),
    includes = ["onlp-bin/include"],
)
cc_library(
    name = "onlp",
    hdrs = [],  # see cc_library rule above
    srcs = ["onlp-bin/lib/libonlp.so",
            "onlp-bin/lib/libonlp.so.1"],
)
cc_library(
    name = "onlp_platform",
    hdrs = [],  # see cc_library rule above
    srcs = ["onlp-bin/lib/libonlp-platform.so",
            "onlp-bin/lib/libonlp-platform.so.1"],
)
cc_library(
    name = "onlp_platform_defaults",
    hdrs = [],  # see cc_library rule above
    srcs = ["onlp-bin/lib/libonlp-platform-defaults.so",
            "onlp-bin/lib/libonlp-platform-defaults.so.1"],
)
""")
    else:  # ONLP_INSTALL env variable is defined
        onlp_path = repository_ctx.os.environ["ONLP_INSTALL"]
        repository_ctx.symlink(onlp_path, "onlp-bin")
        repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)

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
    alwayslink = 1,
)
cc_import(
    name = "onlp_platform_defaults",
    hdrs = [],  # see cc_library rule above
    shared_library = "onlp-bin/lib/libonlp-platform-defaults.so",
    alwayslink = 1,
)
""")

onlp_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["ONLP_INSTALL"])
