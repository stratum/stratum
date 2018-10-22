package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "onlpv2_headers",
    hdrs = glob(["packages/base/any/onlp/src/onlp/module/inc/onlp/*.h"],exclude = ["packages/base/any/onlp/src/onlp/module/inc/onlp/sfp.h"]),
    includes = ["packages/base/any/onlp/src/onlp/module/inc"],
)

genrule(
    name = "sfp_h",
    srcs = ["packages/base/any/onlp/src/onlp/module/inc/onlp/sfp.h"],
    cmd = "sed 's/sff.h/sff_modified.h/g;s/dom.h/dom_modified.h/g' $< > $@",
    outs = ["packages/base/any/onlp/src/onlp/module/inc/onlp/sfp_modified.h"],
    visibility = [],
)

cc_library(
    name = "sfp_onlpv2",
    hdrs = ["sfp_h"],
    deps = [
        "@com_github_bigcode//:onlpv2_headers",
    ],
)

cc_library(
    name = "sff_headers",
    hdrs = glob(["modules/sff/module/inc/sff/*.h"],exclude =["modules/sff/module/inc/sff/sff.h",
    "modules/sff/module/inc/sff/dom.h"]),
    includes = ["modules/sff/module/inc"],
)

genrule(
    name = "sff_h",
    srcs = ["modules/sff/module/inc/sff/sff.h"],
    cmd = "sed '/dependmodules.x/d' $< > $@",
    outs = ["modules/sff/module/inc/sff/sff_modified.h"],
    visibility = [],
)

cc_library( 
    name = "sff", 
    hdrs = ["sff_h"],
    deps = [ 
        "@com_github_bigcode//:sff_headers", 
    ], 
)

genrule(
    name = "dom_h",
    srcs = ["modules/sff/module/inc/sff/dom.h"],
    cmd = "sed 's/sff.h/sff_modified.h/g' $< > $@",
    outs = ["modules/sff/module/inc/sff/dom_modified.h"],
    visibility = [],
)

cc_library(
    name = "dom",
    hdrs = ["dom_h"],
    deps = [
        "@com_github_bigcode//:sff_headers",
    ],
)
 
cc_library(
    name = "AIM_headers",
    hdrs = glob(["modules/AIM/module/inc/AIM/*.h"]),
    includes = ["modules/AIM/module/inc"],
)

cc_library(
    name = "IOF_headers",
    hdrs = glob(["modules/IOF/module/inc/IOF/*.h"]),
    includes = ["modules/IOF/module/inc"],
)

cc_library(
    name = "BigList_headers",
    hdrs = glob(["modules/BigData/BigList/module/inc/BigList/*.h"]),
    includes = ["modules/BigData/BigList/module/inc"],
)

cc_library(
    name = "cjson_headers",
    hdrs = glob(["modules/cjson/module/inc/cjson/*.h"]),
    includes = ["modules/cjson/module/inc"],
)
