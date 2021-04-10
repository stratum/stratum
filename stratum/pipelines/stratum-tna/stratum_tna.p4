// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0
#include <core.p4>
#include <tna.p4>

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;

const bit<16> ETHERTYPE_IPV4 = 0x0800;
const bit<16> ETHERTYPE_ARP  = 0x0806;
const bit<8> PROTO_ICMP = 0x01;
const bit<32> DEFAULT_TBL_SIZE = 1024;

header ethernet_t {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> eth_type;
}

header arp_t {
    bit<16> hw_type;
    bit<16> eth_type;
    bit<16> hw_proto_addr_len;
    bit<16> oper_code;
    mac_addr_t  hw_src_addr;
    ipv4_addr_t proto_src_addr;
    mac_addr_t  hw_dst_addr;
    ipv4_addr_t proto_dst_addr;
}

header ipv4_t {
    bit<8>  version_ihl;
    bit<8>  dscp_ecn;
    bit<16> total_len;
    bit<16> identification;
    bit<3>  flags;
    bit<13> frag_offset;
    bit<8>  ttl;
    bit<8>  protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header icmp_t {
    bit<8> icmp_type;
    bit<8> icmp_code;
    bit<16> checksum;
    bit<16> identifier;
    bit<16> sequence_number;
    bit<64> timestamp;
}

struct empty_t {} // no additions to this struct

struct metadata_t {}
struct headers_t {
    ethernet_t ethernet;
    arp_t arp;
    ipv4_t ipv4;
    icmp_t icmp;
}
//
// struct digest_macs_t {
//     // mac_addr_t dst_addr;
//     PortId_t port;
//     mac_addr_t src_addr;
// }

parser StratumIParser( packet_in pkt, 
    out headers_t hdr, 
    out metadata_t md, 
    out ingress_intrinsic_metadata_t ig_intr_md ) {

    // Checksum() ipv4_sum;
    state start {
        pkt.extract(ig_intr_md);
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }
    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select (hdr.ethernet.eth_type){
            ETHERTYPE_IPV4: parse_ipv4;
            ETHERTYPE_ARP: parse_arp;
            default: reject;
        }
    }
    state parse_arp {
        pkt.extract(hdr.arp);
        transition select(hdr.arp.eth_type){
            ETHERTYPE_IPV4: accept;
            default: reject;
        }
    }
    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        // ipv4_sum.add(hdr.ipv4);
        transition select (hdr.ipv4.protocol){
            PROTO_ICMP: parse_icmp;
            default: accept;
        }
    }
    state parse_icmp {
        pkt.extract(hdr.icmp);
        transition accept;
    }
}

control StratumI (
    inout headers_t hdr, 
    inout metadata_t md, 
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_prsr_md, 
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) { 

    Counter< bit<64>, PortId_t >
        ( 512, CounterType_t.PACKETS_AND_BYTES ) rx_port_counter;
    Counter< bit<64>, PortId_t >
        ( 512, CounterType_t.PACKETS_AND_BYTES ) tx_port_counter;

    DirectCounter< bit<64> >
        ( CounterType_t.PACKETS_AND_BYTES ) ipv4_counter;
    DirectCounter< bit<64> >
        ( CounterType_t.PACKETS ) arp_counter;

    // Meter< bit<16> > ( 512, MeterType_t.BYTES ) test_meter;
    DirectMeter( MeterType_t.BYTES ) host_bytes;

    action color_source(){
        hdr.ipv4.dscp_ecn = host_bytes.execute();
    }
    action missed_source(){
        // hdr.ipv4.dscp_ecn = host_bytes.execute();
        // punt ?
    }
    table host_mac {
        key = {
            hdr.ethernet.src_addr: exact;
        }
        actions = {
            color_source;
            missed_source; 
        }
        meters = host_bytes;
        size = DEFAULT_TBL_SIZE;
        default_action = missed_source;
    }

    action fwd_route(PortId_t port, mac_addr_t dmac) {
	ipv4_counter.count();
        ig_intr_tm_md.ucast_egress_port = port;
        ig_intr_dprsr_md.drop_ctl = 0x0;
        hdr.ethernet.dst_addr = dmac;
        hdr.ipv4.ttl = hdr.ipv4.ttl + 255;
    }
    table ipv4_route {
        key = {
            hdr.ipv4.dst_addr : lpm;
        }
        actions = {
            fwd_route;
        }
        size = DEFAULT_TBL_SIZE;
        counters = ipv4_counter;
    }
    action reply_station_arp(mac_addr_t target_mac ) {
	arp_counter.count();
        hdr.arp.oper_code = 2; // reply code
        hdr.ethernet.src_addr = target_mac;
        hdr.ethernet.dst_addr = hdr.arp.hw_dst_addr;
        hdr.arp.hw_dst_addr = hdr.ethernet.dst_addr;
        hdr.arp.hw_src_addr = target_mac;

        bit<32> tmpAddr = hdr.arp.proto_src_addr;
        hdr.arp.proto_src_addr = hdr.arp.proto_dst_addr;
        hdr.arp.proto_dst_addr = tmpAddr;

        ig_intr_tm_md.ucast_egress_port = ig_intr_md.ingress_port;
        // ig_intr_dprsr_md.drop_ctl = 0x0;

    }
    table arp_station {
        key = {
            hdr.arp.oper_code: exact;
            hdr.arp.proto_dst_addr: exact; // must target switch station mac
        }
        actions = {
            reply_station_arp;
        }
        counters = arp_counter;
    }
    // table icmp {
    //     key = {
    //         // hdr.icmp.
    //     }
    //     actions = {
    //     }
    // }

    apply{
        rx_port_counter.count(ig_intr_md.ingress_port);

        if (hdr.arp.isValid()) {
            arp_station.apply();
            exit;
        }
        if (hdr.ipv4.isValid()) {
            ipv4_route.apply();
        }
        // if (hdr.icmp.isValid()) {
        //     icmp.apply();
        //     exit;
        // }
        //switch (host_mac.apply().action_run) {
            //color_source: {ipv4_route.apply(); }
            // missed_source: {ipv4_route.apply(); }
        //}
        ig_intr_tm_md.bypass_egress = 1w1;
        tx_port_counter.count( ig_intr_tm_md.ucast_egress_port);
    }
}

control StratumIDeparser( packet_out pkt, inout headers_t hdr, in metadata_t md, in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) { 
    // Digest<digest_macs_t> () digest_macs;
    apply{
        pkt.emit(hdr);
    } 
}

parser StratumEParser( packet_in pkt, out empty_t hdr, out empty_t md, out egress_intrinsic_metadata_t eg_intr_md  ){state start{transition accept;}}
control StratumE (inout empty_t hdr, inout empty_t md,
		  in egress_intrinsic_metadata_t eg_intr_md, 
		  in egress_intrinsic_metadata_from_parser_t eg_prsr_md, 
	          inout egress_intrinsic_metadata_for_deparser_t eg_dprsr_md, 
		  inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) { apply{ } }

control StratumEDeparser( packet_out pkt, inout empty_t hdr, in empty_t md, in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md){ apply{} }



Pipeline(
    StratumIParser(),
    StratumI(),
    StratumIDeparser(),
    StratumEParser(),
    StratumE(),
    StratumEDeparser()
) pipe;

Switch(pipe) main;
