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

// This is a .p4 file that HitAssignMapperTest uses to create IR data for
// unit tests.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The "basic_hit" control has a basic table hit test that the p4c frontend
// will transform with a temporary hit status variable.
control basic_hit(inout headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table test_table {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
    default_action = nop();
  }
  apply {
    if (!test_table.apply().hit) {
      mark_to_drop(standard_metadata);
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), basic_hit(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
