# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

"""Load dependencies needed for Stratum."""

load(
    "@bazel_tools//tools/build_defs/repo:git.bzl",
    "git_repository",
)
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//bazel:workspace_rule.bzl", "remote_workspace")

P4RUNTIME_VER = "1.4.0-rc.5"
P4RUNTIME_SHA = "ba31fb9afce6e62ffe565b16bb909e144cd30d65d926cd90af25e99ee8de863a"

GNMI_COMMIT = "39cb2fffed5c9a84970bde47b3d39c8c716dc17a"
GNMI_SHA = "3701005f28044065608322c179625c8898beadb80c89096b3d8aae1fbac15108"

TAI_COMMIT = "9a673b7310b29c97237b3066a96ea2e43e236cf3"
TAI_SHA = "6c3562906be3a3608f2e0e26c407d6ba4cbc4b587f87b99d811c8530e74edfca"

GNOI_COMMIT = "437c62e630389aa4547b4f0521d0bca3fb2bf811"
GNOI_SHA = "77d8c271adc22f94a18a5261c28f209370e87a5e615801a4e7e0d09f06da531f"

def stratum_deps():
    # -----------------------------------------------------------------------------
    #        Protobuf + gRPC compiler and external models
    # -----------------------------------------------------------------------------

    if "com_github_grpc_grpc" not in native.existing_rules():
        http_archive(
            name = "com_github_grpc_grpc",
            urls = ["https://github.com/grpc/grpc/archive/v1.40.0.tar.gz"],
            strip_prefix = "grpc-1.40.0",
            sha256 = "13e7c6460cd979726e5b3b129bb01c34532f115883ac696a75eb7f1d6a9765ed",
        )

    if "com_google_googleapis" not in native.existing_rules():
        http_archive(
            name = "com_google_googleapis",
            urls = ["https://github.com/googleapis/googleapis/archive/9b1c49de24301ba6bf1ee6462a634fffc2b97677.zip"],
            strip_prefix = "googleapis-9b1c49de24301ba6bf1ee6462a634fffc2b97677",
            sha256 = "2b10a2fe30a0ab4279d803ed7b3bfefb61c48fb3aa651e5f2d4899b4167b7f3b",
        )

    if "com_github_p4lang_p4c" not in native.existing_rules():
        # ----- p4c -----
        remote_workspace(
            name = "com_github_p4lang_p4c",
            remote = "https://github.com/p4lang/p4c",
            commit = "94e55783733be7420b8d8fd7bfc0025a3ad9033a",
            build_file = "@//bazel:external/p4c.BUILD",
            sha256 = "541ab66df80465dac9702779b6446b80234210410e6f5948d995a978475b64c2",
        )

    if "judy" not in native.existing_rules():
        # TODO(Yi): add judy.BUILD to bazel/external/ instead depending on specific one
        http_archive(
            name = "judy",
            build_file = "@com_github_p4lang_PI//bazel/external:judy.BUILD",
            url = "http://archive.ubuntu.com/ubuntu/pool/universe/j/judy/judy_1.0.5.orig.tar.gz",
            strip_prefix = "judy-1.0.5",
            sha256 = "d2704089f85fdb6f2cd7e77be21170ced4b4375c03ef1ad4cf1075bd414a63eb",
        )

    if "com_github_p4lang_p4runtime" not in native.existing_rules():
        http_archive(
            name = "com_github_p4lang_p4runtime",
            urls = ["https://github.com/p4lang/p4runtime/archive/v%s.zip" % P4RUNTIME_VER],
            strip_prefix = "p4runtime-%s/proto" % P4RUNTIME_VER,
            sha256 = P4RUNTIME_SHA,
        )

    if "build_stack_rules_proto" not in native.existing_rules():
        remote_workspace(
            name = "build_stack_rules_proto",
            remote = "https://github.com/stackb/rules_proto",
            commit = "2f4e4f62a3d7a43654d69533faa0652e1c4f5082",
        )

    if "com_github_p4lang_PI" not in native.existing_rules():
        # ----- PI -----
        remote_workspace(
            name = "com_github_p4lang_PI",
            remote = "https://github.com/p4lang/PI.git",
            commit = "a5fd855d4b3293e23816ef6154e83dc6621aed6a",
            sha256 = "7df38438f94d64c5005b890210d3f1b40e2402870295e21d44cceac67ebd1a1b",
        )

    if "com_github_p4lang_PI_np4" not in native.existing_rules():
        # ----- PI for Netcope targets -----
        remote_workspace(
            name = "com_github_p4lang_PI_np4",
            remote = "https://github.com/craigsdell/PI.git",
            commit = "12be7a96f3d903afdd6cc3095f7d4003242af60b",
            sha256 = "696bd1f01133e85cc83125ac747f53f67a519208cab3c7ddaa1d131ee0cea65c",
        )

    if "com_github_openconfig_gnmi_proto" not in native.existing_rules():
        http_archive(
            name = "com_github_openconfig_gnmi_proto",
            urls = ["https://github.com/bocon13/gnmi/archive/%s.zip" % GNMI_COMMIT],
            sha256 = GNMI_SHA,
            strip_prefix = "gnmi-%s/proto" % GNMI_COMMIT,
            build_file = "@//bazel:external/gnmi.BUILD",
        )

    if "com_github_openconfig_gnoi" not in native.existing_rules():
        http_archive(
            name = "com_github_openconfig_gnoi",
            urls = ["https://github.com/openconfig/gnoi/archive/%s.zip" % GNOI_COMMIT],
            strip_prefix = "gnoi-%s" % GNOI_COMMIT,
            build_file = "@//bazel:external/gnoi.BUILD",
            sha256 = GNOI_SHA,
            patch_cmds = [
                "find . -name *.proto | xargs sed -i'' -e 's#github.com/openconfig/##g'",
                "mkdir -p gnoi",
                "mv bgp cert common diag file interface layer2 mpls otdr system test types wavelength_router gnoi/",
            ],
        )

    if "com_github_openconfig_public" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_public",
            remote = "https://github.com/openconfig/public",
            commit = "624655d053ad1fdda62901c7e2055c22cd5d6a05",
            build_file = "@//bazel:external/ocpublic.BUILD",
            sha256 = "d9529e43065491b61ce5fdeaf38c0db10a8407cb9f1c4cd23563e5bbe28871f5",
        )

    if "com_github_openconfig_hercules" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_hercules",
            remote = "https://github.com/openconfig/hercules",
            commit = "ca3575e85500fa089dfe0b8cd3ea71943267102e",
            build_file = "@//bazel:external/hercules.BUILD",
            sha256 = "48cc536bc95f363f54aa32ececc24d03e0ab7d97972ab33cf67e63e430883bf8",
        )

    if "com_github_yang_models_yang" not in native.existing_rules():
        remote_workspace(
            name = "com_github_yang_models_yang",
            remote = "https://github.com/YangModels/yang",
            commit = "ed2ce1028ff57d667764dbdbe3c37328820f0e50",
            build_file = "@//bazel:external/yang.BUILD",
            sha256 = "53ba8dd265bff6d3cff108ea44493b3e7cf52c62bc089839e96d4329d2874d95",
        )

    if "com_github_nlohmann_json" not in native.existing_rules():
        http_archive(
            name = "com_github_nlohmann_json",
            url = "https://github.com/nlohmann/json/releases/download/v3.10.4/include.zip",
            sha256 = "62c585468054e2d8e7c2759c0d990fd339d13be988577699366fe195162d16cb",
            build_file = "@//bazel:external/json.BUILD",
        )

    # -----------------------------------------------------------------------------
    #        TAI library
    # -----------------------------------------------------------------------------
    if "com_github_telecominfraproject_oopt_tai_taish" not in native.existing_rules():
        http_archive(
            name = "com_github_telecominfraproject_oopt_tai_taish",
            urls = ["https://github.com/Telecominfraproject/oopt-tai/archive/%s.zip" % TAI_COMMIT],
            sha256 = TAI_SHA,
            strip_prefix = "oopt-tai-%s/tools/taish/proto/" % TAI_COMMIT,
            build_file = "@//bazel:external/taish_proto.BUILD",
        )

    # -----------------------------------------------------------------------------
    #        Third party C++ libraries for common
    # -----------------------------------------------------------------------------
    if "com_google_absl" not in native.existing_rules():
        http_archive(
            name = "com_google_absl",
            urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.0.tar.gz"],
            strip_prefix = "abseil-cpp-20220623.0",
            sha256 = "4208129b49006089ba1d6710845a45e31c59b0ab6bff9e5788a87f55c5abd602",
        )

    if "com_github_google_glog" not in native.existing_rules():
        http_archive(
            name = "com_github_google_glog",
            sha256 = "9826ccc86e70f1f1710fc1bb5ba1dc807afa6d3eac1cd694b9dd374761bccf59",
            strip_prefix = "glog-7bba6030c2a0e78c2f169a8a1cf37d899196f053",
            urls = ["https://github.com/google/glog/archive/7bba6030c2a0e78c2f169a8a1cf37d899196f053.zip"],
        )

    if "com_github_gflags_gflags" not in native.existing_rules():
        http_archive(
            name = "com_github_gflags_gflags",
            sha256 = "cfdba0f2f17e8b1ff75c98113d5080d8ec016148426abcc19130864e2952d7bd",
            strip_prefix = "gflags-827c769e5fc98e0f2a34c47cef953cc6328abced",
            urls = ["https://github.com/gflags/gflags/archive/827c769e5fc98e0f2a34c47cef953cc6328abced.zip"],
        )

    if "com_google_googletest" not in native.existing_rules():
        http_archive(
            name = "com_google_googletest",
            sha256 = "d3d307a240e129bb57da8aae64f3b0099bf1b8efff7249df993b619b8641ec77",
            strip_prefix = "googletest-a3460d1aeeaa43fdf137a6adefef10ba0b59fe4b",
            urls = ["https://github.com/google/googletest/archive/a3460d1aeeaa43fdf137a6adefef10ba0b59fe4b.zip"],
        )

    if "com_googlesource_code_re2" not in native.existing_rules():
        remote_workspace(
            name = "com_googlesource_code_re2",
            remote = "https://github.com/google/re2",
            commit = "be0e1305d264b2cbe1d35db66b8c5107fc2a727e",
            sha256 = "4f94f422c14aea5419970f4399ac15b2148bc2e90c8566b9de45c6cf3ff6ce53",
        )

    if "com_github_systemd_systemd" not in native.existing_rules():
        remote_workspace(
            name = "com_github_systemd_systemd",
            remote = "https://github.com/systemd/systemd",
            commit = "06e93130b4045db1c75f8de506d2447642de74cf",
            build_file = "@//bazel:external/systemd.BUILD",
            sha256 = "1a02064429ca3995558abd118d3dda06571169b7a6d5e2f3289935967c929a45",
        )

    if "com_github_nelhage_rules_boost" not in native.existing_rules():
        git_repository(
            name = "com_github_nelhage_rules_boost",
            commit = "ed844db5990d21b75dc3553c057069f324b3916b",
            remote = "https://github.com/nelhage/rules_boost",
            shallow_since = "1570056263 -0700",
        )

    if "com_github_jbeder_yaml_cpp" not in native.existing_rules():
        git_repository(
            name = "com_github_jbeder_yaml_cpp",
            remote = "https://github.com/jbeder/yaml-cpp.git",
            commit = "a6bbe0e50ac4074f0b9b44188c28cf00caf1a723",
            shallow_since = "1609854028 -0600",
        )

    # -----------------------------------------------------------------------------
    #      Golang specific libraries.
    # -----------------------------------------------------------------------------
    if "bazel_latex" not in native.existing_rules():
        http_archive(
            name = "bazel_latex",
            sha256 = "66ca4240628a4e40cc02d7f77f06b93269dad0068e7a844009fd439e5c55f5a9",
            strip_prefix = "bazel-latex-0.17",
            url = "https://github.com/ProdriveTechnologies/bazel-latex/archive/v0.17.tar.gz",
        )

    # -----------------------------------------------------------------------------
    #        Chipset and Platform specific C/C++ libraries
    # -----------------------------------------------------------------------------
    if "com_github_opennetworkinglab_sdklt" not in native.existing_rules():
        http_archive(
            name = "com_github_opennetworkinglab_sdklt",
            sha256 = "dfe9d73fd52ad7f064837ccab4ef64effffa88a65b16dcbf8048d07c0a349de9",
            urls = ["https://github.com/opennetworkinglab/SDKLT/releases/download/r148/sdklt-4.19.0.tgz"],
            build_file = "@//bazel:external/sdklt.BUILD",
        )

    if "com_github_broadcom_opennsa" not in native.existing_rules():
        http_archive(
            name = "com_github_broadcom_opennsa",
            sha256 = "261a440454015122fbf9ac4cccf018b1c358a641d80690be1f1e972b6265d45c",
            urls = ["https://docs.broadcom.com/docs-and-downloads/csg/opennsa-6.5.19.1.tgz"],
            strip_prefix = "opennsa-6.5.19.1",
            build_file = "@//bazel:external/openNSA.BUILD",
            # TODO(max): This is kind of hacky and should be improved.
            # Each string is a new bash shell, use && to run dependant commands.
            patch_cmds = [
                "wget -qO- https://github.com/stratum/sonic-base-image/releases/download/2022-08-12/linux-headers-4.19.0-12-2-merged.tar.xz | tar xJ",
                "export CC=gcc CXX=g++ CFLAGS='-Wno-error=unused-result -fno-pie' KERNDIR=$(realpath ./linux-headers-4.19.0-12-2-merged) && cd src/gpl-modules/systems/linux/user/x86-smp_generic_64-2_6 && make clean -j && make",
                "rm -rf ./linux-headers-4.19.0-12-2-merged",
            ],
        )

    # TODO(max): change name to `com_github_p4lang_p4_dpdk_target`
    # if "com_github_p4lang_p4_dpdk_target" not in native.existing_rules():
    if "local_tdi_bin" not in native.existing_rules():
        http_archive(
            # name = "com_github_p4lang_p4_dpdk_target",
            name = "local_tdi_bin",
            sha256 = "c58e7a3f13b12515bbcf0f784486125b9c605e54ca93fd920b262d18dbdac0cb",
            # TODO(max): host file somewhere more appropriate
            urls = ["https://github.com/stratum/sonic-base-image/releases/download/2022-08-12/p4-sde-0.1.0-install.tgz"],
            build_file = "@//bazel:external/dpdk.BUILD",
        )

    # -----------------------------------------------------------------------------
    #        P4 testing modules
    # -----------------------------------------------------------------------------

    if "com_github_opennetworkinglab_fabric_p4test" not in native.existing_rules():
        remote_workspace(
            name = "com_github_opennetworkinglab_fabric_p4test",
            remote = "https://github.com/opennetworkinglab/fabric-p4test",
            commit = "ac2b0bf26c4fb91d883492cb85394304cde392c6",
        )

    # -----------------------------------------------------------------------------
    #        Packaging tools
    # -----------------------------------------------------------------------------
    if "rules_pkg" not in native.existing_rules():
        http_archive(
            name = "rules_pkg",
            urls = [
                "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
                "https://github.com/bazelbuild/rules_pkg/releases/download/0.4.0/rules_pkg-0.4.0.tar.gz",
            ],
            sha256 = "038f1caa773a7e35b3663865ffb003169c6a71dc995e39bf4815792f385d837d",
        )
