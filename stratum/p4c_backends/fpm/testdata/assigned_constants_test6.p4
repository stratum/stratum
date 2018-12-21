// This is a .p4 file that AssignedConstantInspector uses to verify various
// assignment statements.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

// The actions in the ingress control assign expressions that contain constant
// mixed with other types.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  action assign1() {
    meta.color = 123;
    meta.other_metadata = 10000;
    meta.smaller_metadata = 1000;
  }
  action assign2(bit<32> big_param, bit<16> small_param) {
    meta.color = 321;
    meta.other_metadata = 2 + big_param;
    meta.smaller_metadata = small_param | 10;
  }
  @switchstack("pipeline_stage: VLAN_ACL")
  table dummy_table {
    actions = {
      assign1;
      assign2;
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
