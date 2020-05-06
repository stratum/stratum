// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a test .p4 file to make sure the midend pass is not introducing
// temporary tables and actions that clutter the p4c output.  The open source
// midends generate these temporaries to help the bmv2 pipeline.  In this
// file, the open source midends added a table and action for the if statement
// after the test_table.apply() below.  The Stratum custom midend should not
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
      mark_to_drop(standard_metadata);
    }
  }
}

V1Switch(ParserImpl(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
