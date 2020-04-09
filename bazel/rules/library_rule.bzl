# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

def stratum_cc_library(name, deps = None, srcs = None, data = None,
                       hdrs = None, copts = None, defines = None,
                       include_prefix = None, includes = None,
                       strip_include_prefix = None, testonly = None,
                       textual_hdrs = None, visibility = None,
                       arches = None):
  if arches and arches != ["x86"] and arches != ["host"]:
    fail("Stratum does not currently support non-x86 architectures")

  alwayslink = 0
  if srcs:
    if type(srcs) == "select":
      alwayslink = 1
    elif [s for s in srcs if not s.endswith(".h")]:
      alwayslink = 1

  native.cc_library(
    name = name,
    deps = deps,
    srcs = srcs,
    data = data,
    hdrs = hdrs,
    alwayslink = alwayslink,
    copts = copts,
    defines = defines,
    include_prefix = include_prefix,
    includes = includes,
    strip_include_prefix = strip_include_prefix,
    testonly = testonly,
    textual_hdrs = textual_hdrs,
    visibility = visibility,
  )
