workspace(name = "com_github_stratum_stratum")

load("//bazel:workspace_rule.bzl", "remote_workspace")

# TODO move remote_workspaces to stratum_deps macro in a .bzl file

# -----------------------------------------------------------------------------
#        Protobuf + gRPC compiler and external models
# -----------------------------------------------------------------------------

# ----- protobuf -----
remote_workspace(
    name = "com_google_protobuf",
    remote = "https://github.com/google/protobuf",
    tag = "3.5.1",
)

# ----- gRPC -----
remote_workspace(
    name = "com_github_grpc_grpc",
    remote = "https://github.com/grpc/grpc",
    tag = "1.12.1",
)

# ----- protoc w/ gRPC compiler -----
#FIXME update to upstream when pull requests are merged
remote_workspace(
    name = "org_pubref_rules_protobuf",
    remote = "https://github.com/bocon13/rules_protobuf",
    branch = "master",
)

# ----- googleapis -----
remote_workspace(
    name = "com_github_googleapis",
    remote = "https://github.com/googleapis/googleapis",
    commit = "a19256f36347fde5f2ab44e24e6e6c6b2a314041",
    build_file = "bazel/external/googleapis.BUILD",
)

# ----- P4 Runtime -----
remote_workspace(
    name = "com_github_p4lang_PI",
    remote = "https://github.com/p4lang/PI",
    branch = "master",
    build_file = "bazel/external/p4runtime.BUILD",
)

# ----- gNMI -----
remote_workspace(
    name = "com_github_openconfig_gnmi",
    remote = "https://github.com/openconfig/gnmi",
    branch = "master",
    build_file = "bazel/external/gnmi.BUILD",
)

# -----------------------------------------------------------------------------
#        Third party C++ libraries
# -----------------------------------------------------------------------------

# ----- Abseil -----
remote_workspace(
    name = "com_google_absl",
    remote = "https://github.com/abseil/abseil-cpp",
    branch = "master",
)

# CCTZ (Time-zone framework); required for Abseil time
remote_workspace(
    name = "com_googlesource_code_cctz",
    remote = "https://github.com/google/cctz",
    branch = "master",
)

# ----- glog -----
remote_workspace(
    name = "com_github_google_glog",
    remote = "https://github.com/google/glog",
    branch = "master",
)

# ----- gflags -----
remote_workspace(
    name = "com_github_gflags_gflags",
    remote = "https://github.com/gflags/gflags",
    branch = "master",
)

# ----- Google Test -----
remote_workspace(
    name = "com_google_googletest",
    remote = "https://github.com/google/googletest",
    branch = "master",
)

# ----- re2 ------
remote_workspace(
    name = "com_googlesource_code_re2",
    remote = "https://github.com/google/re2",
    branch = "master",
)

# -----------------------------------------------------------------------------
#        Load transitive deps
# -----------------------------------------------------------------------------

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")
cpp_proto_repositories()