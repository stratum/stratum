package(
    #FIXME make the default private
    default_visibility = ["//visibility:public"],
)

# Dependencies for ONLP
cc_library(
    name = "AIM",
    hdrs = glob(["sm/infra/modules/AIM/module/inc/AIM/*.h"]),
    strip_include_prefix = "sm/infra/modules/AIM/module/inc",
    srcs = glob([
        "sm/infra/modules/AIM/module/src/*.c",
        "sm/infra/modules/AIM/module/src/*.h",
        "sm/infra/modules/AIM/module/src/posix/*.c",
        "sm/infra/modules/AIM/module/src/posix/*.h",
    ]),
)

cc_library(
    name = "sff",
    hdrs = glob([
        "sm/bigcode/modules/sff/module/inc/sff/*.h",
        "sm/bigcode/modules/sff/module/inc/sff/*.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/sff/module/inc",
    #FIXME srcs not yet building
    #srcs = glob([
    #    "sm/bigcode/modules/sff/module/src/*.c",
    #    "sm/bigcode/modules/sff/module/src/*.h",
    #]),
    deps = [
        ":AIM",
        ":cjson",
        ":cjson_util",
    ],
)

cc_library(
    name = "IOF",
    hdrs = glob(["sm/bigcode/modules/IOF/module/inc/IOF/*.h"]),
    strip_include_prefix = "sm/bigcode/modules/IOF/module/inc",
    srcs = glob([
        "sm/bigcode/modules/IOF/module/src/*.c",
        "sm/bigcode/modules/IOF/module/src/*.h",
    ]),
    deps = [":AIM"],
)

cc_library(
    name = "BigList",
    hdrs = glob(["sm/bigcode/modules/BigData/BigList/module/inc/BigList/*.h"]),
    strip_include_prefix = "sm/bigcode/modules/BigData/BigList/module/inc",
    srcs = glob([
        "sm/bigcode/modules/BigData/BigList/module/src/*.c",
        "sm/bigcode/modules/BigData/BigList/module/src/*.h",
    ]),
    deps = [":AIM"],
)

cc_library(
    name = "cjson",
    hdrs = glob(["sm/bigcode/modules/cjson/module/inc/cjson/*.h"]),
    strip_include_prefix = "sm/bigcode/modules/cjson/module/inc",
    srcs = glob(["sm/bigcode/modules/cjson/module/src/*.c"]),
)

cc_library(
    name = "cjson_util",
    hdrs = glob([
        "sm/bigcode/modules/cjson_util/module/inc/cjson_util/*.h",
        "sm/bigcode/modules/cjson_util/module/inc/cjson_util/*.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/cjson_util/module/inc",
    srcs = glob([
        "sm/bigcode/modules/cjson_util/module/src/*.c",
        "sm/bigcode/modules/cjson_util/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":cjson",
        ":IOF",
    ],
)

cc_library(
    name = "uCli",
    hdrs = glob([
        "sm/bigcode/modules/uCli/module/inc/uCli/*.h",
        "sm/bigcode/modules/uCli/module/inc/uCli/*.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/uCli/module/inc",
    srcs = glob([
        "sm/bigcode/modules/uCli/module/src/*.c",
        "sm/bigcode/modules/uCli/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":BigList",
        ":IOF",
    ],
)

# ONLP targets
# Need to break onlplib and onlp into two parts due to circular dependency between them
cc_library(
    name = "onlplib_headers",
    hdrs = [
               "packages/base/any/onlp/src/onlplib/module/inc/onlplib/onie.h",
               "packages/base/any/onlp/src/onlplib/module/inc/onlplib/onlplib_config.h",
               "packages/base/any/onlp/src/onlplib/module/inc/onlplib/onlplib_porting.h",
               "packages/base/any/onlp/src/onlplib/module/inc/onlplib/pi.h",
    ],
    strip_include_prefix = "packages/base/any/onlp/src/onlplib/module/inc",
    deps = [
        ":AIM",
        ":cjson",
        ":IOF",
    ],
)

cc_library(
    name = "onlp_headers",
    hdrs = [
        "packages/base/any/onlp/src/onlp/module/inc/onlp/fan.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/led.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/oids.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/onlp.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/onlp.x",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/onlp_config.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/onlp_porting.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/platform.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/psu.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/sfp.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/thermal.h",
    ],
    strip_include_prefix = "packages/base/any/onlp/src/onlp/module/inc",
    deps = [
        ":AIM",
        ":BigList",
        ":cjson",
        ":IOF",
        ":sff",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "onlplib",
    hdrs = [
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/crc32.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/file.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/file_uds.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/gpio.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/i2c.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/mmap.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/onlplib_dox.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/onlplib.x",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/sfp.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/shlocks.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/thermal.h",
       "packages/base/any/onlp/src/onlplib/module/inc/onlplib/util.h",
    ],
    strip_include_prefix = "packages/base/any/onlp/src/onlplib/module/inc",
    srcs = glob([
        "packages/base/any/onlp/src/onlplib/module/src/*.c",
        "packages/base/any/onlp/src/onlplib/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":BigList",
        ":cjson",
        ":cjson_util",
        ":IOF",
        ":onlp_headers",
        ":onlplib_headers",
    ],
)

#FIXME this target is not yet building
cc_library(
    name = "onlpv2",
    hdrs = [
        "packages/base/any/onlp/src/onlp/module/inc/onlp/attribute.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/chassis.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/debug.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/generic.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/module.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/onlp_dox.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/stdattrs.h",
    ] + glob([
        "packages/base/any/onlp/src/onlp/module/inc/onlp/platformi/*.h",
    ]),
    strip_include_prefix = "packages/base/any/onlp/src/onlp/module/inc",
    srcs = glob([
        "packages/base/any/onlp/src/onlp/module/src/*.c",
        "packages/base/any/onlp/src/onlp/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":BigList",
        ":cjson",
        ":cjson_util",
        ":IOF",
        ":onlp_headers",
        ":onlplib",
        ":onlplib_headers",
        ":sff",
        ":uCli",
        #FIXME could still be missing: timer_wheel OS
    ],
    visibility = ["//visibility:public"],
)
