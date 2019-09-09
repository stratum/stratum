#
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

def stratum_cc_binary(name, deps = None, srcs = None, data = None, args = None,
                      copts = None, defines = None, includes = None,
                      linkopts = None, testonly = None, visibility = None,
                      arches = None):
  if arches and arches != ["x86"] and arches != ["host"]:
    fail("Stratum does not currently support non-x86 architectures")
  native.cc_binary(
      name = name,
      deps = deps,
      srcs = srcs,
      data = data,
      args = args,
      copts = copts,
      defines = defines,
      includes = includes,
      linkopts = linkopts,
      testonly = testonly,
      visibility = visibility,
  )