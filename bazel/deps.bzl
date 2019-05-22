"""Load dependencies needed for Stratum."""

load("//bazel:workspace_rule.bzl", "remote_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl",
     "git_repository",
     "new_git_repository")

def stratum_deps():
# -----------------------------------------------------------------------------
#        Protobuf + gRPC compiler and external models
# -----------------------------------------------------------------------------
    if "com_google_protobuf" not in native.existing_rules():
        remote_workspace(
            name = "com_google_protobuf",
            remote = "https://github.com/google/protobuf",
            tag = "3.6.1.3",
        )

    if "com_github_grpc_grpc" not in native.existing_rules():
        remote_workspace(
            name = "com_github_grpc_grpc",
            remote = "https://github.com/grpc/grpc",
            tag = "1.17.2",
        )
        # TODO this is a hack for the pubref rules
        #remote_workspace(
        #    name = "com_google_grpc",
        #    remote = "https://github.com/grpc/grpc",
        #    tag = "1.12.1",
        #)

    if "org_pubref_rules_protobuf" not in native.existing_rules():
        # ----- protoc w/ gRPC compiler -----
        #FIXME update to upstream when pull requests are merged
        remote_workspace(
            name = "org_pubref_rules_protobuf",
            # remote = "https://github.com/bocon13/rules_protobuf",
            # branch = "master",
            remote = "https://github.com/pubref/rules_protobuf",
            branch = "master",
        )

    if "com_github_googleapis" not in native.existing_rules():
        remote_workspace(
            name = "com_github_googleapis",
            remote = "https://github.com/googleapis/googleapis",
            commit = "a19256f36347fde5f2ab44e24e6e6c6b2a314041",
            build_file = "@//bazel:external/googleapis.BUILD",
        )

    if "com_github_p4lang_p4c" not in native.existing_rules():
        # ----- p4c -----
        remote_workspace(
            name = "com_github_p4lang_p4c",
            remote = "https://github.com/p4lang/p4c",
            commit = "b7664d8247d00ad68d49567c1195e0fbc5885389",
            build_file = "@//bazel:external/p4c.BUILD",
        )

    if "com_github_p4lang_p4runtime" not in native.existing_rules():
        # ----- P4 Runtime -----
        remote_workspace(
            name = "com_github_p4lang_p4runtime",
            remote = "https://github.com/p4lang/p4runtime",
            tag = "1.0.0",
            build_file = "@//bazel:external/p4runtime.BUILD",
        )

    if "com_github_p4lang_PI" not in native.existing_rules():
        # ----- PI -----
        remote_workspace(
            name = "com_github_p4lang_PI",
            remote = "https://github.com/p4lang/PI.git",
            commit = "f9a5c6c74f7dcde382e27c63af2fe5dffc755364",
        )

    if "com_github_openconfig_gnmi" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_gnmi",
            remote = "https://github.com/openconfig/gnmi",
            branch = "master",
            build_file = "@//bazel:external/gnmi.BUILD",
        )

    if "com_github_openconfig_gnoi" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_gnoi",
            remote = "https://github.com/bocon13/gnoi",
            branch = "no-bazel",
            build_file = "@//bazel:external/gnoi.BUILD",
        )

# -----------------------------------------------------------------------------
#        Third party C++ libraries for common
# -----------------------------------------------------------------------------
    if "com_google_absl" not in native.existing_rules():
        remote_workspace(
            name = "com_google_absl",
            remote = "https://github.com/abseil/abseil-cpp",
            branch = "master",
        )

    if "com_googlesource_code_cctz" not in native.existing_rules():
        # CCTZ (Time-zone framework); required for Abseil time
        remote_workspace(
            name = "com_googlesource_code_cctz",
            remote = "https://github.com/google/cctz",
            branch = "master",
        )

    if "com_github_google_glog" not in native.existing_rules():
        remote_workspace(
            name = "com_github_google_glog",
            remote = "https://github.com/google/glog",
            branch = "master",
        )

    if "com_github_gflags_gflags" not in native.existing_rules():
        remote_workspace(
            name = "com_github_gflags_gflags",
            remote = "https://github.com/gflags/gflags",
            branch = "master",
        )

    if "com_google_googletest" not in native.existing_rules():
        remote_workspace(
            name = "com_google_googletest",
            remote = "https://github.com/google/googletest",
            branch = "master",
        )

    if "com_googlesource_code_re2" not in native.existing_rules():
        remote_workspace(
            name = "com_googlesource_code_re2",
            remote = "https://github.com/google/re2",
            branch = "master",
        )

    if "com_github_systemd_systemd" not in native.existing_rules():
        remote_workspace(
            name = "com_github_systemd_systemd",
            remote = "https://github.com/systemd/systemd",
            branch = "master",
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

# -----------------------------------------------------------------------------
#      Golang specific libraries.
# -----------------------------------------------------------------------------
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
#        Chipset and Platform specific C++ libraries
# -----------------------------------------------------------------------------
    if "com_github_bcm_sdklt" not in native.existing_rules():
        remote_workspace(
            name = "com_github_bcm_sdklt",
            remote = "https://github.com/Broadcom-Network-Switching-Software/SDKLT",
            branch = "master",
            build_file = "@//bazel:external/sdklt.BUILD",
        )
