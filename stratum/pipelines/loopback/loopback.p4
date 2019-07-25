/*
 * Copyright 2018-present Open Networking Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


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

control verify_checksum(inout headers_t hdr,
                inout pgm_metadata_t local_metadata) {

    apply{ }

}

//------------------------------------------------------------------------------
// CHECKSUM UPDATE 
//------------------------------------------------------------------------------

control compute_checksum(inout headers_t hdr,
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
         verify_checksum(),
         ingress(),
         egress(),
         compute_checksum(),
         deparser()) main;

