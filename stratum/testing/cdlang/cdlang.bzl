"""Translation of set of CDLang+Go text template files into C++ code.

This module defines rules that are used to generate (and compile) files
from CDLang source and Go template files using the cdl_tool
"""

def cc_cdlang_library(name, srcs, template, deps, testonly = False, visibility = None):
    """Generates C++ code using Go template and CDLang source files.

    Args:
      name: The name of the package to use for the cc_library.
      srcs: The CDLang source files.
      template: The Go template file.
      deps: List of dependencies for the generated .cc file.
      testonly: A flag marking the destination libraty for usage in tests only.
      visibility: Standard blaze visibility parameter, passed through to
                  subsequent rules.
    """
    _genrules(name, srcs, template, deps, testonly, visibility, ".cc")

def _genrules(name, srcs, template, deps, testonly, visibility, ext):
    cdlang_tool_extra_flags = " "
    if not template.endswith(ext + ".tmpl"):
        fail("Template file must be a Go template ending with " + ext + ".tmpl", "template")
    for imp in srcs:
        if not imp.endswith(".cdlang"):
            fail("Source files must be CDLang files ending with .cdlang.", "srcs")
    srcs += [template]
    out_files = [name + ext]
    cmd = ("mkdir $$$$.tmp ; " + "cp $(SRCS) $$$$.tmp/ ; " + "cd $$$$.tmp ; " +
           ("../$(location //platforms/networking/hercules/testing/cdlang:cdl_tool) " +
            " -t " + template +
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
            "//platforms/networking/hercules/testing/cdlang:cdl_tool",
        ],
        visibility = visibility,
    )
    native.cc_library(
        name = name,
        srcs = [f for f in out_files if f.endswith(ext)],
        deps = deps,
        testonly = testonly,
        visibility = visibility,
    )
