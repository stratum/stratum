def stratum_cc_binary(name, deps = None, srcs = None, data = None, args = None,
                      copts = None, defines = None, includes = None,
                      linkopts = None, testonly = None, visibility = None,
                      arches = None):
  if arches and arches != ["x86"] and arches != ["host"]:
    fail("Stratum does not currently support non-x86 architectures")
  native.cc_binary(
      name = name,
      deps = deps,
      srcs = srcs,
      data = data,
      args = args,
      copts = copts,
      defines = defines,
      includes = includes,
      linkopts = linkopts,
      testonly = testonly,
      visibility = visibility,
  )