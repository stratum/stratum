// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file is a P4 parser setup for generating a P4Parser IR file to
// test the ParserDecoder and ParserFieldMapper.  It includes a small-scale
// parser value set similar in usage to the Stratum P4 programs.

#include <core.p4>
#include <v1model.p4>

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

header udf_payload_t {
  bit<8> udf_data;
}

struct test_metadata_t {
  bit<32> color;
  bit<32> other_metadata;
  bit<16> smaller_metadata;
  bit<8> udf_1;
  bit<8> udf_2;
}

struct headers {
  ethernet_t ethernet;
  udf_payload_t[10] udf_payload;
}

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  bit<5> udf_payload_index = 5w31;
  value_set<bit<5>>(2) udf_vs_1;
  value_set<bit<5>>(2) udf_vs_2;

  state start {
    transition select(standard_metadata.ingress_port) {
      123 : parse_cpu_header;
      _    : parse_ethernet;
    }
  }

  state parse_cpu_header {
    transition accept;
  }

  state parse_ethernet {
    packet.extract(hdr.ethernet);
    transition parse_udf_payload;
  }

  state parse_udf_payload {
    packet.extract(hdr.udf_payload.next);
    udf_payload_index = udf_payload_index + 1;
    transition select (udf_payload_index) {
      udf_vs_1: parse_udf_1;
      udf_vs_2: parse_udf_2;
      10: accept;
      default: parse_udf_payload;
    }
  }

  state parse_udf_1 {
    meta.udf_1 = hdr.udf_payload.last.udf_data;
    transition parse_udf_payload;
  }

  state parse_udf_2 {
    meta.udf_2 = hdr.udf_payload.last.udf_data;
    transition parse_udf_payload;
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
      packet.emit(hdr.ethernet);
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
