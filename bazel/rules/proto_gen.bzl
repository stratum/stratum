# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

load("@bazel_gazelle//:deps.bzl", "go_repository")
load("//bazel:workspace_rule.bzl", "remote_workspace")

def proto_gen_deps():
    if "com_github_openconfig_gnmi" not in native.existing_rules():
        go_repository(
            name = "com_github_openconfig_gnmi",
            importpath = "github.com/openconfig/gnmi",
            remote = "https://github.com/bocon13/gnmi",
            commit = "39cb2fffed5c9a84970bde47b3d39c8c716dc17a",
            patch_cmds = [
                "sed -i.bak 's#//gnmi_ext#//proto/gnmi_ext#g' proto/gnmi/BUILD.bazel",
                "sed -i.bak 's#import \"gnmi_ext#import \"proto/gnmi_ext#g' proto/gnmi/gnmi.proto",
            ],
            vcs = "git",
        )
    if "com_github_openconfig_goyang" not in native.existing_rules():
        go_repository(
            name = "com_github_openconfig_goyang",
            remote = "https://github.com/openconfig/goyang",
            commit = "e8b0ed2cbb0c40683bc0785ea2c796b2c12df80f",
            importpath = "github.com/openconfig/goyang",
            vcs = "git"
        )
    if "com_github_golang_glog" not in native.existing_rules():
        go_repository(
            name = "com_github_golang_glog",
            remote = "https://github.com/golang/glog",
            commit = "23def4e6c14b4da8ac2ed8007337bc5eb5007998",
            importpath = "github.com/golang/glog",
            vcs = "git"
        )
    if "com_github_kylelemons_godebug" not in native.existing_rules():
        go_repository(
            name = "com_github_kylelemons_godebug",
            remote = "https://github.com/kylelemons/godebug",
            commit = "9ff306d4fbead574800b66369df5b6144732d58e",
            importpath = "github.com/kylelemons/godebug",
            vcs = "git"
        )

    if "com_github_openconfig_ygot" not in native.existing_rules():
        go_repository(
            name = "com_github_openconfig_ygot",
            remote = "https://github.com/openconfig/ygot",
            commit = "68346f97239f91ac9fb1f419586f58d6c39f5500",
            importpath = "github.com/openconfig/ygot",
            vcs = "git",
            patches = [
                "//bazel/patches:ygot.patch"
            ],
            patch_args = ["-p1"]
        )
    if "com_github_openconfig_ygot_proto" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_ygot_proto",
            remote = "https://github.com/openconfig/ygot",
            commit = "68346f97239f91ac9fb1f419586f58d6c39f5500",
            build_file = "@//bazel:external/ygot_proto.BUILD",
        )
