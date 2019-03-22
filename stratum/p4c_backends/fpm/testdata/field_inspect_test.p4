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

// This test file supports FieldNameInspectorTest.  It references stacked and
// non-stacked header fields from actions and as table match keys.
#include <core.p4>
#include <v1model.p4>

typedef bit<48> EthernetAddress;

header ethernet_t {
    EthernetAddress dstAddr;
    EthernetAddress srcAddr;
    bit<16> ethertype;
}

// This test-dependent header type prevents use of simple_headers.p4.
header vlan_tag_t {
    bit<3>  pcp;
    bit<1>  cfi;
    bit<12> vlan_id;
    bit<16> ethertype;
}

struct local_metadata_t {
}

struct headers {
    @name("ethernet")
    ethernet_t    ethernet;
    @name("vlan_tag")
    vlan_tag_t[5] vlan_tag;
}

parser ParserImpl(packet_in packet, out headers hdr, inout local_metadata_t meta, inout standard_metadata_t standard_metadata) {
    @name("parse_ethernet") state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition select(hdr.ethernet.ethertype) {
            16w0x8100 &&& 16w0xefff: parse_vlan_tag;
            default: accept;
        }
    }
    @name("parse_vlan_tag") state parse_vlan_tag {
        packet.extract(hdr.vlan_tag.next);
        transition select(hdr.vlan_tag.last.ethertype) {
            16w0x8100 &&& 16w0xefff: parse_vlan_tag;
            default: accept;
        }
    }
    @name("start") state start {
        transition parse_ethernet;
    }
}

control egress(inout headers hdr, inout local_metadata_t meta, inout standard_metadata_t standard_metadata) {
    action set_field(EthernetAddress mac) {
        hdr.ethernet.dstAddr = mac;
    }
    @name("t2") table t2 {
        actions = {
            set_field;
            @default_only NoAction;
        }
        key = {
            hdr.ethernet.srcAddr: exact;
        }
        default_action = NoAction();
    }
    apply {
        t2.apply();
    }
}

control ingress(inout headers hdr, inout local_metadata_t meta, inout standard_metadata_t standard_metadata) {
    action set_outer_vlan(bit<12> vlan) {
        hdr.vlan_tag[0].vlan_id = vlan;
    }
    @name("t1") table t1 {
        actions = {
            set_outer_vlan;
            @default_only NoAction;
        }
        key = {
            hdr.ethernet.dstAddr: exact;
            hdr.vlan_tag[0].vlan_id: exact;
        }
        default_action = NoAction();
    }
    apply {
        t1.apply();
    }
}

control DeparserImpl(packet_out packet, in headers hdr) {
    apply {
        packet.emit(hdr.ethernet);
        packet.emit(hdr.vlan_tag);
    }
}

control verifyChecksum(inout headers hdr, inout local_metadata_t meta) {
    apply {
    }
}

control computeChecksum(inout headers hdr, inout local_metadata_t meta) {
    apply {
    }
}

V1Switch(ParserImpl(), verifyChecksum(), ingress(), egress(), computeChecksum(), DeparserImpl()) main;
