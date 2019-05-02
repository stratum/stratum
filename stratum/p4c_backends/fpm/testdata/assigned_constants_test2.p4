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

// This is a .p4 file that AssignedConstantInspector uses to verify various
// assignment statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Every action in the ingress control assigns a different value to
// the same metadata field.  The first two actions assign a constant;
// the last action assigns a parameter.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action assign1() {
    meta.other_metadata = 1;
  }
  action assign2() {
    meta.other_metadata = 2;
  }
  action assign_param(bit<32> param) {
    meta.other_metadata = param;
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table dummy_table {
    actions = {
      assign1;
      assign2;
      assign_param;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    dummy_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
