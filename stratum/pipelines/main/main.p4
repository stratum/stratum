// Copyright 2019-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <core.p4>
#include <v1model.p4>

#ifndef CPU_PORT
// enforced by FPM / BCM
#define CPU_PORT 0xFD
#endif
#define VLAN_DEPTH 2

#ifndef CPU_MIRROR_SESSION_ID
// enforced by FPM / BCM
#define CPU_MIRROR_SESSION_ID 511
#endif

#include <core.p4>
#include <v1model.p4>

const bit<2> METER_GREEN = 0;

const bit<16> ETHERTYPE_VLAN1 = 0x8100;
const bit<16> ETHERTYPE_VLAN2 = 0x9100;
const bit<16> ETHERTYPE_VLAN3 = 0x9200;
const bit<16> ETHERTYPE_VLAN4 = 0x9300;
const bit<16> ETHERTYPE_IPV4 = 0x800;
const bit<16> ETHERTYPE_IPV6 = 0x86dd;
const bit<16> ETHERTYPE_ARP = 0x806;
const bit<16> ETHERTYPE_ND = 0x6007;
const bit<16> ETHERTYPE_LLDP = 0x88cc;

const bit<8> IP_PROTOCOLS_TCP = 6;
const bit<8> IP_PROTOCOLS_UDP = 17;
const bit<8> IP_PROTOCOLS_ICMP = 1;
const bit<8> IP_PROTOCOLS_ICMPv6 = 58;

typedef bit<48> EthernetAddress;
typedef bit<32> IPv4Address;
typedef bit<128> IPv6Address;
typedef bit<9> PortNum;

header ethernet_t {
    EthernetAddress dst_addr;
    EthernetAddress src_addr;
    bit<16>         ether_type;
}

header ipv4_base_t {
    bit<4>      version;
    bit<4>      ihl;
    bit<8>      diffserv;
    bit<16>     total_len;
    bit<16>     identification;
    bit<3>      flags;
    bit<13>     frag_offset;
    bit<8>      ttl;
    bit<8>      protocol;
    bit<16>     hdr_checksum;
    IPv4Address src_addr;
    IPv4Address dst_addr;
}

header ipv6_base_t {
    bit<4>      version;
    bit<8>      traffic_class;
    bit<20>     flow_label;
    bit<16>     payload_length;
    bit<8>      next_header;
    bit<8>      hop_limit;
    IPv6Address src_addr;
    IPv6Address dst_addr;
}

header udp_t {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> hdr_length;
    bit<16> checksum;
}

header tcp_t {
    bit<16> src_port;
    bit<16> dst_port;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4>  data_offset;
    bit<4>  res;
    bit<8>  flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

// Same for both ip v4 and v6
header icmp_header_t {
    bit<8>  icmp_type;
    bit<8>  code;
    bit<16> checksum;
}

header vlan_tag_t {
    bit<3>  pcp;
    bit<1>  cfi;
    bit<12> vid;
    bit<16> ether_type;
}

header arp_t {
    bit<16> hw_type;
    bit<16> proto_type;
    bit<8>  hw_addr_len;
    bit<8>  proto_addr_len;
    bit<16> opcode;
    bit<48> sender_hw_addr;
    bit<32> sender_proto_addr;
    bit<48> target_hw_addr;
    bit<32> target_proto_addr;
}

@controller_header("packet_in")
header packet_in_header_t {
    @switchstack("field_type: P4_FIELD_TYPE_INGRESS_PORT")
    bit<9>  ingress_physical_port;
    bit<7>  padding1;
    bit<32> ingress_logical_port;
    @switchstack("field_type: P4_FIELD_TYPE_EGRESS_PORT")
    bit<9>  target_egress_port;
    bit<7>  padding2;
}

@not_extracted_in_egress
@controller_header("packet_out")
header packet_out_header_t {
    @switchstack("field_type: P4_FIELD_TYPE_EGRESS_PORT")
    bit<9> egress_physical_port;
    bit<1> submit_to_ingress;
    bit<6> padding;
}

struct local_metadata_t {
    @switchstack("field_type: P4_FIELD_TYPE_VRF")
    bit<10> vrf_id;
    bit<8>  class_id;
    bit<5>  cpu_cos_queue_id;
    bit<1>  skip_egress;
    bit<9>  egress_spec_at_punt_match;
    @switchstack("field_type: P4_FIELD_TYPE_COLOR")
    bit<2>  color;
    bit<16> l4_src_port;
    bit<16> l4_dst_port;
    bit<8>  icmp_code;
    @switchstack("field_type: P4_FIELD_TYPE_L3_ADMIT")
    bit<1>  l3_admit;
    @switchstack("field_type: P4_FIELD_TYPE_VLAN_VID")
    bit<12> dst_vlan;
}

struct parsed_packet_t {
    ethernet_t          ethernet;
    ipv4_base_t         ipv4_base;
    ipv6_base_t         ipv6_base;
    icmp_header_t       icmp_header;
    tcp_t               tcp;
    udp_t               udp;
    vlan_tag_t[2]       vlan_tag;
    arp_t               arp;
    packet_in_header_t  packet_in;
    packet_out_header_t packet_out;
}

parser pkt_parser(packet_in pk,
                  out parsed_packet_t hdr,
                  inout local_metadata_t local_metadata,
                  inout standard_metadata_t standard_metadata) {

    state start {
        transition select(standard_metadata.ingress_port) {
            CPU_PORT: parse_cpu_header;
            default: parse_ethernet;
        }
    }

    state parse_cpu_header {
        pk.extract(hdr.packet_out);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pk.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_VLAN1: parse_vlan;
            ETHERTYPE_VLAN2: parse_vlan;
            ETHERTYPE_VLAN3: parse_vlan;
            ETHERTYPE_VLAN4: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            ETHERTYPE_IPV6: parse_ipv6;
            ETHERTYPE_ARP: parse_arp;
            default: accept;
        }
    }

    state parse_vlan {
        pk.extract(hdr.vlan_tag.next);
        transition select(hdr.vlan_tag.last.ether_type) {
            ETHERTYPE_VLAN1: parse_vlan;
            ETHERTYPE_VLAN2: parse_vlan;
            ETHERTYPE_VLAN3: parse_vlan;
            ETHERTYPE_VLAN4: parse_vlan;
            ETHERTYPE_IPV4: parse_ipv4;
            ETHERTYPE_IPV6: parse_ipv6;
            default: accept;
        }
    }

    state parse_ipv4 {
        pk.extract(hdr.ipv4_base);
        transition select(hdr.ipv4_base.frag_offset ++ hdr.ipv4_base.protocol) {
            13w0 ++ IP_PROTOCOLS_ICMP: parse_icmp;
            13w0 ++ IP_PROTOCOLS_TCP: parse_tcp;
            13w0 ++ IP_PROTOCOLS_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_ipv6 {
        pk.extract(hdr.ipv6_base);
        transition select(hdr.ipv6_base.next_header) {
            IP_PROTOCOLS_ICMPv6: parse_icmp;
            IP_PROTOCOLS_TCP: parse_tcp;
            IP_PROTOCOLS_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_tcp {
        pk.extract(hdr.tcp);
        local_metadata.l4_src_port = hdr.tcp.src_port;
        local_metadata.l4_dst_port = hdr.tcp.dst_port;
        transition accept;
    }

    state parse_udp {
        pk.extract(hdr.udp);
        local_metadata.l4_src_port = hdr.udp.src_port;
        local_metadata.l4_dst_port = hdr.udp.dst_port;
        transition accept;
    }

    state parse_icmp {
        pk.extract(hdr.icmp_header);
        transition accept;
    }

    state parse_arp {
        pk.extract(hdr.arp);
        transition accept;
    }
}

control pkt_deparser(packet_out b, in parsed_packet_t hdr) {
    apply {
        b.emit(hdr.packet_in);
        b.emit(hdr.ethernet);
        b.emit(hdr.vlan_tag);
        b.emit(hdr.ipv4_base);
        b.emit(hdr.ipv6_base);
        b.emit(hdr.arp);
        b.emit(hdr.icmp_header);
        b.emit(hdr.tcp);
        b.emit(hdr.udp);
    }
}

control verify_ipv4_checksum(inout parsed_packet_t hdr,
                             inout local_metadata_t local_metadata) {
    apply {
        verify_checksum(hdr.ipv4_base.isValid(),
            {
                hdr.ipv4_base.version,
                hdr.ipv4_base.ihl,
                hdr.ipv4_base.diffserv,
                hdr.ipv4_base.total_len,
                hdr.ipv4_base.identification,
                hdr.ipv4_base.flags,
                hdr.ipv4_base.frag_offset,
                hdr.ipv4_base.ttl,
                hdr.ipv4_base.protocol,
                hdr.ipv4_base.src_addr,
                hdr.ipv4_base.dst_addr
            },
            hdr.ipv4_base.hdr_checksum,
            HashAlgorithm.csum16);
    }
}

control compute_ipv4_checksum(inout parsed_packet_t hdr,
                              inout local_metadata_t local_metadata) {
    apply {
        update_checksum(hdr.ipv4_base.isValid(),
            {
                hdr.ipv4_base.version,
                hdr.ipv4_base.ihl,
                hdr.ipv4_base.diffserv,
                hdr.ipv4_base.total_len,
                hdr.ipv4_base.identification,
                hdr.ipv4_base.flags,
                hdr.ipv4_base.frag_offset,
                hdr.ipv4_base.ttl,
                hdr.ipv4_base.protocol,
                hdr.ipv4_base.src_addr,
                hdr.ipv4_base.dst_addr
            },
            hdr.ipv4_base.hdr_checksum,
            HashAlgorithm.csum16);
    }
}

control punt(inout parsed_packet_t hdr,
             inout local_metadata_t local_metadata,
             inout standard_metadata_t standard_metadata) {

    direct_meter<bit<2>>(MeterType.bytes) ingress_port_meter;
    direct_counter(CounterType.packets) punt_packet_counter;

    action set_queue_and_clone_to_cpu(bit<5> queue_id) {
        local_metadata.cpu_cos_queue_id = queue_id;
        local_metadata.egress_spec_at_punt_match = standard_metadata.egress_spec;
        clone3(CloneType.I2E, CPU_MIRROR_SESSION_ID, { standard_metadata.ingress_port });
        ingress_port_meter.read(local_metadata.color);
        punt_packet_counter.count();
    }

    action set_queue_and_send_to_cpu(bit<5> queue_id) {
        local_metadata.cpu_cos_queue_id = queue_id;
        local_metadata.egress_spec_at_punt_match = standard_metadata.egress_spec;
        standard_metadata.egress_spec = CPU_PORT;
        ingress_port_meter.read(local_metadata.color);
        punt_packet_counter.count();
    }

    action set_egress_port(PortNum port) {
        local_metadata.egress_spec_at_punt_match = standard_metadata.egress_spec;
        standard_metadata.egress_spec = port;
        ingress_port_meter.read(local_metadata.color);
        punt_packet_counter.count();
    }

    @switchstack("pipeline_stage: INGRESS_ACL")
    table punt_table {
        key = {
            standard_metadata.ingress_port: ternary;
            standard_metadata.egress_spec : ternary;
            hdr.ethernet.ether_type       : ternary;
            hdr.ipv4_base.diffserv        : ternary;
            // hdr.ipv6_base.traffic_class: ternary;
            hdr.ipv4_base.ttl             : ternary;
            // hdr.ipv6_base.hop_limit    : ternary;
            hdr.ipv4_base.src_addr        : ternary;
            hdr.ipv4_base.dst_addr        : ternary;
            // hdr.ipv6_base.src_addr     : ternary;
            // hdr.ipv6_base.dst_addr     : ternary;
            hdr.ipv4_base.protocol        : ternary;
            // hdr.ipv6_base.next_header  : ternary;
            // hdr.arp.target_proto_addr  : ternary;
            local_metadata.icmp_code      : ternary;
            hdr.vlan_tag[0].vid           : ternary;
            hdr.vlan_tag[0].pcp           : ternary;
            local_metadata.class_id       : ternary;
            local_metadata.vrf_id         : ternary;
        }
        actions = {
            set_queue_and_clone_to_cpu;
            set_queue_and_send_to_cpu;
            set_egress_port;
            NoAction;
        }
        const default_action = NoAction();
        meters = ingress_port_meter;
        counters = punt_packet_counter;
        size = 25;
    }

    apply {
        punt_table.apply();
    }
}

control l3_fwd(inout parsed_packet_t hdr,
               inout local_metadata_t local_metadata,
               inout standard_metadata_t standard_metadata) {

    action drop() {
        mark_to_drop(standard_metadata);
    }

    action set_nexthop(PortNum port, EthernetAddress smac, EthernetAddress dmac,
                       bit<12> dst_vlan) {
        standard_metadata.egress_spec = port;
        local_metadata.dst_vlan = dst_vlan;
        hdr.ethernet.src_addr = smac;
        hdr.ethernet.dst_addr = dmac;
        hdr.ipv4_base.ttl = hdr.ipv4_base.ttl - 1;
    }

    @max_group_size(8)
    action_selector(HashAlgorithm.crc16, 32w1024, 32w14) wcmp_action_profile;

    @switchstack("pipeline_stage: L3_LPM")
    table l3_fwd_table {
        key = {
            local_metadata.vrf_id     : exact;
            hdr.ipv4_base.dst_addr    : lpm;
            hdr.ipv4_base.src_addr    : selector;
            hdr.ipv4_base.protocol    : selector;
            local_metadata.l4_src_port: selector;
            local_metadata.l4_dst_port: selector;
        }
        actions = {
            set_nexthop;
            NoAction;
            drop;
        }
        const default_action = NoAction();
        implementation = wcmp_action_profile;
    }

    apply {
        l3_fwd_table.apply();
    }
}

control l2_fwd(inout parsed_packet_t hdr,
               inout local_metadata_t local_metadata,
               inout standard_metadata_t standard_metadata) {

    action set_egress_port(PortNum port) {
        standard_metadata.egress_spec = port;
    }

    @switchstack("pipeline_stage: L2")
    table l2_unicast_table {
        key = {
            hdr.ethernet.dst_addr: exact;
        }
        actions = {
            set_egress_port;
            NoAction;
        }
        const default_action = NoAction();
    }

    apply {
        l2_unicast_table.apply();
    }
}

control ingress(inout parsed_packet_t hdr,
                inout local_metadata_t local_metadata,
                inout standard_metadata_t standard_metadata) {

    action set_l3_admit() {
        local_metadata.l3_admit = 1w1;
    }

    @switchstack("pipeline_stage: L2")
    table my_station_table {
        key = {
            hdr.ethernet.dst_addr: ternary;
        }
        actions = {
            set_l3_admit;
            NoAction;
        }
        const default_action = NoAction();
    }

    apply {
        if (hdr.packet_out.isValid()) {
            standard_metadata.egress_spec = hdr.packet_out.egress_physical_port;
            hdr.packet_out.setInvalid();
        }

        if (standard_metadata.egress_spec == 0) {
            // Egress port not valid or not a packet out.
            my_station_table.apply();
            if (local_metadata.l3_admit == 1w1) {
                l3_fwd.apply(hdr, local_metadata, standard_metadata);
            } else {
                l2_fwd.apply(hdr, local_metadata, standard_metadata);
            }
        } else {
            exit;
        }

        punt.apply(hdr, local_metadata, standard_metadata);
    }
}

control egress(inout parsed_packet_t hdr,
               inout local_metadata_t local_metadata,
               inout standard_metadata_t standard_metadata) {
    apply {
        if (standard_metadata.egress_port == CPU_PORT) {
            hdr.packet_in.setValid();
            hdr.packet_in.ingress_physical_port = standard_metadata.ingress_port;
            hdr.packet_in.target_egress_port = local_metadata.egress_spec_at_punt_match;
            exit;
        }
    }
}

V1Switch(
    pkt_parser(),
    verify_ipv4_checksum(),
    ingress(),
    egress(),
    compute_ipv4_checksum(),
    pkt_deparser())
main;
