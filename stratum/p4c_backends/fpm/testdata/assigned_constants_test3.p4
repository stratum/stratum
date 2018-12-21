// This is a .p4 file that AssignedConstantInspector uses to verify various
// assignment statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// Every action in the ingress control assigns a different constant value to
// the same metadata field.  The ingress control also assigns an initial
// constant value to the field.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action assign1() {
    meta.other_metadata = 1;
  }
  action assign2() {
    meta.other_metadata = 2;
  }
  action assign3() {
    meta.other_metadata = 3;
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
    meta.other_metadata = 0;
    dummy_table.apply();
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
