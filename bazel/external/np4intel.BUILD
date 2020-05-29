# Copyright 2020-present Dell EMC
# SPDX-License-Identifier: Apache-2.0

load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

# trick to export headers in a convenient way
cc_library(
    name = "np4_headers",
    hdrs = glob([
        "netcope-bin/include/np4/*.hpp",
        "netcope-bin/include/np4/p4*.h",
    ]),
    includes = ["netcope-bin/include"],
)

cc_import(
    name = "np4_atom",
    hdrs = [],  # see cc_library rule above
    shared_library = "netcope-bin/lib/libnp4atom.so",
)
