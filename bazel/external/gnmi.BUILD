load(
    "@com_github_stratum_stratum//bazel/rules:proto_rule.bzl",
    "wrapped_cc_proto_library",
)

PREFIX = "github.com/openconfig/gnmi/"

package(
    default_visibility = [ "//visibility:public" ],
)

'''FIXME require non-gRPC and gRPC rules'''

wrapped_cc_proto_library(
    name = "gnmi_ext_proto",
    prefix = PREFIX,
    srcs = ["proto/gnmi_ext/gnmi_ext.proto"],
    with_grpc = False,
    include_wkt = False,
)

wrapped_cc_proto_library(
    name = "gnmi_proto",
    prefix = PREFIX,
    srcs = ["proto/gnmi/gnmi.proto"],
    deps = [":gnmi_ext_proto"],
    with_grpc = True,
    include_wkt = True,
)

# hack for com_github_p4lang_PI
cc_library(name = "gnmi_cc_grpc", deps = [":gnmi_proto"])