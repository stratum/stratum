#
# Copyright 2019 Google LLC
# Copyright 2019-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0
#

load("@rules_cc//cc:defs.bzl", "cc_library")

"""Processing of CDLang and Go text template files.

This module defines rules that are used to generate (and compile) files
from CDLang source and Go template files using the cdl_tool.
"""

load("@bazel_latex//:latex.bzl", "latex_document")

def cc_cdlang_library(name, srcs, deps, template, ver = "latest", libs = None, testonly = False, visibility = None):
    """Generates C++ code using Go templates and CDLang source files.

    Args:
      name: The name of the package to use for the cc_library.
      srcs: The CDLang source files.
      deps: List of dependencies for the generated .cc file.
      template: the Go template file.
      ver: version of the tests. Either "x.y.z" or "latest".
      libs: A list of Go template files that contain rules used by 'template'.
      testonly: A flag marking the destination libraty for usage in tests only.
      visibility: Standard blaze visibility parameter, passed through to
                  subsequent rules.
    """

    templates = [template]
    if libs != None:
        templates += libs
    ext = ".cc"
    out_files = [name + ext]
    _genrules(name, srcs, templates, ver, out_files, visibility, ext)
    cc_library(
        name = name,
        srcs = [f for f in out_files if f.endswith(ext)],
        deps = deps,
        testonly = testonly,
        visibility = visibility,
    )

def pdf_cdlang(name, srcs, template, ver = "latest", libs = None, visibility = None):
    """Generates PDF file via TeX using Go templates and CDLang source files.

    Args:
      name: The name of the package to use for the cc_library.
      srcs: The CDLang source files.
      template: the Go template file.
      ver: version of the tests. Either "x.y.z" or "latest".
      libs: A list of Go template files that contain rules used by 'template'.
      visibility: Standard blaze visibility parameter, passed through to
                  subsequent rules.
    """

    templates = [template]
    if libs != None:
        templates += libs
    ext = ".tex"
    _genrules(name, srcs, templates, ver, [name + ext], visibility, ext)
    latex_document(
        name = name,
        main = name + ext,
        srcs = [],
    )

def _genrules(name, srcs, templates, ver, out_files, visibility, ext):
    cdlang_tool_extra_flags = " "
    for tmpl in templates:
        if not tmpl.endswith(ext + ".tmpl"):
            fail("Template file must be a Go template ending with " + ext + ".tmpl", "templates")
    for imp in srcs:
        if not imp.endswith(".cdlang"):
            fail("Source files must be CDLang files ending with .cdlang.", "srcs")
    srcs += templates
    cmd = ("mkdir $$$$.tmp ; " + "cp $(SRCS) $$$$.tmp/ ; " + "cd $$$$.tmp ; " +
           ("../$(location //stratum/testing/cdlang:cdl_tool) " +
            "-t " + (",".join(templates)) +
            " -v " + ver +
            " -o " + name + ext + " " +
            (" ".join(srcs[:-1])) +
            cdlang_tool_extra_flags +
            " ; ") +
           "cd .. ; " + (
        "".join(
            [
                " cp $$$$.tmp/%s $(@D)/ ;" % filepath
                for filepath in out_files
            ],
        )
    ) + "rm -rf $$$$.tmp")
    native.genrule(
        name = name + "_source",
        srcs = srcs,
        outs = out_files,
        cmd = cmd,
        heuristic_label_expansion = 0,
        tools = [
            "//stratum/testing/cdlang:cdl_tool",
        ],
        visibility = visibility,
    )
