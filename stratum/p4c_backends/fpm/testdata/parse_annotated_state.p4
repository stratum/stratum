// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

// This P4 file tests correct state name extraction for states with
// @name annotations. It checks this by implementing a parser, that
// matches against the standard parser map.

#include <core.p4>
#include <v1model.p4>

header ethernet_t {
    bit<48> dstAddr;
    bit<48> srcAddr;
    bit<16> ethertype;
}

header vlan_tag_t {
}

struct metadata {
}

struct headers {
    ethernet_t ethernet;
    vlan_tag_t vlan_tag;
}

parser ParserImpl(packet_in packet, out headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    state start {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.ethertype) {
            0x8100: parse_vlan_tag;
            default: accept;
        }
    }
    @name("parse_vlan_tag")
    state parse_vlan_tag {
        transition accept;
    }
}

control egress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    action nop() {
    }
    table t2 {
        actions = {
            nop;
            @default_only NoAction;
        }
        key = {
            hdr.ethernet.ethertype: exact;
        }
        default_action = NoAction();
    }
    apply {
        t2.apply();
    }
}

control ingress(inout headers hdr, inout metadata meta, inout standard_metadata_t standard_metadata) {
    apply {
    }
}

control DeparserImpl(packet_out packet, in headers hdr) {
    apply {
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
