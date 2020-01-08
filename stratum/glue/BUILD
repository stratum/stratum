#
# Copyright 2018 Google LLC
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

licenses(["notice"])  # Apache v2

load("//bazel:defs.bzl", "STRATUM_INTERNAL")
load("//bazel/rules:library_rule.bzl", "stratum_cc_library")

package(
    #default_hdrs_check = "strict",
    default_visibility = STRATUM_INTERNAL,
)

# NOTE: All dependencies on @com_github_google_glog//:glog should be via the glue directory so that we
# can maintain flexible dependencies on portable google3.

# There are sections of code under status/ which is guarded by HERCULES. This
# target defines this.
stratum_cc_library(
    name = "define_hercules",
    defines = [
        "HERCULES",
    ],
)

stratum_cc_library(
    name = "init_google",
    hdrs = [
        "init_google.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)

stratum_cc_library(
    name = "logging",
    srcs = [
        "logging.cc",
    ],
    hdrs = [
        "logging.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
    ],
)

stratum_cc_library(
    name = "integral_types",
    hdrs = [
        "integral_types.h",
    ],
)
