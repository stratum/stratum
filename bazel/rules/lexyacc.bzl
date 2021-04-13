# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

# Expects to find flex and bison in the path.
def genlex(name, src, out, prefix, includes = [], visibility = []):
    """Generate a C++ lexer from a lex file using Flex.
    Args:
      name: The name of the rule.
      src: The .lex source file.
      out: The generated source file.
      includes: A list of headers included by the .lex file.
      prefix: Passed to flex as the -P option.
    """
    cmd = "flex -o $(location %s) -P %s $(location %s)" % (out, prefix, src)
    native.genrule(
        name = name,
        outs = [out],
        srcs = [src] + includes,
        cmd = cmd,
        visibility = visibility,
    )

def genyacc(
        name,
        src,
        header_out,
        source_out,
        extra_outs = [],
        visibility = []):
    """Generate a C++ parser from a Yacc file using Bison.
    Args:
      name: The name of the rule.
      src: The input grammar file.
      header_out: The generated header file.
      source_out: The generated source file.
      extra_outs: Additional generated outputs.
    """
    cmd = ("bison --defines=$(location %s) -o $(@D)/%s $(location %s)" %
           (header_out, source_out, src))
    native.genrule(
        name = name,
        outs = [source_out, header_out] + extra_outs,
        srcs = [src],
        cmd = cmd,
        visibility = visibility,
    )
