// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file is a simple P4 parser setup for generating a P4Parser IR file to
// test the ParserDecoder.

#include <core.p4>
#include <v1model.p4>

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

header vlan_tag_t {
  bit<3> pcp;
  bit<1> cfi;
  bit<12> vid;
  bit<16> etherType;
}

header l3_protocol_1_t {
  bit<16> f1_1;
  bit<8> f1_2;
  bit<5> f1_3;
  bit<1> f1_4;
  bit<2> f1_5;
}

header l3_protocol_2_t {
  bit<4> f2_1;
  bit<4> f2_2;
  bit<24> f2_3;
}

struct test_metadata_t {
  bit<32> color;
  bit<32> other_metadata;
  bit<16> smaller_metadata;
}

struct headers {
  ethernet_t ethernet;
  vlan_tag_t vlan_tag;
  l3_protocol_1_t l3_protocol_1;
  l3_protocol_2_t l3_protocol_2;
}

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  state start {
    packet.extract(hdr.ethernet);
    transition select(hdr.ethernet.etherType) {
      0x809b : parse_l3_protocol_1;
      0x80f3 : parse_l3_protocol_2;
      0x8100 : parse_vlan;
      default: accept;
    }
  }

  state parse_vlan {
    packet.extract(hdr.vlan_tag);
    transition select(hdr.vlan_tag.etherType) {
      0x809b : parse_l3_protocol_1;
      0x80f3 : parse_l3_protocol_2;
      default: accept;
    }
  }

  state parse_l3_protocol_1 {
    packet.extract(hdr.l3_protocol_1);
    transition accept;
  }

  state parse_l3_protocol_2 {
    packet.extract(hdr.l3_protocol_2);
    transition accept;
  }
}

control ingress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  apply {
  }
}

control egress(inout headers hdr, inout test_metadata_t meta,
                inout standard_metadata_t standard_metadata) {
  apply {
  }
}

control DeparserImpl(packet_out packet, in headers hdr) {
  apply {
      packet.emit(hdr.vlan_tag);
  }
}

control verifyChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
  }
}

control computeChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
  }
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(),
         computeChecksum(), DeparserImpl()) main;
