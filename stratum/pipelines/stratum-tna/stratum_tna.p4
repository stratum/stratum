// Copyright 2021-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <core.p4>
#include <tna.p4>

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;

const bit<16> ETHERTYPE_IPV4 = 0x0800;
const bit<16> ETHERTYPE_ARP  = 0x0806;
const bit<8> PROTO_ICMP = 0x01;
const bit<16> ARP_OPERATION_REQUEST = 1;
const bit<16> ARP_OPERATION_REPLY = 2;
const bit<32> DEFAULT_TBL_SIZE = 1024;
const bit<16> ETHERTYPE_PACKET_OUT = 0xBF01;

@controller_header("packet_in")
header packet_in_header_t {
    @padding bit<7> pad0;
    PortId_t        ingress_port;
}

// This header must have a pseudo ethertype at offset 12, to be parseable as an
// Ethernet frame in the ingress parser when comming from the CPU port.
@controller_header("packet_out")
header packet_out_header_t {
    PortId_t         egress_port;
    @padding bit<87> pad0;
    bit<16>          ether_type;
}

header ethernet_t {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> eth_type;
}

header arp_t {
    bit<16>     hw_type;
    bit<16>     eth_type;
    bit<16>     hw_proto_addr_len;
    bit<16>     oper_code;
    mac_addr_t  hw_src_addr;
    ipv4_addr_t proto_src_addr;
    mac_addr_t  hw_dst_addr;
    ipv4_addr_t proto_dst_addr;
}

header ipv4_t {
    bit<8>      version_ihl;
    bit<8>      dscp_ecn;
    bit<16>     total_len;
    bit<16>     identification;
    bit<3>      flags;
    bit<13>     frag_offset;
    bit<8>      ttl;
    bit<8>      protocol;
    bit<16>     hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header icmp_t {
    bit<8>  icmp_type;
    bit<8>  icmp_code;
    bit<16> checksum;
    bit<16> identifier;
    bit<16> sequence_number;
    bit<64> timestamp;
}

header bridged_metadata_t {
    @padding bit<7> pad0;
    PortId_t        ig_port;
}

@flexible
struct stratum_ingress_metadata_t {
    bridged_metadata_t bridged;
}

@flexible
struct stratum_egress_metadata_t {
    bridged_metadata_t bridged;
    PortId_t           cpu_port;
}

struct headers_t {
    ethernet_t          ethernet;
    arp_t               arp;
    ipv4_t              ipv4;
    icmp_t              icmp;
    packet_out_header_t packet_out;
    packet_in_header_t  packet_in;
}

struct digest_macs_t {
    // mac_addr_t dst_addr;
    PortId_t port;
    mac_addr_t src_addr;
}

parser StratumIngressParser(packet_in pkt,
                            out headers_t hdr,
                            out stratum_ingress_metadata_t stratum_md,
                            out ingress_intrinsic_metadata_t ig_intr_md) {
    // Checksum() ipv4_sum;
    state start {
        pkt.extract(ig_intr_md);
        pkt.advance(PORT_METADATA_SIZE);
        stratum_md = {{0, 0}};
        stratum_md.bridged.setValid();
        stratum_md.bridged.ig_port = ig_intr_md.ingress_port;
        transition check_ethernet;
    }

    state check_ethernet {
        // We use ethernet-like headers to signal the presence of PacketOut
        // metadata before the actual ethernet frame.
        ethernet_t tmp = pkt.lookahead<ethernet_t>();
        transition select(tmp.eth_type) {
            ETHERTYPE_PACKET_OUT: parse_packet_out;
            default: parse_ethernet;
        }
    }

    state parse_packet_out {
        pkt.extract(hdr.packet_out);
        // Will send to requested egress port as-is. No need to parse further.
        transition accept;
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
        transition select (hdr.ipv4.protocol) {
            PROTO_ICMP: parse_icmp;
            default: accept;
        }
    }

    state parse_icmp {
        pkt.extract(hdr.icmp);
        transition accept;
    }
}

control StratumIngress(
        inout headers_t hdr,
        inout stratum_ingress_metadata_t stratum_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    Counter<bit<64>, PortId_t>(512, CounterType_t.PACKETS_AND_BYTES) rx_port_counter;
    Counter<bit<64>, PortId_t>(512, CounterType_t.PACKETS_AND_BYTES) tx_port_counter;

    DirectCounter<bit<64>>(CounterType_t.PACKETS_AND_BYTES) ipv4_counter;

    // Meter< bit<16> > (512, MeterType_t.BYTES) test_meter;
    DirectMeter(MeterType_t.BYTES) host_bytes;

    action color_source() {
        hdr.ipv4.dscp_ecn = host_bytes.execute();
    }

    action missed_source() {
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

    DirectCounter<bit<64>>(CounterType_t.PACKETS) arp_counter;

    action reply_station_arp(mac_addr_t target_mac) {
	      arp_counter.count();
        hdr.arp.oper_code = ARP_OPERATION_REPLY;
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
        size = 512;
    }

    // table icmp {
    //     key = {
    //         // hdr.icmp.
    //     }
    //     actions = {
    //     }
    // }

    apply {
        rx_port_counter.count(ig_intr_md.ingress_port);

        if (hdr.packet_out.isValid()) {
            ig_intr_tm_md.ucast_egress_port = hdr.packet_out.egress_port[8:0];
            hdr.packet_out.setInvalid();
            ig_intr_tm_md.bypass_egress = 1;
            stratum_md.bridged.setInvalid();
            // No need for further ingress processing, straight to egress.
            exit;
        }
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
        tx_port_counter.count(ig_intr_tm_md.ucast_egress_port);
    }
}

control StratumIngressDeparser(
        packet_out pkt,
        inout headers_t hdr,
        in stratum_ingress_metadata_t stratum_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    // Digest<digest_macs_t> () digest_macs;
    apply {
        pkt.emit(stratum_md.bridged);
        pkt.emit(hdr);
    }
}

parser StratumEgressParser(packet_in pkt,
                           out headers_t hdr,
                           out stratum_egress_metadata_t stratum_md,
                           out egress_intrinsic_metadata_t eg_intr_md) {
    state start {
        stratum_md = {{0, 0}, 0};
        pkt.extract(eg_intr_md);
        pkt.extract(stratum_md.bridged);
        transition accept;
    }
}

control StratumEgress(
        inout headers_t hdr,
        inout stratum_egress_metadata_t stratum_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_prsr_md,
        inout egress_intrinsic_metadata_for_deparser_t eg_dprsr_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) {

    action set_switch_info(PortId_t cpu_port) {
        stratum_md.cpu_port = cpu_port;
    }

    table switch_info {
        actions = {
            set_switch_info;
            @defaultonly NoAction;
        }
        default_action = NoAction();
        const size = 1;
    }

    apply {
        switch_info.apply();
        if (eg_intr_md.egress_port == stratum_md.cpu_port) {
            hdr.packet_in.setValid();
            hdr.packet_in.ingress_port = stratum_md.bridged.ig_port;
            exit;
        }
    }
}

control StratumEgressDeparser(
        packet_out pkt,
        inout headers_t hdr,
        in stratum_egress_metadata_t md,
        in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {
    apply {}
}

Pipeline(
    StratumIngressParser(),
    StratumIngress(),
    StratumIngressDeparser(),
    StratumEgressParser(),
    StratumEgress(),
    StratumEgressDeparser()
) pipe;

Switch(pipe) main;
