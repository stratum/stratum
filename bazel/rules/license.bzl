# Copyright 2021-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@rules_pkg//:pkg.bzl", "pkg_tar")

# Creates a tarball containing an amalgamated NOTICE and LICENSE file from the given files.
# See pkg_tar for all arguments.
def stratum_license_tar(
        name,
        stratum_notice = "//:NOTICE",
        dep_notices = ["//legal:NOTICE.common.txt"],
        extra_licenses = [],
        **kwargs):
    native.genrule(
        name = "notice_file_" + name,
        cmd_bash = "awk 1 $(SRCS) > $@",
        srcs = dep_notices,
        outs = [name + "/NOTICE"],
    )

    native.genrule(
        name = "license_file_" + name,
        cmd_bash = "awk 1 $(SRCS) > $@",
        srcs = [stratum_notice] + extra_licenses,
        outs = [name + "/LICENSE"],
    )

    pkg_tar(
        name = name,
        srcs = [
            ":notice_file_" + name,
            ":license_file_" + name,
        ],
        strip_prefix = name,
        **kwargs
    )
