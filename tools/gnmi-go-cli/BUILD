load("@io_bazel_rules_go//go:def.bzl", "go_library", "go_binary")

package(
    default_visibility = ["//visibility:public"],
)

sh_binary(
  name = "gnmi_request",
  srcs = ["gnmi_request.sh"],
  args = ["$(location @com_github_openconfig_gnmi//cmd/gnmi_cli:gnmi_cli)"],
  data = ["@com_github_openconfig_gnmi//cmd/gnmi_cli:gnmi_cli"],
)