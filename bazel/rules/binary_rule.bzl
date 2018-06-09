def stratum_cc_binary(name, deps = None, srcs = None, copts = None,
                      defines = None, includes = None, testonly = None,
                      visibility = None, arches = None):
  if arches and arches != ["x86"] and arches != ["host"]:
    fail("Stratum does not currently support non-x86 architectures")
  native.cc_binary(
      name = name,
      deps = deps,
      srcs = srcs,
      copts = copts,
      defines = defines,
      includes = includes,
      testonly = testonly,
      visibility = visibility,
  )