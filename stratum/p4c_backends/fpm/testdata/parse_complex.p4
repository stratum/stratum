// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0

// This file contains a P4 parser setup for generating a P4Parser IR file to
// test the ParserDecoder.  This parser uses two variations of a "complex"
// select expression using multiple fields.  The first uses a select expression
// with 2 comma-separated fields as the select paramaters, i.e. select(f1, f2).
// The second uses 2 fields joined by the P4 concatenate operator, i.e.
// select(f1 ++ f2).  This file produces an IR for testing via the p4c_save_ir
// BUILD rule.
#include <core.p4>
#include <v1model.p4>

header test_header_t {
    bit<16> f1;
    bit<16> f2;
}

// This test has its own headers and metadata, so it can't use control_stubs.p4.
struct metadata {
}

struct headers {
    @name("test_header")
    test_header_t test_header;
}

parser ParserImpl(packet_in packet, out headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    @name("parse_nop1") state parse_nop1 {
        transition accept;
    }
    @name("parse_nop2") state parse_nop2 {
        transition accept;
    }
    @name("start") state start {
        packet.extract(hdr.test_header);
        transition select(hdr.test_header.f1, hdr.test_header.f2) {
            (16w0xf101, 16w0xf201): parse_concat;
            (16w0xf102, 16w0xf202): parse_nop2;
            default: accept;
        }
    }
    @name("parse_concat") state parse_concat {
        packet.extract(hdr.test_header);
        transition select(hdr.test_header.f1 ++ hdr.test_header.f2) {
            0xf101f201 : parse_nop1;
            0xf102f202 : parse_nop2;
            default: accept;
        }
    }
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    @name(".do_nop") action do_nop() {
    }
    @name("do_nothing") table do_nothing {
        actions = {
            do_nop;
            @default_only NoAction;
        }
        key = {
            hdr.test_header.f1: exact;
        }
        default_action = NoAction();
    }
    apply {
        do_nothing.apply();
    }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    apply {
    }
}

control DeparserImpl(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.test_header);
    }
}

control verifyChecksum(inout headers hdr, inout metadata meta) {
    apply {
    }
}

control computeChecksum(inout headers hdr, inout metadata meta) {
    apply {
    }
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(), computeChecksum(), DeparserImpl()) main;
