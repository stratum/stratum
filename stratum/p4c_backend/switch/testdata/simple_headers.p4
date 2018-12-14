// This is a .p4 file with some simple header types that can be included
// by other test P4 specs.

#include <core.p4>
#include <v1model.p4>

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_SIMPLE_HEADERS_P4_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_SIMPLE_HEADERS_P4_

enum meter_color_t { COLOR_GREEN, COLOR_RED, COLOR_YELLOW }

struct test_metadata_t {
  bit<32> color;
  meter_color_t enum_color;
  bit<32> other_metadata;
  bit<16> smaller_metadata;
}

header ethernet_t {
  bit<48> dstAddr;
  bit<48> srcAddr;
  bit<16> etherType;
}

struct headers {
  ethernet_t ethernet;
  ethernet_t ethernet2;
  ethernet_t[2] ethernet_stack;
}

// This parser stub can be used by tests that don't depend on real header
// parsing.
parser parser_stub(packet_in packet, out headers hdr,
                   inout test_metadata_t meta,
                   inout standard_metadata_t standard_metadata) {
  state start {
    transition accept;
  }
}

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_SIMPLE_HEADERS_P4_
