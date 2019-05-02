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

// This is a simple P4 program for basic header field decoding tests.
// %%% try some stacks!
#include <core.p4>
#include <v1model.p4>

struct metadata1_t {
  bit<32> data1;
}

struct metadata2_t {
  bit<32> data2;
}

struct metadata_outer_t {
  metadata1_t meta1;
  metadata2_t meta2;
  bit<32> meta3;
}

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

// This test-dependent metadata prevents use of simple_headers.p4.
struct metadata_t {
  metadata_outer_t meta_outer;
  metadata1_t metadata1;
  bit<32> meta_bits;
}

struct headers {
  ethernet_t ethernet;
}

parser ParserImpl(packet_in packet, out headers hdr, inout metadata_t meta, inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

control egress(inout headers hdr, inout metadata_t meta, inout standard_metadata_t standard_metadata) {
  apply {}
}

control ingress(inout headers hdr, inout metadata_t meta, inout standard_metadata_t standard_metadata) {
  action nop() {
  }
  table t1_with_lots_of_match_key_tests {
    actions = {
      nop;
    }
    key = {
      hdr.ethernet.dstAddr: exact;
      hdr.ethernet.etherType: exact;
      meta.meta_outer.meta2.data2: exact;
      meta.meta_outer.meta3: exact;
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

control verifyChecksum(inout headers hdr, inout metadata_t meta) {
  apply {}
}

control computeChecksum(inout headers hdr, inout metadata_t meta) {
  apply {}
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(), computeChecksum(), DeparserImpl()) main;
