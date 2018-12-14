// This is a .p4 file that provides a sample program for the p4c design
// documentation.  A text dump of the IR for this P4 program resides in
// design_doc_sample1_ir.txt in this directory.  The design doc [will] refer
// to both of these files.  If changes to this program occur, the IR text
// file must also be updated.

#include <core.p4>
#include <v1model.p4>

#include "control_stubs.p4"
#include "simple_headers.p4"

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

// The ingress control provides a simple sample of control logic.
control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
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
    if (!test_table.apply().hit) {
      mark_to_drop();
    }
  }
}

V1Switch(ParserImpl(), verify_checksum_stub(), ingress(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
