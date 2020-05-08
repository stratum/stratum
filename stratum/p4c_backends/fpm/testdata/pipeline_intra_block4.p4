// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that PipelineIntraBlockPassesTest uses to verify the
// ability to detect controls that can be optimized.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The ingress control contains a VLAN_ACL table and two L2 tables.
// The L2 tables are separated by a true block in an IfStatement that has no
// impact on the L2 pipeline, so both tables should optimize into an L2
// pipeline statement.
control ingress(inout headers hdr, inout test_metadata_t meta,
               inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table acl_table {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
      hdr.ethernet.etherType: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L2")
  table mac_table_1 {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L2")
  table mac_table_2 {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  apply {
    acl_table.apply();
    mac_table_1.apply();
    if (hdr.ethernet.isValid()) {
      meta.color = 1;
      meta.enum_color = meter_color_t.COLOR_YELLOW;
    }
    mac_table_2.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
