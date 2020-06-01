# Copyright 2020-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

# trick to export headers in a convenient way
cc_library(
    name = "bmv2_headers",
    hdrs = glob([
        "bmv2-bin/include/bm/**/*.h",
        "bmv2-bin/include/bm/**/*.cc",
    ]),
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
