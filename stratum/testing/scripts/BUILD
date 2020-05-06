# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
# SPDX-License-Identifier: Apache-2.0

licenses(["notice"])  # Apache v2

py_binary(
    name = "testbed_model_generator",
    srcs = [
        "testbed_model_generator.py",
    ],
    malloc = "@com_github_google_glog//:glog:system_malloc",  # Can't use TCMalloc: b/77272606.
    deps = [
        "//platforms/networking/sandcastle/model:model_lib",
        "//platforms/networking/sandcastle/model/design/jupiter:abstraction_lib",
        "//platforms/networking/sandcastle/model/design/jupiter/function",
        "//pyglib",
        "//py/ipaddr",
    ],
)

exports_files(["do_nothing.sh"])
