Copyright 2019 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
# Stratum: P4C backend

The p4c_backends directory contains [P4 compiler](https://github.com/p4lang/p4c)
extensions for the Stratum vendor-agnostic networking project.  These
extensions take a [P4 specification](http://p4.org/spec/) as input.  They
produce output that Stratum platforms use to encode/decode P4 runtime RPCs
and implement the network behavior defined by the P4 spec.  Refer to the
[overview]
(http://need_a_link)
for more details.
