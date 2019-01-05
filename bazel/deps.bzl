"""Load dependencies needed for Stratum."""

load("//bazel:workspace_rule.bzl", "remote_workspace")
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
            commit = "4646f1603499ebd2d42c6bf51274c18aa48943d3",
            build_file = "bazel/external/p4c.BUILD",
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
            commit = "8a8853fd755496288995a603ce9aa2685709cd39",
        )
# -----------------------------------------------------------------------------
#        Chipset and Platform specific C++ libraries
# -----------------------------------------------------------------------------
    if "com_github_opennetworklinux" not in native.existing_rules():
        # For ONLPv2 headers
        new_git_repository(
            name = "com_github_opennetworklinux",
            remote = "https://github.com/opennetworkinglab/OpenNetworkLinux.git",
            commit = "8e4ca6b5d07a2dd6296daf6aa652fd12d7e31f48", # ONLPv2 branch on 11/8/2018
            build_file = "@//bazel:external/onlp.BUILD",
            init_submodules = True,
            patch_cmds = [
                # Remove reference to dependmodules.x include (normally generated by ONLP build system)
                "sed -i.bak '/dependmodules.x/d' sm/bigcode/modules/sff/module/inc/sff/sff.h",
                "rm sm/bigcode/modules/sff/module/inc/sff/sff.h.bak",
                "sed -i.bak '/dependmodules.x/d' sm/bigcode/modules/uCli/module/src/ucli_handlers.c",
                "rm sm/bigcode/modules/uCli/module/src/ucli_handlers.c.bak",
                # Replace i2c-devices with i2c-dev header file in includes
                "sed -i.bak 's#<linux/i2c-devices.h>#<linux/i2c-dev.h>#'" +
                    " packages/base/any/onlp/src/onlplib/module/src/i2c.c",
                "rm packages/base/any/onlp/src/onlplib/module/src/i2c.c.bak"
            ],
        )
        #FIXME add support for grabbing headers and object files from the system if building on target

    if "com_github_bcm_sdklt" not in native.existing_rules():
        remote_workspace(
            name = "com_github_bcm_sdklt",
            remote = "https://github.com/Broadcom-Network-Switching-Software/SDKLT",
            branch = "master",
            build_file = "@//bazel:external/sdklt.BUILD",
        )
