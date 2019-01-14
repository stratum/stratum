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

// This is a .p4 file that FieldDecoderTest uses to verify header fields
// with @controller_header and @switchstack("field_type: ...") annotations.

#include <core.p4>
#include <v1model.p4>

#include "simple_headers.p4"

// Controller header definitions for test use, adapted from
// orion/p4/spec/headers.p4
@controller_header("packet_in")
header packet_in_header_t {
  @switchstack("field_type: P4_FIELD_TYPE_INGRESS_PORT")
  bit<9> ingress_physical_port;
  @switchstack("field_type: P4_FIELD_TYPE_INGRESS_TRUNK")
  bit<32> ingress_logical_port;
  @switchstack("field_type: P4_FIELD_TYPE_EGRESS_PORT")
  bit<9> target_egress_port;
}

@controller_header("packet_out")
header packet_out_header_t {
  @switchstack("field_type: P4_FIELD_TYPE_EGRESS_PORT")
  bit<9> egress_physical_port;
  bit<1> submit_to_ingress;  // Intentionally not annotated.
}

struct parsed_packet_t {
  ethernet_t ethernet;
  packet_in_header_t packet_in;
  packet_out_header_t packet_out;
}

parser parser_in(packet_in packet, out parsed_packet_t hdr,
                 inout test_metadata_t meta,
                 inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

// Stubs from control_stubs.p4 can't be used since they don't refer to
// parsed_packet_t as defined above, so they are cloned here with the
// appropriate references.
control egress_stub(inout parsed_packet_t hdr, inout test_metadata_t meta,
                    inout standard_metadata_t standard_metadata) {
  apply {}
}

control ingress_stub(inout parsed_packet_t hdr, inout test_metadata_t meta,
                     inout standard_metadata_t standard_metadata) {
  apply {}
}

control deparser_stub(packet_out packet, in parsed_packet_t hdr) {
  apply {}
}

control verify_checksum_stub(inout parsed_packet_t hdr,
                             inout test_metadata_t meta) {
  apply {}
}

control compute_checksum_stub(inout parsed_packet_t hdr,
                              inout test_metadata_t meta) {
  apply {}
}


V1Switch(parser_in(), verify_checksum_stub(), ingress_stub(), egress_stub(),
         compute_checksum_stub(), deparser_stub()) main;
