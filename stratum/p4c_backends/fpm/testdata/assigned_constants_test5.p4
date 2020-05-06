// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that AssignedConstantInspector uses to verify various
// assignment statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The ingress control tests constant assignments to multiple metadata fields.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action assign1() {
    meta.other_metadata = 1;
    meta.smaller_metadata = 500;
  }
  action assign2() {
    meta.other_metadata = 2;
  }
  action assign3() {
    meta.other_metadata = 3;
    meta.smaller_metadata = 300;
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table dummy_table {
    actions = {
      assign1;
      assign2;
      assign3;
    }
    key = {
      hdr.ethernet.etherType: exact;
    }
  }
  apply {
    meta.smaller_metadata = 0;
    dummy_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
