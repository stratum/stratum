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
      mark_to_drop();
    }
  }
}

V1Switch(parser_stub(), verify_checksum_stub(), basic_hit(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
