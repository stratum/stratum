load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")

package(
    default_visibility = [ "//visibility:public" ],
)

proto_library(
    name = "gnmi_ext_proto",
    srcs = ["gnmi_ext/gnmi_ext.proto"],
)

proto_library(
    name = "gnmi_proto",
    srcs = ["gnmi/gnmi.proto"],
    deps = [
        ":gnmi_ext_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "gnmi_ext_cc_proto",
    deps = [":gnmi_ext_proto"]
)

cc_proto_library(
    name = "gnmi_cc_proto",
    deps = [":gnmi_proto"],
)

cc_grpc_library(
    name = "gnmi_cc_grpc",
    srcs = [":gnmi_proto"],
    deps = [
        ":gnmi_cc_proto"
    ],
    grpc_only = True
)