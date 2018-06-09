load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_library")

def stratum_cc_proto_library(name, deps = [], srcs = [], arches = [],
                             python_support = False, with_grpc = True,
                             include_wkt = True, visibility = None, hdrs=[]):
  if arches and arches != ["x86"] and arches != ["host"]:
    fail("Stratum does not currently support non-x86 architectures")

  new_srcs = []
  if srcs:
    new_srcs += srcs
  if hdrs:
    new_srcs += hdrs

  cpp_proto_library(
    name = name,
    protos = new_srcs,
    proto_deps = deps,
    imports = ["external/com_google_protobuf/src/"] if include_wkt else [],
    inputs = ["@com_google_protobuf//:well_known_protos"] if include_wkt else [],
    with_grpc = with_grpc,
  )

  if python_support:
    """
    For library "name", generates:
      * ${name}_default_pb, a regular proto library.
      * ${name}_py, a py_proto_library based on ${name}_default_pb.
    """
    proto_name = name + "_default_pb"
    native.proto_library(
      name = proto_name,
      srcs = new_srcs,
      deps = [dep + "_default_pb" for dep in deps],
      visibility = visibility)
    native.py_proto_library(
      name = name + "_py",
      api_version = 2,
      deps = [proto_name],
      visibility = visibility)

def wrapped_cc_proto_library(name, deps = [], srcs = [], arches = [],
                             python_support = False, with_grpc = True,
                             include_wkt = True, visibility = None,
                             proto_source_root = None,
                             prefix = "", old_prefix = "",
                             rewrite_proto_imports = False):

  if prefix and not prefix.endswith("/"):
      prefix += "/"
  if old_prefix and not old_prefix.endswith("/"):
      old_prefix += "/"
  if proto_source_root and not proto_source_root.endswith("/"):
      proto_source_root += "/"

  if rewrite_proto_imports and prefix and old_prefix:
    if "#" in prefix or "#" in old_prefix:
      fail("prefix and old_prefix cannot contain #")
    cmd = "sed 's#import[ \t]*\"%s#import \"%s#g' $< > $(OUTS)" % (old_prefix, prefix)
  else:
    cmd = "cp $< $(OUTS)"

  if old_prefix and prefix.endswith(old_prefix):
    prefix = prefix[:-len(old_prefix)]

  gen_srcs = []
  for src in srcs:
    if proto_source_root and src.startswith(proto_source_root):
      gen_src = prefix + src[len(proto_source_root):]
    else:
     gen_src = prefix + src
    if src != gen_src:
      gen_name = name + "_generated_" + src
      native.genrule(
          name = gen_name,
          srcs = [src],
          cmd = cmd,
          outs = [gen_src],
          visibility = ["//visibility:public"],
      )  
      gen_srcs.append(":" + gen_name)
    else:
      gen_srcs.append(src)

  stratum_cc_proto_library(
    name = name,
    deps = deps,
    srcs = gen_srcs,
    arches = arches,
    python_support = python_support,
    with_grpc = with_grpc,
    include_wkt = include_wkt,
  )

# --------------- NOTES ------------------
'''
sc_proto_lib(name = None, srcs = [], hdrs = [], deps = [], arches = [],
                 visibility = None, testonly = None, proto_include = None,
                 python_support = False, services = []):
--- or ---
pubref.cpp_proto_library(
    name,
    langs = [str(Label("//cpp"))],
    protos = [], imports = [], inputs = [], proto_deps = [], output_to_workspace = False,
    protoc = None, pb_plugin = None, pb_options = [],
    grpc_plugin = None, grpc_options = [], proto_compile_args = {},
    with_grpc = True, srcs = [], deps = [], verbose = 0)
--- or ---
native.proto_library(name, deps, srcs, data, compatible_with, deprecation,
        distribs, features, licenses, proto_source_root, restricted_to,
        tags, testonly, visibility)
native.cc_proto_library(name, deps, data, compatible_with, deprecation, distribs,
        features, licenses, restricted_to, tags, testonly, visibility)
grpc.cc_grpc_library(name, srcs, deps, proto_only, well_known_protos,
        generate_mocks = False, use_external = False, **kwargs):

- hdrs vs. protos vs. srcs (in proto_library)
- srcs vs. None vs. None
- deps vs. proto_deps vs. deps (in proto_library)
- arches vs. None vs. None <<same as cc_library>>
- testonly vs. None vs. testonly (in proto_library)
- proto_include vs. imports? vs. use deps? << need to figure out if this is used>>
- python_support vs. separate python rule vs. separate python rule
- services vs. with_grpc vs. separate rule

proposed rule:
stratum_proto_library(name, deps = [], srcs = [], proto_source_root = None,
                      testonly = False, arches = [],
                      with_grpc = True, include_wkt = False)
'''