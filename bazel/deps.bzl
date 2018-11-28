"""Load dependencies needed for Stratum."""

load("//bazel:workspace_rule.bzl", "remote_workspace")

def stratum_deps():
# -----------------------------------------------------------------------------
#        Protobuf + gRPC compiler and external models
# -----------------------------------------------------------------------------
    if "com_google_protobuf" not in native.existing_rules():
        remote_workspace(
            name = "com_google_protobuf",
            remote = "https://github.com/google/protobuf",
            tag = "3.5.1",
        )

    if "com_github_grpc_grpc" not in native.existing_rules():
        remote_workspace(
            name = "com_github_grpc_grpc",
            remote = "https://github.com/grpc/grpc",
            tag = "1.12.1",
        )
        # TODO this is a hack for the pubref rules
        remote_workspace(
            name = "com_google_grpc",
            remote = "https://github.com/grpc/grpc",
            tag = "1.12.1",
        )

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
            build_file = "bazel/external/googleapis.BUILD",
        )

    if "com_github_p4lang_p4runtime" not in native.existing_rules():
        # ----- P4 Runtime -----
        remote_workspace(
            name = "com_github_p4lang_p4runtime",
            remote = "https://github.com/p4lang/p4runtime",
            tag = "1.0.0-rc3",
        )

    if "com_github_p4lang_PI" not in native.existing_rules():
        # ----- PI -----
        remote_workspace(
            name = "com_github_p4lang_PI",
            remote = "https://github.com/MaxPolovyi/PI.git",
            # PI update to p4runtime 1.0.0-rc3
            # commit = "08fa170106e1728b10491bb78c7543a950dc95ec",
            branch = "fix-bazel-build",
        )

    if "com_github_openconfig_gnmi" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_gnmi",
            remote = "https://github.com/openconfig/gnmi",
            branch = "master",
            build_file = "bazel/external/gnmi.BUILD",
        )
    
    if "com_github_openconfig_gnoi" not in native.existing_rules():
        remote_workspace(
            name = "com_github_openconfig_gnoi",
            remote = "https://github.com/bocon13/gnoi",
            branch = "no-bazel",
            build_file = "bazel/external/gnoi.BUILD",
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
            build_file = "bazel/external/systemd.BUILD",
        )
# -----------------------------------------------------------------------------
#        Chipset and Platform specific C++ libraries
# -----------------------------------------------------------------------------
    if "com_github_bcm_sdklt" not in native.existing_rules():
        remote_workspace(
            name = "com_github_bcm_sdklt",
            remote = "https://github.com/Broadcom-Network-Switching-Software/SDKLT",
            branch = "master",
            build_file = "bazel/external/sdklt.BUILD",
        )
