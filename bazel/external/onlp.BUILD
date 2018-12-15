package(
    #FIXME(boc) make the default private
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
    copts = [
        "-DAIM_CONFIG_INCLUDE_DAEMONIZE=1",
    ],
)

cc_library(
    name = "sff",
    hdrs = glob([
        "sm/bigcode/modules/sff/module/inc/sff/*.h",
        "sm/bigcode/modules/sff/module/inc/sff/*.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/sff/module/inc",
    srcs = glob([
        "sm/bigcode/modules/sff/module/src/*.c",
        "sm/bigcode/modules/sff/module/src/*.h",
    ]),
    copts = ["-DDEPENDMODULE_INCLUDE_CJSON_UTIL=1"],
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
        #":ELS",
        ":IOF",
    ],
    copts = [
        #"-DUCLI_CONFIG_INCLUDE_ELS_LOOP=1",
    ],
)

cc_library(
    name = "timer_wheel",
    hdrs = glob([
        "sm/bigcode/modules/timer_wheel/module/inc/timer_wheel/*.h",
        "sm/bigcode/modules/timer_wheel/module/inc/timer_wheel/timer_wheel.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/timer_wheel/module/inc",
    srcs = glob([
        "sm/bigcode/modules/timer_wheel/module/src/*.c",
        "sm/bigcode/modules/timer_wheel/module/src/*.h",
    ]),
    deps = [":AIM"],
)

cc_library(
    name = "OS",
    hdrs = glob([
        "sm/bigcode/modules/OS/module/inc/OS/*.h",
        "sm/bigcode/modules/OS/module/inc/OS/os.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/OS/module/inc",
    srcs = glob([
        "sm/bigcode/modules/OS/module/src/*.c",
        "sm/bigcode/modules/OS/module/src/*.h",
    ]),
    deps = [":AIM"],
    copts = ["-DOS_CONFIG_INCLUDE_POSIX=1"], #FIXME could be other options
)

cc_library(
    name = "ELS",
    hdrs = glob([
        "sm/bigcode/modules/ELS/module/inc/ELS/*.h",
        "sm/bigcode/modules/ELS/module/inc/ELS/*.x",
    ]),
    strip_include_prefix = "sm/bigcode/modules/ELS/module/inc",
    srcs = glob([
        "sm/bigcode/modules/ELS/module/src/*.c",
        "sm/bigcode/modules/ELS/module/src/*.h",
    ]),
    deps = [":AIM"],
)

# ONLP targets
# Need to break onlplib and onlp into two parts (hdrs and srcs) due to circular dependency between them
cc_library(
    name = "onlplib_headers",
    hdrs = glob([
        "packages/base/any/onlp/src/onlplib/module/inc/onlplib/*.h",
        "packages/base/any/onlp/src/onlplib/module/inc/onlplib/*.x",
    ]),
    strip_include_prefix = "packages/base/any/onlp/src/onlplib/module/inc",
    deps = [
        ":AIM",
        ":cjson",
        ":IOF",
    ],
)

cc_library(
    name = "onlp_headers",
    hdrs = glob([
        "packages/base/any/onlp/src/onlp/module/inc/onlp/*.h",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/*.x",
        "packages/base/any/onlp/src/onlp/module/inc/onlp/platformi/*.h",
    ]),
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
    name = "onlp-platform-defaults",
    hdrs = glob([
        "packages/base/any/onlp/src/onlp_platform_defaults/module/inc/onlp_platform_defaults/*.h",
        "packages/base/any/onlp/src/onlp_platform_defaults/module/inc/onlp_platform_defaults/*.x",
    ]),
    strip_include_prefix = "packages/base/any/onlp/src/onlp_platform_defaults/module/inc",
    srcs = glob([
        "packages/base/any/onlp/src/onlp_platform_defaults/module/src/*.c",
        "packages/base/any/onlp/src/onlp_platform_defaults/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":onlp_headers",
        ":onlplib_headers",
        ":IOF",
        ":sff",
    ],
    copts = [
        "-DAIM_CONFIG_INCLUDE_CTOR_DTOR=1",
        "-DONLP_PLATFORM_DEFAULTS_CONFIG_AS_PLATFORM=0",
        "-fPIC",
    ],
    linkopts = ["-lpthread"],
    visibility = ["//visibility:public"],
)

#FIXME(boc) this target has some warnings in src/i2c.c
cc_library(
    name = "onlplib",
    hdrs = [":onlplib_headers"],
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
    alwayslink = 1,
)

#FIXME(boc) this target has some warnings in src/onlp_ucli.c and src/sfp.c
cc_library(
    name = "onlp",
    hdrs = [":onlp_headers"],
    srcs = glob([
        "packages/base/any/onlp/src/onlp/module/src/*.c",
        "packages/base/any/onlp/src/onlp/module/src/*.h",
    ], exclude = [
        "packages/base/any/onlp/src/onlp/module/src/onlp_main.c",
        "packages/base/any/onlp/src/onlp/module/src/onlp_ucli.c",
    ]),
    deps = [
        ":AIM",
        #":BigList",
        ":cjson",
        ":cjson_util",
        ":IOF",
        ":onlp-platform-defaults",
        ":onlplib",
        ":OS",
        ":sff",
        ":uCli",
        ":timer_wheel",
        #FIXME missing ELS
    ],
    copts = [
        "-DAIM_CONFIG_INCLUDE_CTOR_DTOR=1",
        "-DAIM_CONFIG_INCLUDE_MODULES_INIT=1",
        "-DONLP_CONFIG_API_LOCK_GLOBAL_SHARED=1",
        "-DONLP_CONFIG_INCLUDE_SHLOCK_GLOBAL_INIT=1",
        "-DAIM_CONFIG_INCLUDE_PVS_SYSLOG=1",
        "-DAIM_CONFIG_INCLUDE_DAEMONIZE=1",
        "-DONLP_CONFIG_INCLUDE_UCLI=1",
        "-DUCLI_CONFIG_INCLUDE_ELS_LOOP=1",
        "-DDEPENDMODULE_INCLUDE_CJSON_UTIL=1",
        "-fPIC",
    ],
    linkopts = [
        #"-Wl,-undefined,dynamic_lookup",
        "-lpthread",
        #"-ledit", FIXME not found
        #FIXME -ledit $(LIBONLP_PLATFORM) $(LIBONLP_PLATFORM_DEFAULTS)
        #LIBONLP_PLATFORM_DEFAULTS := ../onlp-platform-defaults/$(BUILD_DIR)/bin/libonlp-platform-defaults.so
        #LIBONLP_PLATFORM := ../onlp-platform/$(BUILD_DIR)/bin/libonlp-platform.so

    ],
    alwayslink = 1,
    visibility = ["//visibility:public"],
)

''' FIXME target is shared lib (libonlp-platform.so)
MODULE := libonlp-platform-module
include $(BUILDER)/standardinit.mk

DEPENDMODULES := AIM onlplib onlp_platform_defaults IOF sff

include $(BUILDER)/dependmodules.mk

SHAREDLIB := libonlp-platform.so
$(SHAREDLIB)_TARGETS := $(ALL_TARGETS)
include $(BUILDER)/so.mk

.DEFAULT_GOAL := sharedlibs

GLOBAL_CFLAGS += -DONLP_PLATFORM_DEFAULTS_CONFIG_AS_PLATFORM=1
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_CTOR_DTOR=1
GLOBAL_CFLAGS += -I$(onlp_BASEDIR)/module/inc
GLOBAL_CFLAGS += -fPIC
GLOBAL_LINK_LIBS += -lpthread
'''

''' FIXME
MODULE := onlpd-module
include $(BUILDER)/standardinit.mk

DEPENDMODULES := AIM

include $(BUILDER)/dependmodules.mk

BINARY := onlpd
$(BINARY)_LIBRARIES := $(LIBRARY_TARGETS)
include $(BUILDER)/bin.mk

LIBONLP_SO := ../onlp/$(BUILD_DIR)/bin/libonlp.so

GLOBAL_CFLAGS += -DAIM_CONFIG_AIM_MAIN_FUNCTION=onlpdump_main
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_MODULES_INIT=1
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_MAIN=1
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_DAEMONIZE=1
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_PVS_SYSLOG=1

GLOBAL_LINK_LIBS += $(LIBONLP_SO) -Wl,--unresolved-symbols=ignore-in-shared-libs
GLOBAL_LINK_LIBS += -lpthread -lm -lrt
'''
cc_binary(
    name = "onlpd",
    srcs = [
        "packages/base/any/onlp/src/onlp/module/src/onlp_main.c",
        "packages/base/any/onlp/src/onlp/module/src/onlp_ucli.c",
    ],
    deps = [
        ":AIM",
        ":onlp",
        ":onlp_platform_as7712",
    ],
    copts = [
        "-DAIM_CONFIG_AIM_MAIN_FUNCTION=onlpdump_main",
        "-DAIM_CONFIG_INCLUDE_MODULES_INIT=1",
        "-DAIM_CONFIG_INCLUDE_MAIN=1",
        "-DAIM_CONFIG_INCLUDE_DAEMONIZE=1",
        "-DAIM_CONFIG_INCLUDE_PVS_SYSLOG=1",
    ],
    linkopts = [
        "-Wl,--unresolved-symbols=ignore-in-shared-libs",
        "-lpthread -lm -lrt",
    ],
)

cc_library(
    name = "onlp_sim",
    hdrs = glob([
        "packages/base/any/onlp/src/onlp_platform_sim/module/inc/onlp_platform_sim/*.h",
        "packages/base/any/onlp/src/onlp_platform_sim/module/inc/onlp_platform_sim/*.x",
    ]),
    strip_include_prefix = "packages/base/any/onlp/src/onlp_platform_sim/module/inc",
    srcs = glob([
        "packages/base/any/onlp/src/onlp_platform_sim/module/src/*.c",
        "packages/base/any/onlp/src/onlp_platform_sim/module/src/*.h",
    ]),
    deps = [
        ":onlp",
    ],
    visibility = ["//visibility:public"],
)


'''
GLOBAL_CFLAGS += -DAIM_CONFIG_INCLUDE_CTOR_DTOR=1
GLOBAL_CFLAGS += -DONLP_PLATFORM_DEFAULTS_CONFIG_AS_PLATFORM=0
GLOBAL_CFLAGS += -I$(onlp_BASEDIR)/module/inc
GLOBAL_CFLAGS += -fPIC
GLOBAL_LINK_LIBS += -lpthread
'''

cc_library(
    name = "onlp_platform_as7712",
    hdrs = glob([
        "packages/platforms/accton/x86-64/as7712-32x/onlp/builds/x86_64_accton_as7712_32x/module/inc/x86_64_accton_as7712_32x/*.h",
        "packages/platforms/accton/x86-64/as7712-32x/onlp/builds/x86_64_accton_as7712_32x/module/inc/x86_64_accton_as7712_32x/*.x",
    ]),
    strip_include_prefix = "packages/platforms/accton/x86-64/as7712-32x/onlp/builds/x86_64_accton_as7712_32x/module/inc",
    srcs = glob([
        "packages/platforms/accton/x86-64/as7712-32x/onlp/builds/x86_64_accton_as7712_32x/module/src/*.c",
        "packages/platforms/accton/x86-64/as7712-32x/onlp/builds/x86_64_accton_as7712_32x/module/src/*.h",
    ]),
    deps = [
        ":AIM",
        ":onlp",
        ":onlp-platform-defaults",
        ":onlplib",
        ":uCli",
        ":IOF",
    ],
    copts = [
        "-DAIM_CONFIG_INCLUDE_MODULES_INIT=1",
        "-fPIC",
    ],
    linkopts = ["-lpthread"],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "onlp_uber",
    hdrs = [":onlp_headers"],
    srcs = [
        ":onlp",
        ":onlp_platform_defaults",
        ":onlp_platform_as7712",
            #":uCli",
            #":AIM",
            #":BigList",
            #":cjson",
            #":cjson_util",
            #":IOF",
            #":onlp_headers",
            #":onlplib",
            #":onlplib_headers",
            #":sff",
            #":uCli",
            #":timer_wheel",
            #":OS",
    ],
    linkstatic=1,
    alwayslink=1,
    visibility = ["//visibility:public"],
)