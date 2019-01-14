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

// This is a test .p4 file to make sure the midend pass is not introducing
// temporary tables and actions that clutter the p4c output.  The open source
// midends generate these temporaries to help the bmv2 pipeline.  In this
// file, the open source midends added a table and action for the if statement
// after the test_table.apply() below.  The Hercules custom midend should not
// have these temporaries.
#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

#define COLOR_GREEN 1

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta, inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

control ingress(inout headers hdr, inout test_metadata_t meta, inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  table test_table {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
      hdr.ethernet.etherType: exact;
    }
    default_action = nop();
  }
  apply {
    test_table.apply();
    if(meta.color != COLOR_GREEN) {
      standard_metadata.drop = 1;
    }
  }
}

V1Switch(ParserImpl(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
