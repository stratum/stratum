load("@com_github_stratum_stratum//bazel/rules:yang_to_proto_rule.bzl", "yang_to_proto")
package(
    default_visibility = [ "//visibility:public" ],
)

filegroup(
    name = "hercules_yang_models",
    srcs = [
        "yang/openconfig-hercules-platform-linecard.yang",
        "yang/openconfig-hercules-qos.yang",
        "yang/openconfig-hercules-platform.yang",
        "yang/openconfig-hercules-platform-chassis.yang",
        "yang/openconfig-hercules-platform-port.yang",
        "yang/openconfig-hercules.yang",
        "yang/openconfig-hercules-interfaces.yang",
        "yang/openconfig-hercules-platform-node.yang",
    ]
)

yang_to_proto(
    name = "openconfig_yang_proto",
    srcs = [
        "@com_github_openconfig_public//:openconfig_yang_models",
        ":hercules_yang_models",
        "@//stratum/public/model:stratum_models",
    ],
    hdrs = [
        "@com_github_openconfig_public//:openconfig_yang_models_hdrs",
        "@com_github_yang_models_yang//:ietf_yang_models",
    ],
    outs = [
        "openconfig/openconfig.proto",
        "openconfig/enums/enums.proto",
    ],
    pkg_name = "openconfig",
    exclude_modules = ["ietf-interfaces"],
)

proto_library(
    name = "openconfig_proto",
    srcs = [
        "@com_github_openconfig_hercules//:openconfig_yang_proto",
    ],
    deps = [
        "@com_github_openconfig_ygot_proto//:ywrapper_proto",
        "@com_github_openconfig_ygot_proto//:yext_proto",
        "@com_google_protobuf//:descriptor_proto",
        "@com_google_protobuf//:any_proto",
    ]
)

cc_proto_library(
    name = "openconfig_cc_proto",
    deps = [":openconfig_proto"]
)