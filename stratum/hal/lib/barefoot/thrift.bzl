def _impl(repository_ctx):
    if "THRIFT_INSTALL" not in repository_ctx.os.environ:
        repository_ctx.file("BUILD", """
""")
        return
    thrift_path = repository_ctx.os.environ["THRIFT_INSTALL"]
    repository_ctx.symlink(thrift_path, "thrift-bin")
    repository_ctx.file("BUILD", """
package(
    default_visibility = ["//visibility:public"],
)
cc_import(
  name = "thrift",
  hdrs = [],
  shared_library = "thrift-bin/libthrift.so",
)
""")

thrift_configure = repository_rule(
    implementation=_impl,
    local = True,
    environ = ["THRIFT_INSTALL"])
