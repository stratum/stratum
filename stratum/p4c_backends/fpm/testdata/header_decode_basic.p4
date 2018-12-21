// This is a simple P4 program for basic header field decoding tests.
#include <core.p4>
#include <v1model.p4>

struct routing_metadata_t {
  bit<32> nhop_ipv4;
}

// This test uses more complex header types than those available in
// simple_headers.p4.
header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

header ipv4_t {
  bit<4>  version;
  bit<4>  ihl;
  bit<8>  diffserv;
  bit<16> totalLen;
  bit<16> identification;
  bit<3>  flags;
  bit<13> fragOffset;
  bit<8>  ttl;
  bit<8>  protocol;
  bit<16> hdrChecksum;
  bit<32> srcAddr;
  bit<32> dstAddr;
}

struct metadata {
  routing_metadata_t routing_metadata;
}

struct headers {
  ethernet_t ethernet;
  ipv4_t     ipv4;
}

parser ParserImpl(packet_in packet, out headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
  apply {}
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  table t1_with_lots_of_match_key_tests {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
      hdr.ethernet.etherType: exact;
      hdr.ipv4.dstAddr: lpm;
      hdr.ipv4.srcAddr: ternary;
    }
    default_action = nop();
  }
  apply {
    t1_with_lots_of_match_key_tests.apply();
  }
}

control DeparserImpl(packet_out packet, in headers hdr) {
  apply {}
}

control verifyChecksum(inout headers hdr, inout metadata meta) {
  apply {}
}

control computeChecksum(inout headers hdr, inout metadata meta) {
  apply {}
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(), computeChecksum(), DeparserImpl()) main;
