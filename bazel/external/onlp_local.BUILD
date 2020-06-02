# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "onlp_headers",
    hdrs = glob([
        "onlp-bin/include/**/*.h",
        "onlp-bin/include/**/*.x",
    ]),
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
