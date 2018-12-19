# All p4c backends that build as part of Stratum should add their IR definition
# files here.  This will cause the Stratum build of the p4c external repository
# to include the backend IR extensions as part of the common p4c build. See
# the "ir_generated_files" rule in bazel/external/p4c.BUILD for further details.

P4C_BACKEND_IR_FILES = [
    "@com_github_p4lang_p4c//:backends/bmv2/bmv2.def",
    "@com_github_stratum_stratum//stratum/p4c_backends/ir:stratum_ir.def",
]
