#
# Copyright 2018 Google LLC
# Copyright 2018-present Open Networking Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

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
