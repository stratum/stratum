// This .p4 file contains a set of control stubs for test P4 specs to use when
// they don't care about the control functionality.  For example, a test that
// only needs to verify P4 ingress operation can use stubs for the other
// controls.

#include <core.p4>
#include <v1model.p4>

#include "platforms/networking/hercules/p4c_backend/switch/testdata/simple_headers.p4"

#ifndef PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_CONTROL_STUBS_P4_
#define PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_CONTROL_STUBS_P4_

control egress_stub(inout headers hdr, inout test_metadata_t meta,
                    inout standard_metadata_t standard_metadata) {
  apply {}
}

control ingress_stub(inout headers hdr, inout test_metadata_t meta,
                     inout standard_metadata_t standard_metadata) {
  apply {}
}

control deparser_stub(packet_out packet, in headers hdr) {
  apply {}
}

control verify_checksum_stub(inout headers hdr, inout test_metadata_t meta) {
  apply {}
}

control compute_checksum_stub(inout headers hdr, inout test_metadata_t meta) {
  apply {}
}

#endif  // PLATFORMS_NETWORKING_HERCULES_P4C_BACKEND_SWITCH_TESTDATA_CONTROL_STUBS_P4_
