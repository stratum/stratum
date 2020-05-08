// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This is a .p4 file that ControlInspectorTest uses to verify miscellaneous
// statements that aren't covered by the other control test files.
#include <core.p4>
#include <v1model.p4>

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

// The egress control tests a P4 built-in method call, other than isValid.
control egress(inout headers hdr, inout test_metadata_t meta,
               inout standard_metadata_t standard_metadata) {
  apply {
    hdr.ethernet.setInvalid();
  }
}

// The ingress control currently has no inputs for tests.
control ingress(inout headers hdr, inout test_metadata_t meta, inout standard_metadata_t standard_metadata) {
  apply {}
}

// The deparser control tests a drop decision.
control DeparserImpl(packet_out packet, in headers hdr) {
  apply {
    if (!hdr.ethernet.isValid()) {
      mark_to_drop();
    }
  }
}

// TODO(unknown): Given recent instability in the P4 checksum API and
// likely additional turbulence through PSA, consider moving these tests
// to some other control.
// The checksum verify control tests assignment of a constant to a header field.
control verifyChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
    meta.color = COLOR_YELLOW;
  }
}

// The checksum compute control tests a field comparison condition.
control computeChecksum(inout headers hdr, inout test_metadata_t meta) {
  apply {
    if (meta.color == COLOR_RED) {
      mark_to_drop();
    }
  }
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(),
         computeChecksum(), DeparserImpl()) main;
