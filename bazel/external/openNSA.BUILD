#
# Copyright 2019-present Open Networking Foundation
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

licenses(["notice"])  # Apache v2

package(
    default_visibility = [ "//visibility:public" ],
)

cc_library(
    name = "headers",
    hdrs = glob([
        "include/**/*.h",
        "src/gpl-modules/include/**/*.h",
        "src/gpl-modules/systems/bde/linux/include/**/*.h",
    ]),
    includes = [
        "include",
        "src/gpl-modules/include",
        "src/gpl-modules/systems/bde/linux/include",
    ],
)

cc_library(
    name = "libsdk",
    srcs = [
        "lib/x86-64/libopennsa.a",
        "src/diag/version.c",
        "src/diag/config_init_defaults.c",
    ],
    linkopts = [
        "-lpthread",
        "-lm",
        "-lrt",
    ],
    deps = [":headers"],
    linkstatic = True,
)
