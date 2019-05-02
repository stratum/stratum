// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file is a P4 parser setup for generating a P4Parser IR file to
// test the ParserDecoder and ParserFieldMapper.  The overall P4 headers
// declaration includes a second ethernet_t header that is not extracted
// by any parser state.  It mimics the ERSPAN headers in Stratum P4 programs.

#include <core.p4>
#include <v1model.p4>

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

struct test_metadata_t {
  bit<32> dummy;
}

struct headers {
  ethernet_t non_extracted_ethernet;
  ethernet_t ethernet;
}

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  state start {
    transition select(standard_metadata.ingress_port) {
      1234 : parse_cpu_header;
      _    : parse_ethernet;
    }
  }

  state parse_cpu_header {
    transition accept;
  }

  state parse_ethernet {
    packet.extract(hdr.ethernet);
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
    packet.emit(hdr.non_extracted_ethernet);
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
