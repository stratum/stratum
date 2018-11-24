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

load("//stratum/hal/bin/bmv2:bmv2.bzl", "bmv2_configure")
bmv2_configure(name = "local_bmv2_bin")

load("@com_github_p4lang_PI//bazel:deps.bzl", "PI_deps")
PI_deps()

load("//stratum/hal/lib/barefoot:barefoot.bzl", "barefoot_configure")
barefoot_configure(name = "local_barefoot_bin")

load("//stratum/hal/lib/barefoot:thrift.bzl", "thrift_configure")
thrift_configure(name = "local_thrift_bin")
