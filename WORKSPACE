# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

workspace(name = "com_github_stratum_stratum")

# ---------------------------------------------------------------------------
#       Dependency overrides
#
#       If you would like to override a dependency, you can do so in
#       this section. Be sure to use the same name as the dependency
#       that you are overriding. You can use a local version of the
#       dependency or a remote one.
#
#       Example:
#
#       local_repository(
#           name = "com_github_opennetworklinux",
#           path = "~/OpenNetworkLinux",
#       )
#
#       Please do not push changes to this section upstream.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
#       Load tools to build Stratum
# ---------------------------------------------------------------------------

load("//bazel/rules:build_tools.bzl", "build_tools_deps")
build_tools_deps()

load("//bazel/rules:proto_gen.bzl", "proto_gen_deps")
proto_gen_deps()

# ---------------------------------------------------------------------------
#       Load Stratum dependencies
# ---------------------------------------------------------------------------
load("//bazel:deps.bzl", "stratum_deps")
stratum_deps()

# ---------------------------------------------------------------------------
#        Load transitive dependencies
# ---------------------------------------------------------------------------
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
grpc_extra_deps()

load("@com_github_p4lang_PI//bazel:deps.bzl", "PI_deps")
PI_deps()

load("//stratum/hal/bin/bmv2:bmv2.bzl", "bmv2_configure")
bmv2_configure(name = "local_bmv2_bin")

load("//stratum/hal/lib/barefoot:barefoot.bzl", "barefoot_configure")
barefoot_configure(name = "local_barefoot_bin")

load("//stratum/hal/bin/np4intel:np4intel.bzl", "np4intel_configure")
np4intel_configure(name = "local_np4intel_bin")

# PI NP4 dependancies
load("@com_github_p4lang_PI_np4//targets/np4:np4.bzl", "np4_configure")
np4_configure(name = "local_np4_bin")

load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
boost_deps()

load("@rules_python//python:repositories.bzl", "py_repositories")
py_repositories()

load("@rules_python//python:pip.bzl", "pip_repositories")
pip_repositories()

load("//stratum/hal/lib/phal/onlp:onlp.bzl", "onlp_configure")
onlp_configure(name = "local_onlp_bin")

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
switched_rules_by_language(
    name = "com_google_googleapis_imports",
    grpc = True,
    cc = True,
    python = True,
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")
rules_pkg_dependencies()

# ---------------------------------------------------------------------------
#       Load Golang dependencies.
# ---------------------------------------------------------------------------
load("@io_bazel_rules_go//go:deps.bzl", "go_rules_dependencies", "go_register_toolchains")
go_rules_dependencies()
go_register_toolchains()

load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies", "go_repository")
gazelle_dependencies(go_sdk = "go_sdk")

load("@io_bazel_rules_go//tests:grpc_repos.bzl", "grpc_dependencies")
grpc_dependencies()

# ---------------------------------------------------------------------------
#       Load CDLang dependencies.
# ---------------------------------------------------------------------------
load("//stratum/testing/cdlang:deps.bzl", "cdlang_rules_dependencies")
cdlang_rules_dependencies()

# ---------------------------------------------------------------------------
#       Load dependencies for `latex_document` rule.
# ---------------------------------------------------------------------------
load("@bazel_latex//:repositories.bzl", "latex_repositories")

latex_repositories()

# ---------------------------------------------------------------------------
#       Load dependencies for pipeline PTF rules
# ---------------------------------------------------------------------------
load("@rules_python//python:pip.bzl", "pip_import")
pip_import(
    name = "ptf_deps",
    requirements = "//stratum/pipelines/ptf:requirements.txt",
)

load("@ptf_deps//:requirements.bzl", ptf_pip_install = "pip_install")

ptf_pip_install()
