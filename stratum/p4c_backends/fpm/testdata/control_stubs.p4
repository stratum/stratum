// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This .p4 file contains a set of control stubs for test P4 specs to use when
// they don't care about the control functionality.  For example, a test that
// only needs to verify P4 ingress operation can use stubs for the other
// controls.

#include <core.p4>
#include <v1model.p4>

#include "stratum/p4c_backends/fpm/testdata/simple_headers.p4"

#ifndef THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TESTDATA_CONTROL_STUBS_P4_
#define THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TESTDATA_CONTROL_STUBS_P4_

control egress_stub(inout headers hdr, inout test_metadata_t meta,
                    inout standard_metadata_t standard_metadata) {
  apply {}
}

control ingress_stub(inout headers hdr, inout test_metadata_t meta,
                     inout standard_metadata_t standard_metadata) {
  apply {}
}

control deparser_stub(packet_out packet, in headers hdr) {
  apply {}
}

control verify_checksum_stub(inout headers hdr, inout test_metadata_t meta) {
  apply {}
}

control compute_checksum_stub(inout headers hdr, inout test_metadata_t meta) {
  apply {}
}

#endif  // THIRD_PARTY_STRATUM_P4C_BACKENDS_FPM_TESTDATA_CONTROL_STUBS_P4_
