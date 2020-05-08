// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that HiddenTableMapper uses to verify the
// ability to map hidden tables with local metadata keys .

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The ingress control contains 2 table sets.  The first table in each set
// has actions that assign a metadata value that subsequently becomes the key
// to the remaining hidden tables in the set.  This table structure simulates
// the relationships of the encap and decap tables in Stratum P4 programs.
// The hidden tables need const entries, which must be inserted by the unit
// test setup code according to test conditions.
control ingress(inout headers hdr, inout test_metadata_t test_meta,
                inout standard_metadata_t standard_metadata) {
  action set_decap_key_1() {
    test_meta.smaller_metadata = 1;
  }
  action set_decap_key_2() {
    test_meta.smaller_metadata = 2;
  }
  action set_encap_key_1() {
    test_meta.other_metadata = 1;
  }
  action set_encap_key_2() {
    test_meta.other_metadata = 2;
  }
  action nop() {
  }

  @switchstack("pipeline_stage: VLAN_ACL")
  table decap_table {
    actions = {
      set_decap_key_1;
      set_decap_key_2;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }

  @switchstack("pipeline_stage: HIDDEN")
  table hidden_decap_table {
    actions = {
      nop;
    }
    key = {
      test_meta.smaller_metadata: exact;
    }
  }

  @switchstack("pipeline_stage: VLAN_ACL")
  table encap_table {
    actions = {
      set_encap_key_1;
      set_encap_key_2;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }

  @switchstack("pipeline_stage: HIDDEN")
  table hidden_encap_table_v4 {
    actions = {
      nop;
    }
    key = {
      test_meta.other_metadata: exact;
    }
  }

  @switchstack("pipeline_stage: HIDDEN")
  table hidden_encap_table_v6 {
    actions = {
      nop;
    }
    key = {
      test_meta.other_metadata: exact;
    }
  }

  apply {
    if (decap_table.apply().hit) {
      hidden_decap_table.apply();
    } else if (encap_table.apply().hit) {
      hidden_encap_table_v4.apply();
      hidden_encap_table_v6.apply();
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
