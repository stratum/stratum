workspace(name = "com_github_stratum_stratum")

load("//bazel:deps.bzl", "stratum_deps")
stratum_deps()

# -----------------------------------------------------------------------------
#        Load transitive deps
# -----------------------------------------------------------------------------
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")
cpp_proto_repositories()