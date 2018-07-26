package(
    default_visibility = [ "//visibility:public" ],
)

genrule(
    name = "util_h",
    srcs = ["proto/PI/proto/util.h"],
    cmd = "sed 's#<p4/config/v1/p4info.pb.h>#\"p4/config/v1/p4info.pb.h\"#g' $< > $(OUTS)",
    outs = ["PI/proto/util.h"],
    visibility = [],
)

genrule(
    name = "util_cpp",
    srcs = ["proto/src/util.cpp"],
    cmd = "sed 's#<PI/proto/util.h>#\"PI/proto/util.h\"#g' $< > $(OUTS)",
    outs = ["src/util.cpp"],
    visibility = [],
)

cc_library(
    name = "util",
    hdrs = ["util_h"],
    srcs = ["util_cpp"],
    deps = [
        "@com_github_p4lang_p4runtime//:p4info_proto",
    ],
)
