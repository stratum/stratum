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

// This is a .p4 file that PipelinePassesTest uses to verify block-level
// optimization.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The ingress control contains four tables, which map to physical pipeline
// stages as follows:
//  table acl_v->VLAN_ACL
//  table lpm_1->L3_LPM
//  table lpm_2->L3_LPM
//  table lpm_3->L3_LPM
// The optimization passes should optimize the L3_LPM stages in the apply
// logic below.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table acl_v {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L3_LPM")
  table lpm_1 {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L3_LPM")
  table lpm_2 {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L3_LPM")
  table lpm_3 {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  apply {
    if (!acl_v.apply().hit) {
      if (!lpm_1.apply().hit) {
        lpm_2.apply();
      }
    } else {
      lpm_3.apply();
    }
  }
}

// The egress control has some if statements that should not be
// transformed by the PipelineIfBlockInsertPass class.
control egress_no_block(inout headers hdr, inout test_metadata_t meta,
                        inout standard_metadata_t standard_metadata) {
  apply {
    // If statments do not need BlockStatement wrappers.
    if (hdr.ethernet.isValid()) {
      // Assignments do not need BlockStatement wrappers.
      meta.color = 1;
    } else {
      // Empty statements, returns, and exits do not need BlockStatement
      // wrappers.
      if (meta.color == 2) {
      } else {
        if (meta.color == 3) {
          return;
        } else {
          exit;
        }
      }
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_no_block(),
         compute_checksum_stub(), deparser_stub()) main;
