load("//golang/antlr4:antlr4.bzl", "go_antlr4_combined")

package(
    #default_hdrs_check = "strict",
    default_visibility = ["//visibility:public"],
)

go_binary(
    name = "cdl_tool",
    srcs = ["main.go"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_google_glog//:glog/go:flag",
        "@com_github_google_glog//:glog/go:log",
        "//stratum/testing/cdlang",
        "//stratum/testing/cdlang:cdl",
    ],
)

go_antlr4_combined(
    name = "cdlang",
    src = "CDLang.g4",
    listener = True,
    visitor = True,
)

go_library(
    name = "cdl",
    srcs = [
        "dom.go",
        "dom_vistor.go",
        "tree_builder.go",
        "visitor.go",
    ],
    deps = [
        "@com_github_google_glog//:glog/go:log",
        "//stratum/testing/cdlang",
        "//golang/antlr4:antlr",
    ],
)

go_test(
    name = "cdl_test",
    size = "small",
    srcs = ["main_test.go"],
    library = ":cdl",
    deps = ["//golang/godebug/pretty"],
)
