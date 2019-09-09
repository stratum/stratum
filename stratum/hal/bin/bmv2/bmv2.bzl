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

# This Skylark rule imports the bmv2 shared libraries and headers since there is
# not yet any native support for Bazel in bmv2. The BMV2_INSTALL environment
# variable needs to be set, otherwise the Stratum rules which depend on bmv2
# cannot be built.

def _impl(repository_ctx):
    if "BMV2_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    bmv2_path = repository_ctx.os.environ["BMV2_INSTALL"]
    repository_ctx.symlink(bmv2_path, "bmv2-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
# trick to export headers in a convenient way
cc_library(
    name = "bmv2_headers",
    hdrs = glob(["bmv2-bin/include/bm/**/*.h", "bmv2-bin/include/bm/**/*.cc"]),
    includes = ["bmv2-bin/include"],
)
cc_import(
  name = "bmv2_simple_switch",
  hdrs = [],  # see cc_library rule above
  shared_library = "bmv2-bin/lib/libsimpleswitch_runner.so",
  # If alwayslink is turned on, libsimpleswitch_runner.so will be forcely linked
  # into any binary that depends on it.
  alwayslink = 1,
)
cc_import(
  name = "bmv2_pi",
  hdrs = [],  # see cc_library rule above
  shared_library = "bmv2-bin/lib/libbmpi.so",
  alwayslink = 1,
)
""")

bmv2_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["BMV2_INSTALL"])
