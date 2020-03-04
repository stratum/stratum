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

"""Load dependencies needed for Stratum."""

load("//bazel:workspace_rule.bzl", "remote_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl",
     "git_repository",
     "new_git_repository")
load("@bazel_gazelle//:deps.bzl", "go_repository")

P4RUNTIME_VER = "1.1.0-rc.1"
P4RUNTIME_SHA = "fb4eb0767ea9e9697b2359be6979942c54abf64187a2d0f5ff61f227500ec195"

GNMI_COMMIT = "39cb2fffed5c9a84970bde47b3d39c8c716dc17a";
GNMI_SHA = "3701005f28044065608322c179625c8898beadb80c89096b3d8aae1fbac15108"

BF_SDE_PI_VER = {
    "8_9_2": "aa1f4f338008e48877f7dc407244a4d018a8fb7b",
    "9_0_0": "ca0291420b5b47fa2596a00877d1713aab61dc7a",
    "9_1_0": "41358da0ff32c94fa13179b9cee0ab597c9ccbcc",
}

def stratum_deps():
# -----------------------------------------------------------------------------
#        Protobuf + gRPC compiler and external models
# -----------------------------------------------------------------------------
    if "com_google_protobuf" not in native.existing_rules():
        remote_workspace(
            name = "com_google_protobuf",
            remote = "https://github.com/google/protobuf",
            tag = "3.7.1",
        )

    if "com_github_grpc_grpc" not in native.existing_rules():
        remote_workspace(
            name = "com_github_grpc_grpc",
            remote = "https://github.com/grpc/grpc",
            tag = "1.21.3",
            patches = ["@//bazel/patches:grpc.patch"],
            patch_args = ["-p1"],
        )

    if "com_google_googleapis" not in native.existing_rules():
        remote_workspace(
            name = "com_google_googleapis",
            remote = "https://github.com/googleapis/googleapis",
            commit = "84c8ad4e52f8eec8f08a60636cfa597b86969b5c",
        )

    if "com_github_p4lang_p4c" not in native.existing_rules():
        # ----- p4c -----
        remote_workspace(
            name = "com_github_p4lang_p4c",
            remote = "https://github.com/p4lang/p4c",
            commit = "43568b75796d68a6424ad22eebeee62f46ccd3fe",
            build_file = "@//bazel:external/p4c.BUILD",
        )

    if "judy" not in native.existing_rules():
        # TODO(Yi): add judy.BUILD to bazel/external/ instead depending on specific one
        http_archive(
            name = "judy",
            build_file = "@com_github_p4lang_PI//bazel/external:judy.BUILD",
            url = "http://archive.ubuntu.com/ubuntu/pool/universe/j/judy/judy_1.0.5.orig.tar.gz",
            strip_prefix = "judy-1.0.5",
        )

    if "com_github_p4lang_p4runtime" not in native.existing_rules():
        http_archive(
            name = "com_github_p4lang_p4runtime",
            urls = ["https://github.com/p4lang/p4runtime/archive/v%s.zip" % P4RUNTIME_VER],
            sha256 = P4RUNTIME_SHA,
            strip_prefix = "p4runtime-%s/proto" % P4RUNTIME_VER,
            build_file = "@//bazel:external/p4runtime.BUILD",
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
            commit = "1539ecd8a50c159b011d9c5a9c0eba99f122a845",
        )

    for sde_ver in BF_SDE_PI_VER:
        dep_name = "com_github_p4lang_PI_bf_" + sde_ver
        pi_commit = BF_SDE_PI_VER[sde_ver]
        if dep_name not in native.existing_rules():
                # ----- PI for Barefoot targets -----
                remote_workspace(
                    name = dep_name,
                    remote = "https://github.com/p4lang/PI.git",
                    commit = pi_commit,
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
        remote_workspace(
            name = "com_github_openconfig_gnoi",
            remote = "https://github.com/openconfig/gnoi",
            commit = "437c62e630389aa4547b4f0521d0bca3fb2bf811",
            build_file = "@//bazel:external/gnoi.BUILD",
        )

    if "io_bazel_rules_python" not in native.existing_rules():
        remote_workspace(
            name = "io_bazel_rules_python",
            commit = "8b5d0683a7d878b28fffe464779c8a53659fc645",
            remote = "https://github.com/bazelbuild/rules_python.git",
        )
    if "cython" not in native.existing_rules():
        http_archive(
            name = "cython",
            build_file = "@com_github_grpc_grpc//third_party:cython.BUILD",
            sha256 = "d68138a2381afbdd0876c3cb2a22389043fa01c4badede1228ee073032b07a27",
            strip_prefix = "cython-c2b80d87658a8525ce091cbe146cb7eaa29fed5c",
            urls = [
                "https://github.com/cython/cython/archive/c2b80d87658a8525ce091cbe146cb7eaa29fed5c.tar.gz",
            ],
        )
    if "com_github_openconfig_public" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_public",
            remote = "https://github.com/openconfig/public",
            commit = "5897507ecdb54453d4457e7dbb0a3d4b7ead4314",
            build_file = "@//bazel:external/ocpublic.BUILD",
        )

    if "com_github_openconfig_hercules" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_hercules",
            remote = "https://github.com/openconfig/hercules",
            commit = "cd48feeaaa54426df561d8c961d18d344365998b",
            build_file = "@//bazel:external/hercules.BUILD",
        )

    if "com_github_yang_models_yang" not in native.existing_rules():
        remote_workspace(
            name = "com_github_yang_models_yang",
            remote = "https://github.com/YangModels/yang",
            commit = "31daa2507ae507776c23b4d4176b6cdcef2a308c",
            build_file = "@//bazel:external/yang.BUILD",
        )

# -----------------------------------------------------------------------------
#        Third party C++ libraries for common
# -----------------------------------------------------------------------------
    if "com_google_absl" not in native.existing_rules():
        remote_workspace(
            name = "com_google_absl",
            remote = "https://github.com/abseil/abseil-cpp",
            branch = "lts_2019_08_08",
        )

    if "com_googlesource_code_cctz" not in native.existing_rules():
        # CCTZ (Time-zone framework); required for Abseil time
        remote_workspace(
            name = "com_googlesource_code_cctz",
            remote = "https://github.com/google/cctz",
            commit = "b4935eef53820cf1643355bb15e013b4167a2867",
        )

    if "com_github_google_glog" not in native.existing_rules():
        remote_workspace(
            name = "com_github_google_glog",
            remote = "https://github.com/google/glog",
            commit = "ba8a9f6952d04d1403b97df24e6836227751454e",
        )

    if "com_github_gflags_gflags" not in native.existing_rules():
        remote_workspace(
            name = "com_github_gflags_gflags",
            remote = "https://github.com/gflags/gflags",
            commit = "28f50e0fed19872e0fd50dd23ce2ee8cd759338e",
        )

    if "com_google_googletest" not in native.existing_rules():
        remote_workspace(
            name = "com_google_googletest",
            remote = "https://github.com/google/googletest",
            branch = "3525e3984282c827c7207245b1d4a47f4eaf3c91",
        )

    if "com_googlesource_code_re2" not in native.existing_rules():
        remote_workspace(
            name = "com_googlesource_code_re2",
            remote = "https://github.com/google/re2",
            commit = "be0e1305d264b2cbe1d35db66b8c5107fc2a727e",
        )

    if "com_github_systemd_systemd" not in native.existing_rules():
        remote_workspace(
            name = "com_github_systemd_systemd",
            remote = "https://github.com/systemd/systemd",
            commit = "06e93130b4045db1c75f8de506d2447642de74cf",
            build_file = "@//bazel:external/systemd.BUILD",
        )

    if "boringssl" not in native.existing_rules():
        remote_workspace(
            name = "boringssl",
            remote = "https://github.com/google/boringssl",
            branch = "chromium-stable-with-bazel",
            #commit = "90bd81032325ba659e538556e64977c29df32a3c", or afc30d43eef92979b05776ec0963c9cede5fb80f
        )

    if "com_github_nelhage_rules_boost" not in native.existing_rules():
        remote_workspace(
            name = "com_github_nelhage_rules_boost",
            remote = "https://github.com/nelhage/rules_boost",
            commit = "a3b25bf1a854ca7245d5786fda4821df77c57827",
        )

    if "rules_cc" not in native.existing_rules():
        remote_workspace(
            name = "rules_cc",
            remote = "https://github.com/bazelbuild/rules_cc",
            commit = "cfe68f6bc79dea602f2f6a767797f94a5904997f",
        )

    if "com_github_jbeder_yaml_cpp" not in native.existing_rules():
        git_repository(
            name = "com_github_jbeder_yaml_cpp",
            remote = "https://github.com/jbeder/yaml-cpp.git",
            commit = "de8253fcb075c049c4ad1c466c504bf3cf022f45",
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

    if "net_zlib" not in native.existing_rules():
        native.bind(
            name = "zlib",
            actual = "@net_zlib//:zlib",
        )
        http_archive(
            name = "net_zlib",
            build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
            sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
            strip_prefix = "zlib-1.2.11",
            urls = ["https://zlib.net/zlib-1.2.11.tar.gz"],
        )
    if "io_bazel_rules_go" not in native.existing_rules():
        remote_workspace(
            name = "io_bazel_rules_go",
            remote = "https://github.com/bazelbuild/rules_go",
            commit = "2eb16d80ca4b302f2600ffa4f9fc518a64df2908",
        )

    if "bazel_gazelle" not in native.existing_rules():
        remote_workspace(
            name = "bazel_gazelle",
            remote = "https://github.com/bazelbuild/bazel-gazelle",
            commit = "e443c54b396a236e0d3823f46c6a931e1c9939f2",
        )
# -----------------------------------------------------------------------------
#        Chipset and Platform specific C/C++ libraries
# -----------------------------------------------------------------------------
    if "com_github_opennetworkinglab_sdklt" not in native.existing_rules():
        http_archive(
            name = "com_github_opennetworkinglab_sdklt",
            sha256 = "38a59fe2db5122dd76fcbed234c68c59ccfdb68890199b4b891aeb86817713f4",
            urls = ["https://github.com/opennetworkinglab/SDKLT/releases/download/r69/sdklt-4.14.49.tgz"],
            build_file = "@//bazel:external/sdklt.BUILD",
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
            url = "https://github.com/bazelbuild/rules_pkg/releases/download/0.2.4/rules_pkg-0.2.4.tar.gz",
            sha256 = "4ba8f4ab0ff85f2484287ab06c0d871dcb31cc54d439457d28fd4ae14b18450a",
        )
