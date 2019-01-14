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

// This is a .p4 file that HeaderPathInspectorTest uses to verify more complex
// headers, such as repeated types and unions.

#include <core.p4>
#include <v1model.p4>

#include "simple_headers.p4"

// These test-dependent header types prevent use of simple_headers.p4.
header proto1_t {
  bit<32> proto1_field;
}

header proto2_t {
  bit<16> proto2_field;
}

header_union proto_union_t {
  proto1_t proto1;
  proto2_t proto2;
}

// The ethernet_t type is used repeatedly in the same header.  The proto1_t
// and proto2_t types surround a union in which they also appear
// to assure that HeaderPathInspector follows all possible paths.
struct parsed_packet_t {
  ethernet_t ether_outer;
  ethernet_t ether_inner;
  proto1_t proto_before_union;
  proto_union_t proto_union;
  proto2_t proto_after_union;
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
  apply {
    hdr.ether_outer.etherType = 0x800;  // Force a header reference.
    meta.color = 1;  // Force a metadata reference.
  }
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
