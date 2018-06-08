load("//build_util/rules:library_rule.bzl", "stratum_cc_library")
load("//build_util/rules:binary_rule.bzl", "stratum_cc_binary")
load("//build_util/rules:proto_rule.bzl", "stratum_cc_proto_library")
load("//build_util/rules:test_rule.bzl", "stratum_cc_test")
load("//build_util/rules:platform_rules.bzl",
     "stratum_platform_select", "stratum_platform_filter", "stratum_platform_alias")
load(":defs.bzl", "EMBEDDED_ARCHES", "HOST_ARCHES", "STRATUM_INTERNAL")