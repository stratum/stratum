load("//bazel/rules:library_rule.bzl", "stratum_cc_library")
load("//bazel/rules:binary_rule.bzl", "stratum_cc_binary")
load("//bazel/rules:test_rule.bzl", "stratum_cc_test")
load("//bazel/rules:platform_rules.bzl",
     "stratum_platform_select", "stratum_platform_filter", "stratum_platform_alias")
load("//bazel/rules:package_rule.bzl", "stratum_package")
load(":defs.bzl", "EMBEDDED_ARCHES", "HOST_ARCHES", "STRATUM_INTERNAL")