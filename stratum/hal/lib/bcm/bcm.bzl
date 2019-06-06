#
# Copyright 2018-present Open Networking Foundation
# Copyright 2019 Broadcom. All rights reserved. The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
    hdrs = glob(["bcm-bin/include/sdklt/**/*.h" ]),
    includes = ["bcm-bin/include/sdklt"],
)
cc_import(
  name = "bcm_sdklt",
  hdrs = [],  # see cc_library rule above
  #shared_library = "bcm-bin/lib/libsdklt.so",
  static_library = "bcm-bin/lib/libsdklt.a",
  alwayslink = 1,
)
""")

bcm_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["SDKLT_INSTALL"])
