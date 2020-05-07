// Copyright 2018-present Open Networking Foundation
// SPDX-License-Identifier: Apache-2.0

#include <core.p4>
#include <v1model.p4>

//------------------------------------------------------------------------------
// HEADERS
//------------------------------------------------------------------------------

header ethernet_t {
    bit<48> dst_addr;
    bit<48> src_addr;
    bit<16> ether_type;
}

struct headers_t {
	ethernet_t	ethernet;
}

//------------------------------------------------------------------------------
// PROGRAM METADATA
//------------------------------------------------------------------------------

struct pgm_metadata_t {
    bit<48> ingress_dst_addr;
    bit<48> ingress_src_addr;
}

//------------------------------------------------------------------------------
// PARSER
//------------------------------------------------------------------------------

parser parser_impl( packet_in packet,
                    out headers_t hdr,
                    inout pgm_metadata_t local_metadata,
                    inout standard_metadata_t standard_metadata){

    state start {
        transition parse_ethernet;
    }

    state parse_ethernet {
        packet.extract(hdr.ethernet);
        transition accept;
    }
}

//------------------------------------------------------------------------------
// INGRESS PIPELINE
//------------------------------------------------------------------------------

control ingress(inout headers_t hdr,
                inout pgm_metadata_t local_metadata,
                inout standard_metadata_t standard_metadata) {

    action redirect(){
    	standard_metadata.egress_spec = standard_metadata.ingress_port;
    }

    table t_redirect{

    	actions = {
	    	redirect;
    	}

    	const default_action = redirect;
    }

    action input_addr_read(){
        local_metadata.ingress_dst_addr = hdr.ethernet.dst_addr;
        local_metadata.ingress_src_addr = hdr.ethernet.src_addr;
    }

    table t_input_addr_read{
        actions = {
            input_addr_read;
        }

        const default_action = input_addr_read;
    }

    apply {
        // Save ethernet addresses to metadata.
        t_input_addr_read.apply();

        // Set output port to ingress port value.
    	t_redirect.apply();
    }
}

//------------------------------------------------------------------------------
// EGRESS PIPELINE
//------------------------------------------------------------------------------

control egress(inout headers_t hdr,
               inout pgm_metadata_t local_metadata,
               inout standard_metadata_t standard_metadata) {

    action output_addr_write(){
        hdr.ethernet.dst_addr = local_metadata.ingress_src_addr;
        hdr.ethernet.src_addr = local_metadata.ingress_dst_addr;
    }

    table t_output_addr_write{
        actions = {
            output_addr_write;
        }

        const default_action = output_addr_write;
    }

    apply{
        // Write ethernet addresses to packet.
        t_output_addr_write.apply();
    }
}

//------------------------------------------------------------------------------
// CHECKSUM VERIFICATION
//------------------------------------------------------------------------------

control verify_cksum(inout headers_t hdr,
                inout pgm_metadata_t local_metadata) {

    apply{ }

}

//------------------------------------------------------------------------------
// CHECKSUM UPDATE
//------------------------------------------------------------------------------

control compute_cksum(inout headers_t hdr,
                inout pgm_metadata_t local_metadata) {

    apply{ }
}

//------------------------------------------------------------------------------
// DEPARSER
//------------------------------------------------------------------------------

control deparser(packet_out packet, in headers_t hdr) {
    apply {
        packet.emit(hdr.ethernet);
    }
}

//------------------------------------------------------------------------------
// SWITCH INSTANTIATION
//------------------------------------------------------------------------------

V1Switch(parser_impl(),
         verify_cksum(),
         ingress(),
         egress(),
         compute_cksum(),
         deparser()) main;

