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

# All p4c backends that build as part of Stratum should add their IR definition
# files here.  This will cause the Stratum build of the p4c external repository
# to include the backend IR extensions as part of the common p4c build. See
# the "ir_generated_files" rule in bazel/external/p4c.BUILD for further details.

P4C_BACKEND_IR_FILES = [
    "@com_github_p4lang_p4c//:backends/bmv2/bmv2.def",
    "@com_github_stratum_stratum//stratum/p4c_backends/ir:stratum_fpm_ir.def",
]
