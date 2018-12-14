// This is a .p4 file that PipelinePassesTest uses to verify the ability to
// detect controls that can be optimized.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The egress control contains a table in an "unoptimized" ACL pipeline stage.
control egress(inout headers hdr, inout test_metadata_t meta,
               inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  @switchstack("pipeline_stage: EGRESS_ACL")
  table egress_table {
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
    egress_table.apply();
  }
}

// The ingress control contains two tables, one of which maps to a physical
// table in a fixed pipeline stage ("pipeline_stage: L2").
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
      hdr.ethernet.etherType: exact;
    }
    default_action = nop();
  }
  @switchstack("pipeline_stage: L2")
  table fixed_table {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
    }
    default_action = nop();
  }
  apply {
    if (!fixed_table.apply().hit) {
      acl_table.apply();
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress(),
         compute_checksum_stub(), deparser_stub()) main;
