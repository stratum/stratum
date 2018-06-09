def stratum_cc_test(name, deps = None, srcs = None, data = None, copts = None,
                    defines = None, linkopts = None, size = "small",
                    visibility = None):
  native.cc_test(
    name = name,
    deps = deps,
    srcs = srcs,
    data = data,
    copts = copts,
    defines = defines,
    linkopts = linkopts,
    size = size,
    visibility = visibility,
  )