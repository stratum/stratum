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

// This is a simple P4 program for stacked header field decoding tests.
#include <core.p4>
#include <v1model.p4>

struct dummy_metadata_t {
  bit<32> data;
}

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

// This test-dependent header type prevents use of simple_headers.p4.
header stacked_header_t {
  bit<4>  f1;
  bit<4>  f2;
  bit<8>  f3;
  bit<16> f4;
}

struct metadata {
  dummy_metadata_t dummy_metadata;
}

struct headers {
  ethernet_t ethernet;
  stacked_header_t[4] stacked;
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
  table t1_with_stacked_key {
    actions = {
      nop;
    }
    key = {
      hdr.stacked[2].f4: exact;
    }
    default_action = nop();
  }
  apply {
    t1_with_stacked_key.apply();
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
