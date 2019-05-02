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

// This is a .p4 file that ControlInspectorTest uses to verify if-statement
// decoding.
#include <core.p4>
#include <v1model.p4>

#define COLOR_GREEN  1
#define COLOR_RED    2
#define COLOR_YELLOW 3

struct test_metadata_t {
  bit<32> color;
}

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

struct headers {
  ethernet_t ethernet;
}

parser ParserImpl(packet_in packet, out headers hdr, inout test_metadata_t meta,
                  inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

// The egress control contains a simple if statement with no else.
control egress(inout headers hdr, inout test_metadata_t meta,
               inout standard_metadata_t standard_metadata) {
  apply {
    if (hdr.ethernet.isValid()) {
      mark_to_drop();
    }
  }
}

// The ingress control contains a basic if-else statement.
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
    if (test_table.apply().hit) {
      meta.color = COLOR_GREEN;
    } else {
      standard_metadata.drop = 1;
    }
  }
}

// The deparser control currently has no inputs for tests.
control DeparserImpl(packet_out packet, in headers hdr) {
  apply {}
}

// TODO: Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The checksum verify control provides a test of the unary not operator.
control verifyChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (!hdr.ethernet.isValid()) {
      meta.color = COLOR_RED;
    }
  }
}

// The checksum compute control provides a test of nested if statements.
control computeChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (hdr.ethernet.isValid()) {
      if (hdr.ethernet.etherType == 0x8100)
        meta.color = COLOR_GREEN;
      else
        meta.color = COLOR_YELLOW;
    } else {
      if (hdr.ethernet.etherType != 0x806)
        meta.color = COLOR_RED;
    }
  }
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(),
         computeChecksum(), DeparserImpl()) main;
